from pathlib import Path
import hashlib,json,shutil,subprocess
r=Path(__file__).resolve().parents[1]
# Also runs from the workflow's always() step: even partial artifact bundles
# retain the notices already supplied by this repository.
(r/'artifacts').mkdir(exist_ok=True)
shutil.copytree(r/'licenses',r/'artifacts/licenses',dirs_exist_ok=True)
paths=[p for base in ['src','ci','tests','scripts','.github','licenses'] for p in (r/base).rglob('*') if p.is_file() and '__pycache__' not in p.parts]
if (r/'apply_update.py').exists():paths.append(r/'apply_update.py')
result={'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip(),'sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}}
(r/'artifacts/input-manifest.json').write_text(json.dumps(result,indent=2)+'\n')
outputs={p.relative_to(r/'artifacts').as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((r/'artifacts').rglob('*')) if p.is_file() and p!=r/'artifacts/output-sha256.json'}
(r/'artifacts/output-sha256.json').write_text(json.dumps(outputs,indent=2)+'\n')
