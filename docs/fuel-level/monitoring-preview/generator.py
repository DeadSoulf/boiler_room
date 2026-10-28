#!/usr/bin/env python3
"""Create six static 480x800 monitoring mockups and an inline-SVG Canva HTML.

No firmware, hardware, screenshots or PDFs are produced. Pillow is used only to
measure the installed DejaVu Sans font and validate text geometry.
"""
from pathlib import Path
from functools import lru_cache
import html
import json
import xml.etree.ElementTree as ET
from PIL import ImageFont

ROOT = Path(__file__).resolve().parent
WIDTH, HEIGHT = 480, 800
BG, CARD, LINE = '#09121F', '#152439', '#293D54'
TXT, MUTED = '#F1F6FC', '#96ABC1'
GREEN, SKY, AMBER, RED = '#66DDB4', '#62CFFF', '#F4C083', '#F18F93'
BUTTON = '#214258'
FONT_DIR = Path('/usr/share/fonts/truetype/dejavu')


@lru_cache(None)
def font(size, bold=False):
    return ImageFont.truetype(str(FONT_DIR / ('DejaVuSans-Bold.ttf' if bold else 'DejaVuSans.ttf')), size)


class Page:
    def __init__(self, number, slug, title, notes):
        self.number, self.slug, self.title, self.notes = number, slug, title, notes
        self.svg = ET.Element('svg', {'xmlns': 'http://www.w3.org/2000/svg', 'width': '480', 'height': '800',
                                     'viewBox': '0 0 480 800', 'role': 'img', 'aria-label': title})
        ET.SubElement(self.svg, 'title').text = title
        ET.SubElement(self.svg, 'desc').text = notes
        self.texts, self.buttons = [], []
        self.rect(0, 0, WIDTH, HEIGHT, BG, 0, None)

    def rect(self, x, y, w, h, fill=CARD, radius=18, stroke=LINE):
        assert x >= 0 and y >= 0 and x+w <= WIDTH and y+h <= HEIGHT
        attrs = {'x': str(x), 'y': str(y), 'width': str(w), 'height': str(h), 'rx': str(radius), 'fill': fill}
        if stroke: attrs.update({'stroke': stroke, 'stroke-width': '1'})
        return ET.SubElement(self.svg, 'rect', attrs)

    def line(self, x1, y, x2, color=LINE):
        ET.SubElement(self.svg, 'line', {'x1': str(x1), 'y1': str(y), 'x2': str(x2), 'y2': str(y),
                                      'stroke': color, 'stroke-width': '1'})

    def dot(self, x, y, color=MUTED, radius=4):
        ET.SubElement(self.svg, 'circle', {'cx': str(x), 'cy': str(y), 'r': str(radius), 'fill': color})

    def text(self, x, y, value, size=19, color=TXT, anchor='start', bold=False, max_width=None):
        assert size >= 14, (self.title, value, size)
        f = font(size, bold)
        length = f.getlength(value)
        if max_width is not None:
            assert length <= max_width, (self.title, value, round(length, 2), max_width)
        left = x if anchor == 'start' else x-length if anchor == 'end' else x-length/2
        bounds = f.getbbox(value, anchor='ls')
        box = (left+bounds[0], y+bounds[1], left+bounds[2], y+bounds[3])
        assert box[0] >= -0.5 and box[1] >= 0 and box[2] <= WIDTH+0.5 and box[3] <= HEIGHT, (value, box)
        attrs = {'x': str(x), 'y': str(y), 'fill': color, 'font-family': 'DejaVu Sans',
                 'font-size': str(size), 'font-weight': '700' if bold else '400', 'text-anchor': anchor}
        ET.SubElement(self.svg, 'text', attrs).text = value
        self.texts.append({'text': value, 'box': box, 'size': size})
        return box

    def wrap(self, x, y, value, size=19, color=TXT, width=414, leading=26, max_lines=2, bold=False):
        lines, current = [], ''
        for word in value.split():
            proposed = (current+' '+word).strip()
            if current and font(size, bold).getlength(proposed) > width:
                lines.append(current); current = word
            else: current = proposed
        if current: lines.append(current)
        assert len(lines) <= max_lines, (self.title, value, lines)
        for i, line in enumerate(lines): self.text(x, y+i*leading, line, size, color, bold=bold, max_width=width)
        return len(lines)

    def button(self, x, y, w, h, label, fill=BUTTON):
        assert h >= 44
        self.rect(x, y, w, h, fill, 12, None)
        box = self.text(x+w/2, y+h/2+6.5, label, 19, anchor='middle', max_width=w-24)
        assert box[0] >= x+8 and box[2] <= x+w-8 and box[1] >= y+4 and box[3] <= y+h-4
        self.buttons.append({'label': label, 'width': w, 'height': h})

    def header(self, title, subtitle=None, home=True):
        self.text(18, 48, title, 26, max_width=318)
        if home: self.button(352, 16, 112, 48, 'Домой')
        if subtitle: self.text(18, 104, subtitle, 19, MUTED, max_width=444)

    def badge(self):
        self.text(18, 788, 'МАКЕТ · пример состояний', 14, MUTED)

    def page_dots(self, active):
        for i in range(4): self.rect(206+i*19, 749, 16 if i == active else 7, 6, SKY if i == active else LINE, 3, None)

    def warning(self, y, title, subtitle, height=72):
        self.rect(16, y, 448, height, CARD, 16, AMBER)
        self.dot(43, y+31, AMBER, 15)
        self.text(43, y+38, '!', 22, BG, anchor='middle', bold=True)
        count = self.wrap(70, y+29, title, 19, AMBER, 374, 24, 2, True)
        self.text(70, y+(55 if count == 1 else 76), subtitle, 14, MUTED, max_width=376)

    def validate(self):
        overlaps = []
        for i, a in enumerate(self.texts):
            for b in self.texts[i+1:]:
                x = min(a['box'][2], b['box'][2])-max(a['box'][0], b['box'][0])
                y = min(a['box'][3], b['box'][3])-max(a['box'][1], b['box'][1])
                if x > 1 and y > 1: overlaps.append([a['text'], b['text'], round(x, 1), round(y, 1)])
        assert not overlaps, (self.title, overlaps)
        return {'page': self.number, 'label': self.title, 'size': [WIDTH, HEIGHT], 'text_elements': len(self.texts),
                'min_font_px': min(t['size'] for t in self.texts), 'text_overlaps': [], 'buttons': self.buttons}

    def serialize(self):
        ET.indent(self.svg, space='  ')
        return ET.tostring(self.svg, encoding='unicode')


def temperature_card(p, x, y, name, accent):
    p.rect(x, y, 216, 220)
    p.rect(x+16, y+17, 30, 4, accent, 2, None)
    p.text(x+15, y+47, name, 19, max_width=186)
    p.text(x+14, y+120, '--.-', 52)
    p.text(x+201, y+148, '°C', 25, accent, anchor='end')
    p.text(x+15, y+171, 'Нет данных', 14, MUTED)
    p.text(x+15, y+195, 'Истории пока нет', 14, accent)
    p.rect(x+15, y+211, 186, 3, LINE, 1, None)


def reed_row(p, y, upper, reached, reached_color=GREEN):
    p.rect(16, y, 448, 92)
    color = reached_color if reached else MUTED
    p.dot(38, y+28, color, 5)
    p.text(58, y+34, 'Верхний уровень' if upper else 'Нижний уровень', 19, max_width=222)
    p.text(58, y+65, 'Герконовый датчик', 14, MUTED)
    p.text(446, y+35, 'Достигнут' if reached else 'Не достигнут', 19, color, anchor='end', max_width=166)


def fuel_page(number, upper):
    title = 'Топливо — верхний рабочий уровень' if upper else 'Топливо — рабочий диапазон'
    notes = 'Демонстрационный макет мониторинга. Нижний порог достигнут; '+('верхний достигнут.' if upper else 'верхний не достигнут.')
    notes += ' Температура не имитируется. Состояния не являются показаниями физического оборудования.'
    p = Page(number, 'fuel-upper' if upper else 'fuel-working', title, notes)
    p.header('Топливо', 'Два герконовых датчика')
    p.rect(16, 126, 448, 113, CARD, 18, SKY if upper else GREEN)
    p.text(32, 153, 'Уровень топлива', 14, MUTED)
    p.text(32, 189, 'Верхний уровень достигнут' if upper else 'Рабочий диапазон', 25, SKY if upper else GREEN, max_width=416)
    p.text(32, 220, 'Оба порога подтверждены' if upper else 'Нижний достигнут · верхний ещё нет', 14, MUTED, max_width=416)
    reed_row(p, 255, True, upper, SKY if upper else GREEN)
    reed_row(p, 363, False, True)
    p.rect(16, 471, 448, 130)
    p.text(32, 500, 'Температура топлива', 19)
    p.text(32, 537, '--.- °C', 25, SKY)
    p.text(185, 534, 'Нет данных температуры', 14, MUTED, max_width=261)
    p.button(32, 547, 416, 44, 'История температуры')
    p.button(16, 619, 216, 48, 'Журнал')
    p.button(248, 619, 216, 48, 'Диагностика')
    p.text(18, 693, 'Уровень определяется по двум порогам.', 14, MUTED, max_width=444)
    p.button(16, 714, 448, 48, 'Назад')
    p.badge()
    return p


def build_pages():
    pages = []
    p = Page(1, 'home-low', 'Главная — мало топлива',
             'Демонстрационный сценарий: оба нормализованных порога не достигнуты. Общее предупреждение о низком уровне видно на Главной. Температуры не поступают; четыре точки пассивны.')
    p.header('КОТЕЛЬНАЯ', home=False)
    p.text(18, 77, 'Температура и уровень топлива', 14, MUTED, max_width=302)
    p.text(462, 45, '--:--', 25, SKY, anchor='end')
    p.text(462, 74, 'Время не задано', 14, MUTED, anchor='end')
    p.warning(95, 'Мало топлива', 'Нижний уровень не достигнут')
    temperature_card(p, 16, 184, 'В котельной', GREEN)
    temperature_card(p, 248, 184, 'На улице', SKY)
    temperature_card(p, 16, 420, 'Теплоноситель', AMBER)
    x, y = 248, 420
    p.rect(x, y, 216, 220)
    p.rect(x+16, y+17, 30, 4, SKY, 2, None)
    p.text(x+15, y+47, 'Топливо', 19)
    p.text(x+15, y+76, 'Температура', 14, MUTED)
    p.text(x+158, y+77, '--.-', 25, anchor='end')
    p.text(x+201, y+77, '°C', 19, SKY, anchor='end')
    p.text(x+15, y+101, 'Нет данных', 14, MUTED)
    p.line(x+15, y+113, x+201)
    p.text(x+15, y+135, 'Уровень топлива', 14, MUTED)
    p.text(x+15, y+162, 'Ниже нижнего', 19, AMBER, max_width=186)
    for offset, name in [(185, 'Верхний'), (208, 'Нижний')]:
        p.dot(x+18, y+offset-5, MUTED, 3)
        p.text(x+29, y+offset, name, 14, MUTED)
        p.text(x+201, y+offset, 'Не достигнут', 14, MUTED, anchor='end', max_width=105)
    p.text(18, 675, 'Свежие температуры: 0 / 4', 19)
    p.text(18, 701, 'Данные температуры пока не поступают', 14, MUTED)
    p.text(18, 733, 'Свайп влево / вправо — смена страницы', 14, MUTED)
    p.page_dots(0); p.badge(); pages.append(p)
    pages.extend([fuel_page(2, False), fuel_page(3, True)])

    p = Page(4, 'journal', 'Журнал — события текущего запуска',
             'Пример относительной последовательности событий после запуска. Формат времени — минуты:секунды. Постоянное хранение и восстановление после перезапуска не заявляются.')
    p.header('Журнал', 'События уровня топлива')
    p.text(18, 139, 'Время от запуска · мин:сек', 14, MUTED)
    p.line(18, 157, 462)
    events = [
        ('+00:00', 'Запуск панели', 'Ожидание двух показаний', TXT),
        ('+00:01', 'Нижний уровень достигнут', 'Уровень между датчиками', GREEN),
        ('+01:12', 'Верхний уровень достигнут', 'Оба порога подтверждены', SKY),
        ('+02:04', 'Верхний уровень не достигнут', 'Уровень между датчиками', TXT),
        ('+02:18', 'Нижний уровень не достигнут', 'Предупреждение: мало топлива', AMBER),
        ('+02:50', 'Нет актуальных показаний', 'Оценка уровня приостановлена', AMBER),
    ]
    for i, (time, title, detail, color) in enumerate(events):
        y = 173+i*83
        p.text(18, y+22, time, 19, SKY)
        count = p.wrap(112, y+22, title, 19, color, 350, 24, 2)
        p.text(112, y+(50 if count == 1 else 71), detail, 14, MUTED, max_width=350)
        p.line(18, y+76, 462)
    p.text(18, 695, 'События текущего запуска', 14, MUTED)
    p.button(16, 714, 448, 48, 'Назад к топливу')
    p.badge(); pages.append(p)

    p = Page(5, 'diagnostics', 'Диагностика — условный пример контактов',
             'Только в этом иллюстративном сценарии замкнутый контакт соответствует достигнутому порогу. Реальная полярность, GPIO и аппаратная интеграция не назначены. Давность показания не является контролем обрыва линии.')
    p.header('Диагностика', 'Входы уровня топлива')
    p.rect(16, 124, 448, 84)
    p.text(32, 152, 'Условный пример контактов', 19, SKY)
    p.text(32, 178, 'В этом макете: замкнут = достигнут', 14, MUTED)
    p.text(32, 199, 'Полярность подключения не задана', 14, MUTED)
    for y, title, contact, level, color in [(224, 'Верхний датчик', 'Разомкнут', 'Не достигнут', MUTED),
                                          (403, 'Нижний датчик', 'Замкнут', 'Достигнут', GREEN)]:
        p.rect(16, y, 448, 163)
        p.dot(38, y+24, color, 5)
        p.text(58, y+31, title, 19)
        for offset, label, value, ink in [(64, 'Контакт', contact, TXT), (99, 'Логический порог', level, color), (134, 'Подтверждение', '1 с назад', TXT)]:
            p.text(32, y+offset, label, 14, MUTED)
            p.text(446, y+offset, value, 19, ink, anchor='end', max_width=210)
    p.text(18, 602, 'Контроль линии: не подключён', 19, AMBER)
    p.text(18, 635, 'Давность относится к показанию.', 14, MUTED)
    p.text(18, 658, 'Она не подтверждает целостность линии.', 14, MUTED)
    p.button(16, 714, 448, 48, 'Назад к топливу')
    p.badge(); pages.append(p)

    p = Page(6, 'service-stale', 'Сервис — нет актуальных показаний уровня',
             'Демонстрация общего предупреждения вне раздела топлива. Оба показания устарели; низкий уровень не выводится. Состояние контактов и фактический запас топлива по устаревшим данным неизвестны.')
    p.header('Сервис', 'Состояние панели')
    p.warning(124, 'Нет актуальных показаний уровня', 'Оценка топлива приостановлена', 92)
    p.rect(16, 234, 448, 192)
    p.text(32, 264, 'Датчики топлива', 19)
    p.text(32, 302, 'Данные устарели', 25, AMBER)
    for y, name in [(340, 'Верхний уровень'), (373, 'Нижний уровень')]:
        p.text(32, y, name, 19)
        p.text(446, y, 'Устарело', 19, AMBER, anchor='end')
    p.text(32, 410, 'Последнее подтверждение: 35 с назад', 14, MUTED)
    p.rect(16, 444, 448, 163)
    p.text(32, 476, 'Мониторинг', 19, SKY)
    p.text(32, 520, 'Температуры', 19)
    p.text(446, 520, 'Нет данных', 19, MUTED, anchor='end')
    p.text(32, 563, 'Журнал', 19)
    p.text(446, 563, 'Текущий запуск', 19, MUTED, anchor='end')
    p.button(16, 625, 448, 50, 'Открыть топливо')
    p.text(18, 731, 'Свайп влево / вправо — смена страницы', 14, MUTED)
    p.page_dots(3); p.badge(); pages.append(p)
    return pages


def main():
    pages = build_pages()
    checks = [p.validate() for p in pages]
    markup = []
    for p in pages:
        svg = p.serialize()
        (ROOT/f'screens-{p.number:02d}-{p.slug}.svg').write_text(svg+'\n', encoding='utf-8')
        markup.append(f'<section class="screen" data-document-role="page" data-label="{html.escape(p.title, quote=True)}" '
                      f'data-speaker-notes="{html.escape(p.notes, quote=True)}">\n{svg}\n</section>')
    document = '''<!doctype html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Панель котельной — мониторинг топлива, макет</title>
<style>
html,body{margin:0;padding:0;background:#09121F;color:#F1F6FC;font-family:"DejaVu Sans",Arial,sans-serif}
.screen{position:relative;width:480px;height:800px;overflow:hidden;margin:0;break-after:page;page-break-after:always}
.screen>svg{display:block;width:480px;height:800px}
@page{size:480px 800px;margin:0}
</style>
</head>
<body>
'''+ '\n'.join(markup)+'\n</body>\n</html>\n'
    (ROOT/'screens.html').write_text(document, encoding='utf-8')
    print(json.dumps({'pages': checks, 'page_roots': len(pages), 'external_resources': False, 'javascript': False,
                      'html': 'screens.html'}, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
