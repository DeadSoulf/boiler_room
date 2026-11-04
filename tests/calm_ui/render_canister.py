"""Render the actual LVGL captures, not invented live measurements. Standard library only."""
from pathlib import Path
import struct,zlib,base64,json,sys
root=Path(__file__).resolve().parents[2]
source=Path(sys.argv[1]) if len(sys.argv)>1 else root/'artifacts/canister'
out=root/'docs/fuel-canister';out.mkdir(parents=True,exist_ok=True)
names=['full','half','empty','invalid','unknown','stale']
labels=['Оба разомкнуты','Верхний замкнут','Оба замкнуты','Нижний замкнут отдельно','Нет показаний','Данные устарели']
def ppm(p):
 magic,wh,peak,pixels=p.read_bytes().split(b'\n',3);assert magic==b'P6' and peak==b'255';w,h=map(int,wh.split());assert len(pixels)==w*h*3;return w,h,pixels
def png(w,h,pixels):
 def chunk(k,b):return struct.pack('>I',len(b))+k+b+struct.pack('>I',zlib.crc32(k+b)&0xffffffff)
 raw=b''.join(b'\0'+pixels[y*w*3:(y+1)*w*3] for y in range(h))
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b'')
frames={};crops=[]
for n in names:
 w,h,p=ppm(source/f'canister-{n}-fixture.ppm');assert (w,h)==(480,800)
 frames[n]=png(w,h,p);(out/f'home-{n}.png').write_bytes(frames[n])
 # Exact card bounds in the current 480x800 layout. No scaling or re-drawing.
 x,y,cw,ch=248,434,208,300
 cropped=b''.join(p[((y+j)*w+x)*3:((y+j)*w+x+cw)*3] for j in range(ch))
 crops.append(cropped);(out/f'card-{n}.png').write_bytes(png(cw,ch,cropped))
width,height=3*224+16,332
pixels=bytearray(bytes([16,23,25])*width*height)
for i,crop in enumerate(crops[:3]):
 for y in range(300):
  a=((y+16)*width+16+i*224)*3;pixels[a:a+208*3]=crop[y*208*3:(y+1)*208*3]
(out/'three-states.png').write_bytes(png(width,height,pixels))
encoded={n:'data:image/png;base64,'+base64.b64encode(frames[n]).decode() for n in names}
html='''<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Канистра в карточке топлива</title><style>body{margin:24px;background:#101719;color:#eff5f3;font:16px/1.5 system-ui}main{display:flex;gap:30px;max-width:950px;margin:auto;flex-wrap:wrap}img{max-width:100%;width:480px;height:auto}nav{display:grid;gap:10px}button{padding:14px;background:#1b2528;border:1px solid #304044;color:#eff5f3;cursor:pointer;border-radius:12px;text-align:left;font:inherit}button[aria-pressed=true]{border-color:#76d6c3;color:#76d6c3}aside{max-width:370px}p{color:#a1b3b5}h1{font-size:24px}</style><main><img id="screen" alt="Главная страница панели с канистрой"><aside><h1>Канистра внутри карточки «Топливо»</h1><p>Температура маленьким шрифтом над значком. Нижняя кнопка удалена.</p><nav id="states"></nav><p>Это отрисовка настоящего LVGL с тестовыми значениями, не данные устройства. Файл работает без сети и ничего не меняет на панели.</p><p>Для указанной схемы контактов оба входа используют режим NC: разомкнутый контакт = уровень достигнут. Пустая канистра с «!» отличается от неизвестных показаний с «?».</p></aside></main><script>const frames=FRAMES,names=NAMES,labels=LABELS;const image=document.getElementById('screen'),nav=document.getElementById('states');function show(n){image.src=frames[n];nav.querySelectorAll('button').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.name===n)))}names.forEach((n,i)=>{const b=document.createElement('button');b.textContent=labels[i];b.dataset.name=n;b.onclick=()=>show(n);nav.append(b)});show('full');</script></html>'''
html=html.replace('FRAMES',json.dumps(encoded)).replace('NAMES',json.dumps(names)).replace('LABELS',json.dumps(labels,ensure_ascii=False))
(out/'preview.html').write_text(html,encoding='utf-8')
print('PASS: six real LVGL states rendered; full/half/empty montage; offline preview')
