"""Offline structural checks; not a claim of physical hardware compatibility."""
from pathlib import Path
import struct,json
root=Path(__file__).resolve().parents[1]/'artifacts'
raw=(root/'firmware.bin').read_bytes()
assert len(raw)>=24 and raw[0]==0xE9
chip=struct.unpack_from('<H',raw,12)[0]
assert chip==18, f'Expected ESP32-P4 image chip ID 18, got {chip}'
parts=[];data=(root/'partitions.bin').read_bytes()
for offset in range(0,len(data),32):
 record=data[offset:offset+32]
 if record[:2]!=b'\xaa\x50':break
 _,kind,subtype,start,size,label,flags=struct.unpack('<HBBII16sI',record)
 assert start+size<=16*1024*1024
 parts.append(dict(type=kind,subtype=subtype,offset=start,size=size,label=label.split(b'\0')[0].decode(),flags=flags))
assert parts and any(p['type']==0 for p in parts)
assert all(len(raw)<=p['size'] for p in parts if p['type']==0)
ordered=sorted(parts,key=lambda p:p['offset'])
for a,b in zip(ordered,ordered[1:]):assert a['offset']+a['size']<=b['offset']
result={'chip':'ESP32-P4','image_chip_id':chip,'image_bytes':len(raw),'min_chip_revision_legacy':raw[14],'min_chip_revision_full':struct.unpack_from('<H',raw,15)[0],'max_chip_revision_full':struct.unpack_from('<H',raw,17)[0],'partitions':parts,'hardware_verified':False,'note':'Image revision requirements must be checked against real board before manual installation. No C6 image or flash operation.'}
(root/'binary-verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
