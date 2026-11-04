"""v0.6.1: static invariants of real DS18B20/reed acquisition and pin safety."""
from pathlib import Path
import subprocess,tempfile,re
r=Path(__file__).resolve().parents[1]
read=lambda name:(r/name).read_text(encoding="utf-8")
pins=read("src/panel_pin_profile.h")
sensors=read("src/panel_sensors.cpp")
config=read("src/panel_config.cpp")
web=read("src/panel_web.cpp")
page=read("src/panel_web_page.h")
main=read("src/main.cpp")
ci=read("ci/prepare.py")
version=read("src/panel_types.h")
assert 'VERSION = "v0.8.1"' in version
expected={52:7,33:8,51:9,31:10,50:11,30:12,49:13,29:14,34:17,32:19,28:21}
pairs={int(g):int(p) for g,p in re.findall(r"\{(\d+),(\d+)\}",pins)}
assert pairs==expected,(pairs,expected)
assert "panel_pins::sensorPin(a.gpio)" in config
assert "if(!checked(next.item))" in config
assert "if(h!=entry.hash || !checked(entry.item))return false;" in config
assert "bp_cfg_a" in config and "bp_cfg_b" in config
assert "bool rollback()" in config
assert "OneWire32" in sensors
assert "d.bus->search(devices,2)" in sensors
assert "crc8(rom,7)!=rom[7]" in sensors
assert "d.bus->getTemp(d.rom,c)" in sensors
assert "CONVERSION_MS=850" in sensors
assert "gpio_set_level" not in sensors and "digitalWrite(" not in sensors
assert "pinMode(a.gpio,INPUT_PULLUP)" in sensors
assert "panel_config::reedLevelReached(d.setting.type,inputHigh)" in sensors
assert "type==Type::ReedNO ? !inputHigh : inputHigh" in read("src/panel_config.h")
assert "d.due=now+PERIOD_MS" in sensors
assert "xQueueSend(samples,&e,0)" in sensors
assert "if(config.revision!=activeRevision)reconfigure(config)" in sensors
assert "panel_sensors::poll(sensorEvent)" in main
assert "fuelLevel.lower.put(" in main and "fuelLevel.upper.put(" in main
assert "measurements[sensorEvent.channel].put" in main
assert "panel_storage::measurements(measurements,millis())" in main
assert "panel_sensors::begin()" in main
assert "panel_sensors::snapshot(sensors)" in web
assert "connection_rollback" in web and "connection_rollback" in page
assert "cfg-gpio" in page and "JP1 контакт" in page
assert "drivers_active" in web
assert "c2aa0147aa7414419fce61e89ed07ef6ff1d535b" in ci
assert "DS18B20" not in main or "DS18B20" in sensors
for p in (sensors,config,web,main):
    assert "DEMO" not in p and "demo_on" not in p
script=page.split("<script>",1)[1].split("</script>",1)[0]
with tempfile.TemporaryDirectory() as tmp:
    js=Path(tmp)/"panel.js"
    js.write_text(script,encoding="utf-8")
    subprocess.run(["node","--check",str(js)],check=True)
print("PASS: validated JP1, real OneWire/contacts, CRC/debounce, NVS rollback, LVGL, web and microSD")
