#!/usr/bin/env python3
"""Render exact SVG primitives to PDF and PNG; requires ReportLab, PyMuPDF and Pillow."""
from pathlib import Path
import hashlib
import json
import fitz
import xml.etree.ElementTree as ET
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.colors import HexColor
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parent
svgs=sorted(ROOT.glob('screen-*.svg'))
if len(svgs)!=6:
    svgs=sorted(ROOT.glob('screens-*.svg'))
if len(svgs)!=6:
    svgs=sorted(ROOT.glob('*.svg'))
assert len(svgs)==6, f'Expected 6 screens, got {len(svgs)}'
pdfmetrics.registerFont(TTFont('PanelSans','/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'))
pdfmetrics.registerFont(TTFont('PanelSansBold','/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'))
pdf_path=ROOT/'boiler-fuel-monitoring.pdf'
vector_pdf=canvas.Canvas(str(pdf_path),pagesize=(480,800),pageCompression=1)
vector_pdf.setTitle('Панель котельной - топливо, предупреждения и диагностика')
vector_pdf.setAuthor('Boiler Panel project')
vector_pdf.setSubject('Согласованный визуальный макет. Все состояния демонстрационные.')
for source in svgs:
    root=ET.fromstring(source.read_text())
    assert root.attrib.get('viewBox')=='0 0 480 800'
    for el in root:
        tag=el.tag.rsplit('}',1)[-1]
        a=el.attrib
        number=lambda key,default=0:float(a.get(key,default))
        fill=a.get('fill')
        stroke=a.get('stroke')
        if fill and fill!='none': vector_pdf.setFillColor(HexColor(fill))
        if stroke and stroke!='none': vector_pdf.setStrokeColor(HexColor(stroke))
        vector_pdf.setLineWidth(number('stroke-width',1))
        if tag=='rect':
            vector_pdf.roundRect(number('x'),800-number('y')-number('height'),number('width'),number('height'),number('rx'),stroke=int(bool(stroke)),fill=int(bool(fill and fill!='none')))
        elif tag=='circle':
            vector_pdf.circle(number('cx'),800-number('cy'),number('r'),stroke=int(bool(stroke)),fill=int(bool(fill and fill!='none')))
        elif tag=='line':
            vector_pdf.line(number('x1'),800-number('y1'),number('x2'),800-number('y2'))
        elif tag=='text':
            assert not len(el), 'Nested text is not supported by this exact SVG renderer'
            vector_pdf.setFont('PanelSansBold' if number('font-weight',400)>=600 else 'PanelSans',number('font-size',19))
            method={'start':vector_pdf.drawString,'middle':vector_pdf.drawCentredString,'end':vector_pdf.drawRightString}[a.get('text-anchor','start')]
            method(number('x'),800-number('y'),el.text or '')
        else:
            assert tag in ('title','desc'), f'Unsupported SVG primitive: {tag}'
    vector_pdf.showPage()
vector_pdf.save()
combined=fitz.open(pdf_path)
assert len(combined)==6
rows=[]
for i,(source,p) in enumerate(zip(svgs,combined),1):
    assert abs(p.rect.width-480)<0.1 and abs(p.rect.height-800)<0.1,(source,p.rect)
    output=source.with_suffix('.png')
    p.get_pixmap(matrix=fitz.Matrix(1,1),alpha=False).save(output)
    im=Image.open(output).convert('RGB')
    im.quantize(colors=128,method=Image.Quantize.MEDIANCUT).save(output,optimize=True)
    text=p.get_text()
    assert 'МАКЕТ' in text.upper(),f'Missing demo label: {source}'
    rows.append({'screen':i,'svg':source.name,'png':output.name,'width':480,'height':800,'text':text})
combined.close()
font_path='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
label_font=ImageFont.truetype(font_path,19)
small_font=ImageFont.truetype(font_path,14)
labels=['Главная · мало топлива','Топливо · рабочий диапазон','Топливо · верхний уровень','Журнал событий','Диагностика датчиков','Сервис · данные устарели']
def contact_sheet(indices,name):
    gap=24
    cols=2
    nrows=(len(indices)+1)//2
    width=gap+cols*(480+gap)
    height=74+nrows*(800+50)+gap
    sheet=Image.new('RGB',(width,height),'#09121F')
    draw=ImageDraw.Draw(sheet)
    draw.text((gap,16),'ПАНЕЛЬ КОТЕЛЬНОЙ · ТОПЛИВО',font=label_font,fill='#F1F6FC')
    draw.text((gap,44),'Согласованный макет · примеры состояний · 480 × 800',font=small_font,fill='#96ABC1')
    for j,index in enumerate(indices):
        x=gap+(j%2)*(480+gap)
        y=74+(j//2)*850
        draw.text((x,y+8),labels[index],font=label_font,fill='#F1F6FC')
        screen=Image.open(ROOT/rows[index]['png']).convert('RGB')
        sheet.paste(screen,(x,y+42))
        draw.rectangle((x-1,y+41,x+480,y+842),outline='#293D54',width=1)
    sheet.quantize(colors=192,method=Image.Quantize.MEDIANCUT).save(ROOT/name,optimize=True)
contact_sheet([0,1,3,4],'overview.png')
contact_sheet([0,1,2,3,4,5],'all-screens.png')
review={'renderer':'ReportLab SVG primitives with embedded DejaVu Sans -> vector PDF; PyMuPDF PNG; Pillow contact sheets','screens':rows,'pdf_pages':6,'actual_screen_size':[480,800]}
(ROOT/'render-evidence.json').write_text(json.dumps(review,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'pages':6,'screens':[r['png'] for r in rows],'pdf':'boiler-fuel-monitoring.pdf','overview':'overview.png','all_screens':'all-screens.png'},ensure_ascii=False))
