from pathlib import Path
import shutil,json,re,importlib.util,os,hashlib,subprocess,tempfile
from portrait import transform
root=Path(__file__).resolve().parents[1]
bsp=root/'.build/bsp'; example=bsp/'examples/LvglUi'
board=json.loads((bsp/'boards/jc4880p4.json').read_text(encoding='utf-8'))
assert board['build']['chip_variant']=='esp32p4_es'
ini=example/'platformio.ini'; s=ini.read_text(encoding='utf-8')
assert s.count('lvgl@~8.3.0')==1
s=s.replace('lvgl@~8.3.0','lvgl@8.3.11')
# Compile the reviewed OneWire sources directly in the generated board project.
onewire_commit='c2aa0147aa7414419fce61e89ed07ef6ff1d535b'
onewire_cache=root/'.build/onewire-git'
if not onewire_cache.is_dir():
    subprocess.run(['git','init','--bare',str(onewire_cache)],check=True)
git=['git','--git-dir',str(onewire_cache)]
if subprocess.run(git+['cat-file','-e',onewire_commit+'^{commit}'],stderr=subprocess.DEVNULL).returncode:
    subprocess.run(git+['fetch','--depth=1','https://github.com/junkfix/esp32-ds18b20.git',onewire_commit],check=True)
with tempfile.TemporaryDirectory() as stage:
    library=Path(stage)
    for name in ('OneWireESP32.h','OneWireESP32.cpp','LICENSE'):
        (library/name).write_bytes(subprocess.check_output(git+['show',onewire_commit+':'+name]))
    if (library/'LICENSE').read_text(encoding='utf-8')!=(root/'licenses/esp32-ds18b20-MIT.txt').read_text(encoding='utf-8'):
        raise RuntimeError('Packaged esp32-ds18b20 license differs from the pinned source')
    subprocess.run(['git','apply','--no-index',str(root/'ci/onewire-rmt.patch')],cwd=library,check=True)
    for name in ('OneWireESP32.h','OneWireESP32.cpp'):
        shutil.copy2(library/name,example/'src'/name)
url='https://github.com/pioarduino/platform-espressif32/releases/download/55.03.36-1/platform-espressif32.zip'
assert s.count(url)==1
# Verify the pinned platform archive independently of OS-specific PlatformIO URI handling.
archive=root/'.build/platform-espressif32.zip'
assert hashlib.sha256(archive.read_bytes()).hexdigest()=='837ff736b9a088186ecb99a96b830a1eaccc7f16bb0ee6c73c4e76c2ef3d936e'
# PlatformIO on Windows treats file:///C:/... as /C:/...; use the pinned release URL.
s=s.replace(url,url if os.name=='nt' else archive.as_uri())
s,n=re.subn(r'-DCORE_DEBUG_LEVEL=\d+', '-DCORE_DEBUG_LEVEL=0',s)
assert n==1
assert 'board_build.f_cpu' not in s
s=s.replace('board = jc4880p4','board = jc4880p4\nboard_build.f_cpu = 360000000L')
ini.write_text(s,encoding='utf-8')
glue=example/'src/lvgl_glue.c'; glue.write_text(transform(glue.read_text(encoding='utf-8')),encoding='utf-8')
# Reuse exactly the installer's checked v0.3.1 touch-ready/wake adapter.
spec=importlib.util.spec_from_file_location('boiler_updater',root/'apply_update.py')
updater=importlib.util.module_from_spec(spec);spec.loader.exec_module(updater)
glue.write_text(updater.patch_glue(glue.read_text(encoding='utf-8')),encoding='utf-8')
assert glue.read_text(encoding='utf-8').count('panel_screen_filter_touch(pressed)')==1
assert glue.read_text(encoding='utf-8').count('panel_screen_touch_ready()')==1
assert 'board_p4_touch_init()' not in glue.read_text(encoding='utf-8')
assert re.search(r'^board_build\.f_cpu\s*=\s*360000000L$',s,re.M)
assert s.count('-DCORE_DEBUG_LEVEL=0')==1
shutil.copy2(glue,root/'artifacts/lvgl_glue.c')
for p in (root/'src').iterdir():
 if p.is_file(): shutil.copy2(p,example/'src'/p.name)
shutil.copy2(ini,root/'artifacts/platformio.ini')
shutil.copy2(bsp/'boards/jc4880p4.json',root/'artifacts/jc4880p4.json')
