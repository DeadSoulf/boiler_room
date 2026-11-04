from pathlib import Path
import tempfile,subprocess,re
root=Path(__file__).resolve().parents[1]
main=(root/"src/main.cpp").read_text(encoding="utf-8")
cfg=(root/"src/panel_config.cpp").read_text(encoding="utf-8")
web=(root/"src/panel_web.cpp").read_text(encoding="utf-8")
html=(root/"src/panel_web_page.h").read_text(encoding="utf-8")
types=(root/"src/panel_types.h").read_text(encoding="utf-8")
assert 'VERSION = "v0.7.0"' in types
assert 'const char* titles[]={"Wi-Fi","Bluetooth устройства","Экран","Веб-доступ"};' in main
assert 'Проверка символов' not in main
assert 'case Time:view=3;' not in main
assert 'if(kind=="clock")' in web and 'if(kind=="zone")' in web
assert 'data-tab="clock"' in html
assert 'data-page="connections"' in html
assert 'data-action="connection_set"' in html
assert 'if(kind=="connection_set")' in web
assert 'if(!authorized())' in web
assert 'bp_cfg_a' in cfg and 'bp_cfg_b' in cfg
assert "panel_pins::sensorPin(a.gpio)" in cfg
pins=(root/"src/panel_pin_profile.h").read_text(encoding="utf-8")
for reserved in ('case 3:','case 5:','case 7:','case 23:','case 14:','case 18:','case 39:','case 43:','case 26:','case 35:'):
 assert reserved in pins,reserved
assert 'no device driver' not in cfg # explicit no activation reported through API
assert "drivers_active" in web and "panel_sensors::snapshot(sensors)" in web
assert 'gpio_set_level' not in cfg and 'digitalWrite' not in cfg
js=html.split("<script>",1)[1].split("</script>",1)[0]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/"app.js";p.write_text(js,encoding="utf-8")
 subprocess.run(["node","--check",str(p)],check=True)
print("PASS: v0.7.0 navigation, web clock, six-channel NVS dual slots, pin guards, no physical commands")
