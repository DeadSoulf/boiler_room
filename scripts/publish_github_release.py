#!/usr/bin/env python3
"""Publish a hardware-accepted release or an explicitly pending OTA candidate.

Uses the existing Git credential helper only for github.com/DeadSoulf/boiler_room.
Credentials stay in memory and are never printed or written. Default: dry-run.
Never uploads backups or files outside the build's allowlisted firmware package.
"""
from pathlib import Path, PurePosixPath
import argparse,hashlib,io,json,os,re,subprocess,sys,urllib.request,urllib.error,urllib.parse,zipfile
ROOT=Path(__file__).resolve().parents[1]
REPO='DeadSoulf/boiler_room'
sys.path.insert(0,str(ROOT/'ci'))
from ota_manifest import MANIFEST_NAME,validate_assets
CANDIDATE_MARKER='<!-- boiler-panel-release:ota-candidate-v1 -->'
HARDWARE_MARKER='<!-- boiler-panel-release:hardware-accepted-v1 -->'
BROWSER_TEST='tests/equipment_web_browser.py'
WEB_HEADER='src/panel_web_page.h'
def require(ok,message):
 if not ok:raise ValueError(message)
def exact_keys(value,keys,label):
 require(type(value) is dict and set(value)==set(keys),f'{label}: missing or unexpected fields')
def strict_json(raw,label):
 require(len(raw)<=1024*1024 and not raw.startswith(b'\xef\xbb\xbf'),f'{label}: invalid JSON size or BOM')
 def unique(pairs):
  value=dict(pairs);require(len(value)==len(pairs),f'{label}: duplicate JSON field');return value
 def invalid_constant(value):raise ValueError(f'{label}: non-JSON numeric constant')
 value=json.loads(raw.decode('utf-8'),object_pairs_hook=unique,parse_constant=invalid_constant)
 require(type(value) is dict,f'{label}: an object is required');return value
def artifact_bindings(payloads):
 return {name:{'size':len(data),'sha256':hashlib.sha256(data).hexdigest()} for name,data,_ in payloads if name!='SHA256SUMS.txt'}
def evidence_bytes(root,entry,label):
 exact_keys(entry,('path','sha256'),label)
 path=entry['path'];digest=entry['sha256']
 require(type(path) is str and '\\' not in path and ':' not in path,f'{label}: invalid evidence path')
 relative=PurePosixPath(path)
 require(not relative.is_absolute() and relative.as_posix()==path and len(relative.parts)>1
         and relative.parts[0]=='artifacts' and '..' not in relative.parts,f'{label}: evidence must be under artifacts/')
 require(type(digest) is str and re.fullmatch(r'[0-9a-f]{64}',digest),f'{label}: invalid evidence SHA-256')
 file=(root/path).resolve(strict=True)
 require(file.is_relative_to((root/'artifacts').resolve()) and file.is_file(),f'{label}: evidence escapes artifacts/')
 require(0<file.stat().st_size<=16*1024*1024,f'{label}: evidence is empty or too large')
 raw=file.read_bytes();require(hashlib.sha256(raw).hexdigest()==digest,f'{label}: evidence SHA-256 mismatch');return raw
def validate_candidate(root,proof,tag,source,payloads,source_file=None):
 exact_keys(proof,('schema','kind','project','version','source_commit','hardware_acceptance','assets','checks'),'OTA build result')
 require(type(proof['schema']) is int and proof['schema']==1 and proof['kind']=='ota-build-candidate','Unsupported OTA build result schema/kind')
 require(proof['project']==REPO and proof['version']==tag and proof['source_commit']==source,'OTA build result identifies a different source/version')
 require(proof['hardware_acceptance']=='pending','A build candidate cannot claim hardware acceptance')
 expected=artifact_bindings(payloads)
 exact_keys(proof['assets'],expected,'OTA build assets')
 for name,actual in proof['assets'].items():
  exact_keys(actual,('size','sha256'),'OTA build asset')
  require(type(actual['size']) is int and actual==expected[name],f'OTA build result asset mismatch: {name}')
 exact_keys(proof['checks'],('final_build','browser_tests'),'OTA build checks')
 build=proof['checks']['final_build'];browser=proof['checks']['browser_tests']
 exact_keys(build,('source_commit','firmware_sha256','target','exit_code','log'),'Final build check')
 require(build['source_commit']==source and build['target']=='jc4880p4' and type(build['exit_code']) is int
         and build['exit_code']==0 and build['firmware_sha256']==expected[f'BoilerPanel-{tag}-firmware.bin']['sha256'],
         'Final build did not pass for this exact source/firmware')
 log=evidence_bytes(root,build['log'],'Final build log')
 require(b'[SUCCESS]' in log and b'[FAILED]' not in log,'Final build log does not confirm a successful final run')
 exact_keys(browser,('source_commit','test_script','report'),'Browser check')
 require(browser['source_commit']==source and browser['test_script']==BROWSER_TEST,'Browser check identifies a different source/test')
 report=strict_json(evidence_bytes(root,browser['report'],'Browser report'),'Browser report')
 require(report.get('result')=='PASS' and report.get('test_script')==BROWSER_TEST and report.get('page_errors')==[]
         and report.get('mutation_requests')==[] and report.get('physical_panel_contacted') is False,
         'Browser report is not a successful isolated final test')
 require(report.get('widths')==[320,390,768,1280] and all(type(n) is int for n in report['widths'])
         and type(report.get('status_polls')) is int and report['status_polls']>0
         and type(report.get('coverage')) is list and len(report['coverage'])>0
         and all(type(item) is str and item for item in report['coverage']),'Browser report lacks the agreed regression coverage')
 if source_file is None:
  source_file=lambda path:subprocess.check_output(['git','show',f'{source}:{path}'],cwd=root)
 for path,key in ((WEB_HEADER,'source_header_sha256'),(BROWSER_TEST,'test_script_sha256')):
  checked=(root/path).read_bytes();committed=source_file(path)
  require(checked==committed and report.get(key)==hashlib.sha256(committed).hexdigest(),
          f'Browser evidence is not bound to the final committed file: {path}')
def validate_hardware(proof,tag,source,payloads,notes_only=False):
 expected=artifact_bindings(payloads)
 require(proof.get('version')==tag and proof.get('source_commit')==source
         and proof.get('firmware_sha256')==expected[f'BoilerPanel-{tag}-firmware.bin']['sha256'],
         'Hardware result identifies a different source/firmware')
 require(proof.get('readback_and_metadata_verified') is True and proof.get('http_ok') is True,
         'Hardware readback/metadata and HTTP acceptance are required')
 require('kind' not in proof and proof.get('hardware_acceptance','accepted')=='accepted',
         'A pending build result cannot serve as hardware acceptance')
 for key,name in (('zip_sha256',f'BoilerPanel-{tag}-COM3.zip'),('ota_manifest_sha256',MANIFEST_NAME)):
  if notes_only or key in proof:require(proof.get(key)==expected[name]['sha256'],f'Hardware result mismatch: {key}')
 if 'network_ota_install_verified' in proof:
  require(type(proof['network_ota_install_verified']) is bool,'Network OTA result must be a boolean')
 if notes_only:require(proof.get('network_ota_install_verified') is True,'Notes-only OTA acceptance requires a completed hardware OTA')
def release_notes(notes,mode,tag,source,proof):
 require(CANDIDATE_MARKER not in notes and HARDWARE_MARKER not in notes,'Release notes cannot override the generated acceptance status')
 if mode=='candidate':
  status=(CANDIDATE_MARKER+'\n## OTA-кандидат: аппаратная приёмка ожидается\n\n'
          '**Установка этой версии через OTA на физическую панель ещё не подтверждена.** '
          'Финальная target-сборка и браузерный регрессионный тест пройдены для указанных исходников и файлов. '
          'Выпуск опубликован как stable/latest, чтобы панель могла предложить его для первой аппаратной OTA-приёмки. '
          'Установка требует подтверждения пользователя; публикация не означает успешную установку.\n')
 else:
  path='Аппаратная установка через GitHub OTA подтверждена.' if proof.get('network_ota_install_verified') is True else 'Аппаратная установка подтверждена; сетевой путь OTA этим результатом не заявлен.'
  status=HARDWARE_MARKER+'\n## Аппаратная приёмка\n\n'+path+' Проверены образ, метаданные запуска и HTTP.\n'
 return status+f'\nВерсия: `{tag}`. Исходный commit: `{source}`.\n\n## Изменения и сведения о выпуске\n\n'+notes.rstrip()+'\n'
def checked_remote_assets(release,payloads,complete=False):
 expected={name:(data,mime) for name,data,mime in payloads}
 assets=release.get('assets',[])
 require(type(assets) is list and all(type(a) is dict for a in assets),'Invalid GitHub asset metadata')
 names=[a.get('name') for a in assets]
 require(all(type(name) is str for name in names) and len(names)==len(set(names)),'Duplicate or invalid GitHub asset names')
 require(set(names)<=set(expected),'Release contains assets outside the fixed allowlist')
 if complete:require(set(names)==set(expected),'Release does not contain the complete immutable asset set')
 actual={a['name']:a for a in assets}
 for name,asset in actual.items():
  data,_=expected[name]
  require(type(asset.get('size')) is int and asset['size']==len(data)
          and asset.get('digest')=='sha256:'+hashlib.sha256(data).hexdigest(),f'Existing asset differs; refusing to overwrite: {name}')
 return actual
def publish_release(api,tag,source,notes,payloads,mode,notes_only=False):
 require(mode in ('candidate','hardware'),'Unknown release acceptance mode')
 release=api('GET','/releases/tags/'+tag,allow404=True)
 if release is not None:
  require(release.get('tag_name')==tag,'Existing release identifies another tag')
  checked_remote_assets(release,payloads,complete=notes_only or not release.get('draft',True))
 if notes_only:
  require(release is not None and release.get('draft') is False and release.get('prerelease') is False,
          'Notes-only acceptance requires an existing public stable release')
  require(mode=='hardware','Notes-only acceptance requires a hardware result')
  # No create, upload, replace, draft, prerelease or latest operation in this path.
  if release.get('body')!=notes:api('PATCH','/releases/'+str(release['id']),{'body':notes})
 else:
  if mode=='candidate' and release is not None:
   body=release.get('body') or ''
   require(not body.startswith(HARDWARE_MARKER) and (release.get('draft') is True or body.startswith(CANDIDATE_MARKER)),
           'An accepted or unclassified public release cannot be changed into a pending candidate')
  if release is None:
   release=api('POST','/releases',{'tag_name':tag,'target_commitish':source,'name':'Boiler Panel '+tag,
                                'body':notes,'draft':True,'prerelease':False})
   require(release.get('tag_name')==tag and release.get('draft') is True,'GitHub did not create the expected draft')
  assets=checked_remote_assets(release,payloads)
  for name,data,mime in payloads:
   if name in assets:continue
   asset=api('POST',f'/releases/{release["id"]}/assets?name='+urllib.parse.quote(name),data,mime,upload=True)
   checked_remote_assets({'assets':[asset]},[(name,data,mime)],complete=True)
  checked=api('GET','/releases/'+str(release['id']))
  require(checked.get('tag_name')==tag,'Release tag changed during publication')
  checked_remote_assets(checked,payloads,complete=True)
  # A retry of an already public release updates only its notes, not latest.
  if checked.get('draft') is True or checked.get('prerelease') is True:
   api('PATCH','/releases/'+str(release['id']),{'body':notes,'draft':False,'prerelease':False,'make_latest':'true'})
  elif checked.get('body')!=notes:api('PATCH','/releases/'+str(release['id']),{'body':notes})
 final=api('GET','/releases/'+str(release['id']))
 require(final.get('tag_name')==tag and final.get('draft') is False and final.get('prerelease') is False
         and final.get('body')==notes,'GitHub did not confirm the requested stable release notes')
 checked_remote_assets(final,payloads,complete=True)
 return final
def argument_parser():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--source',required=True);parser.add_argument('--package',type=Path,required=True)
 result=parser.add_mutually_exclusive_group(required=True)
 result.add_argument('--hardware-result',type=Path);result.add_argument('--ota-build-result',type=Path)
 parser.add_argument('--notes',type=Path,required=True)
 parser.add_argument('--notes-only',action='store_true',help='Accept completed OTA by changing only notes of matching existing assets')
 parser.add_argument('--publish',action='store_true');return parser
def request_body(data,upload=False):
 if data is None:return None
 if upload:
  if not isinstance(data,bytes):raise TypeError('Release assets must be exact bytes')
  return data
 return json.dumps(data).encode('utf-8')
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT,text=True).strip()
def main():
 parser=argument_parser();args=parser.parse_args()
 if args.notes_only and args.hardware_result is None:parser.error('--notes-only requires --hardware-result')
 tag=(ROOT/'VERSION').read_text(encoding='ascii').strip()
 require(re.fullmatch(r'v\d+\.\d+\.\d+',tag),'Invalid release version')
 require(re.fullmatch(r'[0-9a-f]{40}',args.source),'A full immutable source commit is required')
 subprocess.run([sys.executable,str(ROOT/'scripts/check_release_version.py')],check=True,cwd=ROOT)
 package=args.package.resolve();require(package.parent==(ROOT/'artifacts').resolve(),'Package must be under artifacts/')
 require(package.name==f'BoilerPanel-{tag}-COM3.zip','Package name does not match the version')
 package_bytes=package.read_bytes()
 with zipfile.ZipFile(io.BytesIO(package_bytes)) as z:
  require(z.testzip() is None,'Package ZIP checksum failed')
  prefix=f'BoilerPanel-{tag}-COM3/'
  entries=z.namelist()
  require(len(entries)==len(set(entries)) and all(n.startswith(prefix) and '..' not in n and '\\' not in n for n in entries),
          'Package contains duplicate or invalid paths')
  files={n[len(prefix):]:z.read(n) for n in entries}
  allowed={'firmware.bin','bootloader.bin','partitions.bin','binary-verification.json','scripts/flash_panel.py','scripts/flash_COM3.cmd','docs/flash-panel.md','SOURCE.txt','START_HERE.txt','SHA256SUMS.json'}
  require(allowed<=set(files) and all(n in allowed or n.startswith('licenses/') for n in files),
          'Unexpected or missing files: backup/credentials must never be uploaded')
  manifest=strict_json(files['SHA256SUMS.json'],'Package hashes')
  require(set(manifest)==set(files)-{'SHA256SUMS.json'},'Every package payload must be hashed')
  require(all(hashlib.sha256(files[n]).hexdigest()==h for n,h in manifest.items()),'Package payload hash mismatch')
  source_lines=re.findall(r'^GitHub commit: ([0-9a-f]{40})$',files['SOURCE.txt'].decode('utf-8'),re.M)
  require(source_lines==[args.source],'Package identifies a different or ambiguous source commit')
 ota_bytes=(ROOT/'artifacts'/MANIFEST_NAME).read_bytes()
 ota=validate_assets(ROOT,tag,args.source,files['firmware.bin'],files['bootloader.bin'],files['partitions.bin'])
 require((ROOT/'artifacts'/MANIFEST_NAME).read_bytes()==ota_bytes,'OTA manifest changed during validation')
 firmware_name=ota['firmware']['name']
 payloads=[(package.name,package_bytes,'application/zip'),
           (firmware_name,files['firmware.bin'],'application/octet-stream'),
           (MANIFEST_NAME,ota_bytes,'application/json')]
 sums=''.join(f'{hashlib.sha256(data).hexdigest()}  {name}\n' for name,data,_ in payloads).encode('ascii')
 payloads.append(('SHA256SUMS.txt',sums,'text/plain'))
 mode='hardware' if args.hardware_result is not None else 'candidate'
 result_path=args.hardware_result if mode=='hardware' else args.ota_build_result
 proof=strict_json(result_path.read_bytes(),'Release result')
 if mode=='hardware':validate_hardware(proof,tag,args.source,payloads,notes_only=args.notes_only)
 else:
  validate_candidate(ROOT,proof,tag,args.source,payloads)
  # An attestation for the final source cannot cover modified/untracked build inputs.
  inputs=('src','ci','VERSION',BROWSER_TEST)
  subprocess.run(['git','diff','--exit-code',args.source,'--',*inputs],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
  require(not git('ls-files','--others','--exclude-standard','--',*inputs),'Untracked files would change the candidate build/test inputs')
 require(git('rev-parse',f'refs/tags/{tag}')==args.source,'Tag must identify the exact build source')
 subprocess.run(['git','merge-base','--is-ancestor',args.source,'origin/main'],cwd=ROOT,check=True)
 remote=git('ls-remote','origin',f'refs/tags/{tag}').split()
 require(remote and remote[0]==args.source,'Remote tag must identify the exact build source')
 notes=release_notes(args.notes.read_text(encoding='utf-8'),mode,tag,args.source,proof)
 print(json.dumps({'tag':tag,'source':args.source,'package':package.name,'sha256':hashlib.sha256(package_bytes).hexdigest(),
                   'acceptance':'pending' if mode=='candidate' else 'hardware_accepted','notes_only':args.notes_only,'publish':args.publish}))
 if not args.publish:return
 env=os.environ.copy();env.update(GIT_TERMINAL_PROMPT='0',GCM_INTERACTIVE='never',GIT_TRACE='0',GIT_TRACE_CURL='0',GIT_CURL_VERBOSE='0')
 credential=subprocess.run(['git','credential','fill'],cwd=ROOT,input=f'protocol=https\nhost=github.com\npath={REPO}.git\n\n',capture_output=True,text=True,env=env)
 if credential.returncode:raise RuntimeError('Existing GitHub credentials unavailable; no release was published')
 values=dict(line.split('=',1) for line in credential.stdout.splitlines() if '=' in line)
 token=values.get('password')
 if not token:raise RuntimeError('Existing GitHub token unavailable')
 def api(method,path,data=None,mime='application/json',upload=False,allow404=False):
  base='https://uploads.github.com/repos/' if upload else 'https://api.github.com/repos/'
  # application/json is also an asset MIME: its exact bytes must not be re-encoded.
  payload=request_body(data,upload=upload)
  request=urllib.request.Request(base+REPO+path,data=payload,method=method,headers={'Authorization':'Bearer '+token,'Accept':'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28','User-Agent':'BoilerPanel-release','Content-Type':mime})
  try:
   with urllib.request.urlopen(request,timeout=90) as response:return json.loads(response.read() or b'{}')
  except urllib.error.HTTPError as e:
   if allow404 and e.code==404:return None
   raise RuntimeError(f'GitHub {method} failed with HTTP {e.code}; credentials are not logged') from None
 final=publish_release(api,tag,args.source,notes,payloads,mode,notes_only=args.notes_only)
 print(json.dumps({'release':final['html_url'],'tag':final['tag_name'],'assets':[{'name':a['name'],'size':a['size'],'digest':a.get('digest')} for a in final['assets']]}))
 values.clear();token=None
if __name__=='__main__':main()
