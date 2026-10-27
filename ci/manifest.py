from pathlib import Path
import hashlib,json,subprocess
r=Path(__file__).resolve().parents[1]
paths=[p for base in ['src','ci','tests','.github'] for p in (r/base).rglob('*') if p.is_file() and '__pycache__' not in p.parts]
if (r/'apply_update.py').exists():paths.append(r/'apply_update.py')
result={'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip(),'sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}}
(r/'artifacts/input-manifest.json').write_text(json.dumps(result,indent=2)+'\n')
outputs={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((r/'artifacts').iterdir()) if p.is_file() and p.name!='output-sha256.json'}
(r/'artifacts/output-sha256.json').write_text(json.dumps(outputs,indent=2)+'\n')
