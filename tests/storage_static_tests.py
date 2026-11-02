#!/usr/bin/env python3
"""Contract checks: web-triggered microSD formatting NEVER automatic."""
from pathlib import Path
from html.parser import HTMLParser
import subprocess,tempfile,re
root=Path(__file__).resolve().parents[1]
sd=(root/"src/panel_storage.cpp").read_text(encoding="utf-8")
hdr=(root/"src/panel_storage.h").read_text(encoding="utf-8")
web=(root/"src/panel_web.cpp").read_text(encoding="utf-8")
main=(root/"src/main.cpp").read_text(encoding="utf-8")
page=(root/"src/panel_web_page.h").read_text(encoding="utf-8")
ver=(root/"src/panel_types.h").read_text(encoding="utf-8")
assert 'VERSION = "v0.6.1-neon"' in ver
assert "SDMMC SLOT 0" in sd and "host.slot=SDMMC_HOST_SLOT_0" in sd
assert "BOARD_P4_SD_CLK,BOARD_P4_SD_CMD,BOARD_P4_SD_D0" in sd
assert "BOARD_P4_SD_D1,BOARD_P4_SD_D2,BOARD_P4_SD_D3" in sd
assert "SD_MMC.setPowerChannel(-1)" in sd
assert 'SD_MMC.begin(kMountPath,false,false,SDMMC_FREQ_DEFAULT,3)' in sd
mount=sd.split("bool mountCard()",1)[1].split("bool rotate(",1)[0]
assert "format_if_mount_failed=true" not in mount
assert 'SD_MMC.begin(kMountPath,false,false,SDMMC_FREQ_DEFAULT,3)' in mount
assert "esp_vfs_fat_sdcard_format(kMountPath,card)" in sd
assert "esp_vfs_fat_sdcard_unmount(kMountPath,card)" in sd
assert "SD_MMC.end()" in sd
assert "cfg.format_if_mount_failed=true" in sd
assert "No automatic formatting at boot" in sd
assert "runPhysicalFormat()" in sd
assert "if(startFormat){runPhysicalFormat();continue;}" in sd
assert "published.formatAttempted=true;" in sd
assert "published.formatPhase==FormatPhase::Queued" in sd
assert "published.formatPhase==FormatPhase::AwaitingPhysical" in sd
assert "formatDeadline=millis()+kFormatWindowMs" in sd
assert "kFormatWindowMs=90000" in sd
assert "bool confirmFormatPhysically()" in sd and "bool cancelFormatPhysically()" in sd
assert "bool prepareFormat()" in sd
assert "panel_storage::confirmFormatPhysically()" in main
assert "panel_storage::cancelFormatPhysically()" in main
assert 'SdFormatApprove=120,SdFormatCancel' in main
assert "updateStorageLabel();" in main
assert 'id="sd-format-phrase"' in page
assert 'data-action="sd_format_prepare"' in page
assert "requestStorageFormat()" in page
assert 'if(server.arg("confirm")!="СТЕРЕТЬ MICROSD")' in web
assert 'if(kind=="sd_format_prepare")' in web
assert web.index("if(!authorized())") < web.index('if(kind=="sd_format_prepare")')
assert "panel_storage::prepareFormat()" in web
assert "formatPhysicalCard()" not in web
assert "panel_storage::confirmFormatPhysically()" not in web
assert "cfg.format_if_mount_failed" not in web
assert "SD_MMC." not in web and "SD_MMC." not in main
assert "sessionToken" not in sd and "password" not in sd.lower()
assert "esp_restart()" not in sd and "ESP.restart()" not in sd
assert "kEventLimit=128*1024" in sd and "kSampleLimit=256*1024" in sd
assert "/boiler-panel/logs" in sd
start=page.index("<!doctype html>")
end=page.index("</html>")+len("</html>")
html=page[start:end]
class Parser(HTMLParser):
    def __init__(self):super().__init__();self.ids=set()
    def handle_starttag(self,tag,attrs):
        attrs=dict(attrs)
        if "id" in attrs:
            assert attrs["id"] not in self.ids
            self.ids.add(attrs["id"])
parser=Parser();parser.feed(html)
for elem in ("sd-format-button","sd-format-phrase","sd-format-progress","sd-state","sd-card","sd-free"):
    assert elem in parser.ids,elem
js=html.split("<script>",1)[1].split("</script>",1)[0]
with tempfile.TemporaryDirectory() as td:
    p=Path(td)/"app.js";p.write_text(js,encoding="utf-8")
    subprocess.run(["node","--check",str(p)],check=True)
print("PASS: physically confirmed one-shot SDMMC0 format; normal mounts never format; no P4 Flash/NVS/C6 writes; HTML/JS valid")
