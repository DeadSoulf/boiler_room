#!/usr/bin/env python3
"""SDMMC mount + read-only status + bounded logging structural checks."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
cpp=(root/"src/panel_storage.cpp").read_text(encoding="utf-8")
header=(root/"src/panel_storage.h").read_text(encoding="utf-8")
main=(root/"src/main.cpp").read_text(encoding="utf-8")
web=(root/"src/panel_web.cpp").read_text(encoding="utf-8")
page=(root/"src/panel_web_page.h").read_text(encoding="utf-8")
version=(root/"src/panel_types.h").read_text(encoding="utf-8")
assert 'VERSION = "v0.5.7"' in version
assert 'SD_MMC.setPins(BOARD_P4_SD_CLK,BOARD_P4_SD_CMD,BOARD_P4_SD_D0' in cpp
assert 'BOARD_P4_SD_D1,BOARD_P4_SD_D2,BOARD_P4_SD_D3' in cpp
assert 'SD_MMC.begin("/sdcard",false,allowFormat,SDMMC_FREQ_DEFAULT,3)' in cpp
assert 'SD_MMC.setPowerChannel(-1)' in cpp
assert 'SDMMC SLOT 0' in cpp
for forbidden in ('format_if_mount_failed=true','SD_MMC.format','mkfs','f_mkfs','format()','eraseFlash','formatOnFail'):
    assert forbidden not in cpp,forbidden
assert "xTaskCreate(worker" in cpp
assert "xQueueReceive(" in cpp and "xQueueSend(pending,&e,0)" in cpp
assert 'return appendLine(kEvents,line,size_t(n),kEventLimit);' in cpp
assert 'return appendLine(kSamples,line,size_t(n),kSampleLimit);' in cpp
assert 'kEventLimit=128*1024' in cpp and 'kSampleLimit=256*1024' in cpp
assert 'kMinReserve=8ULL*1024*1024' in cpp
assert 'total/20' in cpp
assert 'lastSampleAt)<60000U' in cpp
assert 'if(!any)return false;' in cpp
assert '/boiler-panel/logs' in cpp
assert 'SD_MMC.remove(older)' in cpp and 'SD_MMC.rename(previous,older)' in cpp
assert 'panel_storage::begin()' in main
assert 'panel_storage::measurements(measurements,millis())' in main
assert 'panel_storage::snapshot(data)' in main
assert 'panel_storage::snapshot(storage)' in web
for name in ('card_bytes','total_bytes','used_bytes','free_bytes','written_events','written_samples','logging','mounted'):
    assert name in web,name
for name in ('sd-card','sd-total','sd-free','sd-used','sd-logging','sd-state'):
    assert name in page,name
assert 'panel_storage::event("wifi"' in main
assert "password" not in cpp.lower() and "sessiontoken" not in cpp.lower()
# SD functions must be exclusive to the module, not called from GUI or HTTP task.
assert "SD_MMC." not in main and "SD_MMC." not in web
html=page.split("<!doctype html>",1)[1].split("</html>",1)[0]
script=html.split("<script>",1)[1].split("</script>",1)[0]
with tempfile.TemporaryDirectory() as td:
    p=Path(td)/"browser.js"
    p.write_text(script,encoding="utf-8")
    subprocess.run(["node","--check",str(p)],check=True)
print("PASS: SDMMC fixed pins, no formatting, bounded logs, async task, actual-only readings, web status and JS syntax")
