"""Export actual native LVGL fixtures for the v0.9.1 home-card review."""
from pathlib import Path
import struct,zlib,shutil
r=Path(__file__).resolve().parents[2]
out=r/'docs/home-icons-v091';out.mkdir(parents=True,exist_ok=True)
def png(w,h,rgb):
 def chunk(k,p):return struct.pack('>I',len(p))+k+p+struct.pack('>I',zlib.crc32(k+p)&0xffffffff)
 raw=b''.join(b'\0'+rgb[y*w*3:(y+1)*w*3] for y in range(h))
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b'')
for source,target in [('home-icons-no-data','panel-no-data'),('home-icons-values-fixture','panel-values-fixture'),('pairing-header-fixture','panel-pairing-fixture')]:
 magic,wh,depth,rgb=(r/'artifacts/calm-ui'/f'{source}.ppm').read_bytes().split(b'\n',3)
 w,h=map(int,wh.split());assert magic==b'P6' and depth==b'255' and (w,h)==(480,800) and len(rgb)==w*h*3
 (out/f'{target}.png').write_bytes(png(w,h,rgb))
for name in ('desktop-home.png','mobile-home.png','browser-result.json'):
 src=r/'artifacts/web-review'/name
 if src.exists():shutil.copyfile(src,out/name)
print('PASS: native 480x800 previews exported; all values and pairing code are synthetic fixtures')
