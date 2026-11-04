#!/usr/bin/env python3
"""Fail visibly when public version labels diverge. No network or device access."""
from pathlib import Path
import re
r=Path(__file__).resolve().parents[1]
version=re.search(r'\bVERSION\s*=\s*"(v\d+\.\d+\.\d+)"',(r/'src/panel_types.h').read_text(encoding='utf-8')).group(1)
assert (r/'VERSION').read_text(encoding='ascii').strip()==version,'VERSION differs from firmware'
for name in ('README.md','README_RU.md'):
 title=(r/name).read_text(encoding='utf-8').splitlines()[0]
 assert version in title,(name,title,version)
assert re.search(r'^## '+re.escape(version)+r'\b',(r/'CHANGELOG.md').read_text(encoding='utf-8'),re.M),'Missing changelog entry'
print('PASS: firmware, VERSION, both README titles and CHANGELOG agree:',version)
