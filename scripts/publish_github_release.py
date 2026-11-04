#!/usr/bin/env python3
"""Publish a checked firmware package after a successful hardware installation.

Uses the existing Git credential helper only for github.com/DeadSoulf/boiler_room.
Credentials stay in memory and are never printed or written. Default: dry-run.
Never uploads backups or files outside the build's allowlisted firmware package.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,sys,urllib.request,urllib.error,urllib.parse,zipfile
ROOT=Path(__file__).resolve().parents[1]
REPO='DeadSoulf/boiler_room'
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT,text=True).strip()
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--source',required=True);parser.add_argument('--package',type=Path,required=True)
 parser.add_argument('--hardware-result',type=Path,required=True);parser.add_argument('--notes',type=Path,required=True)
 parser.add_argument('--publish',action='store_true');args=parser.parse_args()
 tag=(ROOT/'VERSION').read_text(encoding='ascii').strip()
 assert re.fullmatch(r'v\d+\.\d+\.\d+',tag)
 assert re.fullmatch(r'[0-9a-f]{40}',args.source)
 subprocess.run([sys.executable,str(ROOT/'scripts/check_release_version.py')],check=True,cwd=ROOT)
 package=args.package.resolve();assert package.parent== (ROOT/'artifacts').resolve()
 assert package.name==f'BoilerPanel-{tag}-COM3.zip'
 with zipfile.ZipFile(package) as z:
  assert z.testzip() is None
  prefix=f'BoilerPanel-{tag}-COM3/'
  entries=z.namelist();assert all(n.startswith(prefix) and '..' not in n for n in entries)
  files={n[len(prefix):]:z.read(n) for n in entries}
  allowed={'firmware.bin','bootloader.bin','partitions.bin','binary-verification.json','scripts/flash_panel.py','scripts/flash_COM3.cmd','docs/flash-panel.md','SOURCE.txt','START_HERE.txt','SHA256SUMS.json'}
  assert all(n in allowed or n.startswith('licenses/') for n in files),'Unexpected files: backup/credentials must never be uploaded'
  manifest=json.loads(files['SHA256SUMS.json'])
  assert all(hashlib.sha256(files[n]).hexdigest()==h for n,h in manifest.items())
  assert f'GitHub commit: {args.source}' in files['SOURCE.txt'].decode()
 firmware_hash=hashlib.sha256(files['firmware.bin']).hexdigest();package_hash=hashlib.sha256(package.read_bytes()).hexdigest()
 hw=json.loads(args.hardware_result.read_text(encoding='utf-8'))
 assert hw['version']==tag and hw['firmware_sha256']==firmware_hash and hw['source_commit']==args.source
 assert hw['readback_and_metadata_verified'] is True and hw['http_ok'] is True
 assert git('rev-parse',f'refs/tags/{tag}')==args.source,'Tag must identify the exact build source'
 subprocess.run(['git','merge-base','--is-ancestor',args.source,'origin/main'],cwd=ROOT,check=True)
 remote=git('ls-remote','origin',f'refs/tags/{tag}');assert remote.split()[0]==args.source
 notes=args.notes.read_text(encoding='utf-8')
 print(json.dumps({'tag':tag,'source':args.source,'package':package.name,'sha256':package_hash,'publish':args.publish}))
 if not args.publish:return
 env=os.environ.copy();env.update(GIT_TERMINAL_PROMPT='0',GCM_INTERACTIVE='never',GIT_TRACE='0',GIT_TRACE_CURL='0',GIT_CURL_VERBOSE='0')
 credential=subprocess.run(['git','credential','fill'],cwd=ROOT,input=f'protocol=https\nhost=github.com\npath={REPO}.git\n\n',capture_output=True,text=True,env=env)
 if credential.returncode:raise RuntimeError('Existing GitHub credentials unavailable; no release was published')
 values=dict(line.split('=',1) for line in credential.stdout.splitlines() if '=' in line)
 token=values.get('password')
 if not token:raise RuntimeError('Existing GitHub token unavailable')
 def api(method,path,data=None,mime='application/json',upload=False,allow404=False):
  base='https://uploads.github.com/repos/' if upload else 'https://api.github.com/repos/'
  payload=(json.dumps(data).encode() if data is not None and mime=='application/json' else data)
  request=urllib.request.Request(base+REPO+path,data=payload,method=method,headers={'Authorization':'Bearer '+token,'Accept':'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28','User-Agent':'BoilerPanel-release','Content-Type':mime})
  try:
   with urllib.request.urlopen(request,timeout=90) as response:return json.loads(response.read() or b'{}')
  except urllib.error.HTTPError as e:
   if allow404 and e.code==404:return None
   raise RuntimeError(f'GitHub {method} failed with HTTP {e.code}; credentials are not logged') from None
 release=api('GET','/releases/tags/'+tag,allow404=True)
 if release is None:release=api('POST','/releases',{'tag_name':tag,'target_commitish':args.source,'name':'Boiler Panel '+tag,'body':notes,'draft':False,'prerelease':False,'make_latest':'true'})
 else:
  assert release['tag_name']==tag
  release=api('PATCH','/releases/'+str(release['id']),{'body':notes,'draft':False,'prerelease':False,'make_latest':'true'})
 sums=(f'{package_hash}  {package.name}\n{firmware_hash}  firmware.bin\n').encode()
 assets={a['name']:a for a in release.get('assets',[])}
 for name,data,mime in [(package.name,package.read_bytes(),'application/zip'),('SHA256SUMS.txt',sums,'text/plain')]:
  digest='sha256:'+hashlib.sha256(data).hexdigest()
  if name in assets:
   assert assets[name].get('digest')==digest,'Existing asset differs; refusing to overwrite'
   continue
  asset=api('POST',f'/releases/{release["id"]}/assets?name='+urllib.parse.quote(name),data,mime,upload=True)
  assert asset['size']==len(data)
  if asset.get('digest'):assert asset['digest']==digest
 final=api('GET','/releases/'+str(release['id']))
 assert not final['draft'] and any(a['name']==package.name for a in final['assets'])
 print(json.dumps({'release':final['html_url'],'tag':final['tag_name'],'assets':[{'name':a['name'],'size':a['size'],'digest':a.get('digest')} for a in final['assets']]}))
 values.clear();token=None
if __name__=='__main__':main()
