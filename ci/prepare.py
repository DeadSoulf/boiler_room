from pathlib import Path
import shutil,json,re,importlib.util
from portrait import transform
root=Path(__file__).resolve().parents[1]
bsp=root/'.build/bsp'; example=bsp/'examples/LvglUi'
board=json.loads((bsp/'boards/jc4880p4.json').read_text())
assert board['build']['chip_variant']=='esp32p4_es'
ini=example/'platformio.ini'; s=ini.read_text()
assert s.count('lvgl@~8.3.0')==1
s=s.replace('lvgl@~8.3.0','lvgl@8.3.11')
url='https://github.com/pioarduino/platform-espressif32/releases/download/55.03.36-1/platform-espressif32.zip'
assert s.count(url)==1
s=s.replace(url,(root/'.build/platform-espressif32.zip').as_uri())
s,n=re.subn(r'-DCORE_DEBUG_LEVEL=\d+', '-DCORE_DEBUG_LEVEL=0',s)
assert n==1
assert 'board_build.f_cpu' not in s
s=s.replace('board = jc4880p4','board = jc4880p4\nboard_build.f_cpu = 360000000L')
ini.write_text(s)
glue=example/'src/lvgl_glue.c'; glue.write_text(transform(glue.read_text()))
# Reuse exactly the installer's checked v0.3.1 touch-ready/wake adapter.
spec=importlib.util.spec_from_file_location('boiler_updater',root/'apply_update.py')
updater=importlib.util.module_from_spec(spec);spec.loader.exec_module(updater)
glue.write_text(updater.patch_glue(glue.read_text()))
assert glue.read_text().count('panel_screen_filter_touch(pressed)')==1
assert glue.read_text().count('panel_screen_touch_ready()')==1
assert 'board_p4_touch_init()' not in glue.read_text()
assert re.search(r'^board_build\.f_cpu\s*=\s*360000000L$',s,re.M)
assert s.count('-DCORE_DEBUG_LEVEL=0')==1
shutil.copy2(glue,root/'artifacts/lvgl_glue.c')
for p in (root/'src').iterdir():
 if p.is_file(): shutil.copy2(p,example/'src'/p.name)
shutil.copy2(ini,root/'artifacts/platformio.ini')
shutil.copy2(bsp/'boards/jc4880p4.json',root/'artifacts/jc4880p4.json')
