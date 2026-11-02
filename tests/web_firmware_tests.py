#!/usr/bin/env python3
"""Static safety checks for on-device HTTP and browser UI v0.5.3."""
from pathlib import Path
from html.parser import HTMLParser
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
cpp=(root/"src/panel_web.cpp").read_text(encoding="utf-8")
page=(root/"src/panel_web_page.h").read_text(encoding="utf-8")
main=(root/"src/main.cpp").read_text(encoding="utf-8")
types=(root/"src/panel_types.h").read_text(encoding="utf-8")
assert 'VERSION = "v0.5.3"' in types
for route in ('"/"','"/health"','"/api/status"','"/api/pair"','"/api/command"','"/api/logout"'):
    assert route in cpp,route
assert cpp.count("HTTP_POST")==3
assert "server.collectHeaders(headers,1)" in cpp
assert "if(!authorized())" in cpp
assert "physicalPin[0]&&future(pinExpires)" in cpp
assert "failedCodes>=5" in cpp and "esp_fill_random" in cpp
assert "sessionExpires=millis()+3600000U" in cpp
assert "pinExpires=millis()+120000U" in cpp
assert "enableCORS" not in cpp
assert "digitalWrite(" not in cpp and "gpio_set_level(" not in cpp
assert "panel_web::takeDemoAction(webCmd)" in main
assert "panel_web::armPairing()" in main
assert "panel_web::revokePairing()" in main
assert "panel_web::publish(measurements,fuelLevel,equipmentDemo,millis())" in main
start=page.index("<!doctype html>")
end=page.index("</html>")+len("</html>")
html=page[start:end]
class Parser(HTMLParser):
    def __init__(self):
        super().__init__();self.ids=set()
    def handle_starttag(self,tag,attrs):
        a=dict(attrs)
        if "id" in a:
            assert a["id"] not in self.ids,a["id"]
            self.ids.add(a["id"])
p=Parser();p.feed(html)
for required in ("home","equipment","fuel","settings","service","paircode","journal","wifi-form","ble-list","screen-select"):
    assert required in p.ids,required
script=html.split("<script>",1)[1].split("</script>",1)[0]
with tempfile.TemporaryDirectory() as td:
    js=Path(td)/"panel.js";js.write_text(script,encoding="utf-8")
    subprocess.run(["node","--check",str(js)],check=True)
print("PASS: full web routes, pairing, no physical writes, DOM and JS syntax")
