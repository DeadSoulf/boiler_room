#!/usr/bin/env python3
"""Static safety and browser-script checks for the device-served HTTP page."""
from html.parser import HTMLParser
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
html=(root/"src/panel_web_page.h").read_text(encoding="utf-8")
cpp=(root/"src/panel_web.cpp").read_text(encoding="utf-8")
main=(root/"src/main.cpp").read_text(encoding="utf-8")
assert 'server.on("/",HTTP_GET,index)' in cpp
assert 'server.on("/api/status",HTTP_GET,status)' in cpp
assert 'server.on("/health",HTTP_GET' in cpp
assert "HTTP_POST" not in cpp and "enableCORS" not in cpp
assert "panel_radio::submit" not in cpp
assert "setAutoReconnect" not in cpp
assert "panel_web::publish(measurements,fuelLevel,equipmentDemo,millis())" in main
assert "panel_web::begin()" in main
assert "password" not in cpp.lower() or "No passwords" in cpp
assert 'data_origin' in cpp and 'physical_control' in cpp
class Tags(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids=set()
    def handle_starttag(self,tag,attrs):
        for key,value in attrs:
            if key=="id":
                assert value not in self.ids, value
                self.ids.add(value)
start=html.index("<!doctype html>")
end=html.index("</html>")+len("</html>")
page=html[start:end]
parser=Tags()
parser.feed(page)
for name in ("t0","t1","t2","t3","wifi","ip","rssi","screen","fuel","equipment","radio","uptime","onlineText"):
    assert name in parser.ids, name
assert 'fetch("/api/status"' in page
assert "HTTP" in page and "ДЕМО" in page
js=page.split("<script>",1)[1].split("</script>",1)[0]
with tempfile.TemporaryDirectory() as folder:
    path=Path(folder)/"page.js"
    path.write_text(js,encoding="utf-8")
    subprocess.run(["node","--check",str(path)],check=True)
print("PASS: read-only HTTP routes, no commands, response fields, DOM ids, JavaScript syntax")
