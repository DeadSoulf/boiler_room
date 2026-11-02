#!/usr/bin/env python3
"""Firmware/web guard: no simulated equipment and no remote actuator operations."""
from pathlib import Path
from html.parser import HTMLParser
import re, subprocess, tempfile

root=Path(__file__).resolve().parents[1]
main=(root/"src/main.cpp").read_text(encoding="utf-8")
web=(root/"src/panel_web.cpp").read_text(encoding="utf-8")
page=(root/"src/panel_web_page.h").read_text(encoding="utf-8")
header=(root/"src/panel_web.h").read_text(encoding="utf-8")
equipment=(root/"src/panel_equipment.h").read_text(encoding="utf-8")
radio=(root/"src/panel_radio.cpp").read_text(encoding="utf-8")
version=(root/"src/panel_types.h").read_text(encoding="utf-8")

assert 'VERSION = "v0.5.6"' in version
assert "recoverIdleWifiForScan" in radio
assert '#include "panel_equipment.h"' in main and '#include "panel_equipment.h"' in web
assert "panel_web::publish(measurements,fuelLevel,millis())" in main
assert 'equipment=NOT_CONNECTED outputs=NONE' in main
assert "panel_equipment::COUNT" in web
assert "equipmentInfoCard(" in main
assert "Контроллер не подключён" in main
assert "gpio_set_level(" not in web
assert "digitalWrite(" not in main and "digitalWrite(" not in web
assert "panel_radio::submit(cmd)" in web
assert 'server.on("/api/pair",HTTP_POST,pairRequest)' in web
assert 'server.on("/api/command",HTTP_POST,commandRequest)' in web
for val in ('"wifi_scan"','"wifi_connect"','"wifi_forget"','"wifi_disconnect"','"ble_scan"','"zone"','"clock"','"screen"'):
    assert val in web,val
for source in (main,web,page,header,equipment):
    assert not re.search(r'(?i)demo|демо|имитац|фиктивн',source), \
        [(m.group(),source[m.start()-20:m.end()+20]) for m in re.finditer(r'(?i)demo|демо|имитац|фиктивн',source)]
assert "equipment_demo.h" not in main and "equipment_demo.h" not in web
assert 'enabled' not in equipment
assert r'\"connected\":false' in web
assert r'\"not_connected\"' in web
assert "Включить (ДЕМО)" not in page and "Сбросить ДЕМО" not in page
assert "data-action=\"wifi_scan\"" in page

start=page.index("<!doctype html>")
end=page.index("</html>")+len("</html>")
html=page[start:end]
class CheckHTML(HTMLParser):
    def __init__(self):
        super().__init__();self.ids=set()
    def handle_starttag(self,tag,attributes):
        attrs=dict(attributes)
        if "id" in attrs:
            assert attrs["id"] not in self.ids,attrs["id"]
            self.ids.add(attrs["id"])
parser=CheckHTML();parser.feed(html)
for expected in ("home","equipment","fuel","settings","service","equipment-list","wifi-form","paircode","screen-select","sd-card","sd-free","sd-state"):
    assert expected in parser.ids,expected
assert "demo-outcome" not in parser.ids
script=html.split("<script>",1)[1].split("</script>",1)[0]
with tempfile.TemporaryDirectory() as td:
    js=Path(td)/"app.js";js.write_text(script,encoding="utf-8")
    subprocess.run(["node","--check",str(js)],check=True)
print("PASS: no simulation in firmware/browser, seven unconnected devices, protected network settings, valid JavaScript")
