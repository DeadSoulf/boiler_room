#!/usr/bin/env python3
"""Offline BoilerPanel v0.3.0/v0.3.1/v0.4.0 -> v0.5.0 source update.
Default read-only. Only exact known complete installations are accepted (UTF-8
BOM/CRLF tolerated). v0.1/v0.2 and edited/mixed installations are unsupported.
Caught write failures are rolled back; multi-file updates are NOT power-loss atomic.
No flashing, networking, NVS/flash erasure, or C6 changes.
"""
from __future__ import annotations
import argparse, ast, configparser, hashlib, json, os, re, sys
from datetime import datetime
from pathlib import Path

FILES = ('main.cpp','panel_types.h','panel_radio.h','panel_radio.cpp',
         'screen_state.h','panel_screen.h','panel_screen.cpp','equipment_demo.h')
RETIRED = ('display_idle.h',)
MARKER='/* BOILER_SCREEN_031: deferred touch and wake filter. */'

def digest(data): return hashlib.sha256(data).hexdigest()
def sha(path): return digest(path.read_bytes())
def text(path): return path.read_text(encoding='utf-8-sig').replace('\r\n','\n')
def normalized_sha(path): return digest(text(path).encode())

KNOWN_RELEASES = {'v03': {'main.cpp': 'f3b868683746c7902129618cc374fad61698d22c4562efc04db7a768948f48ff',
         'panel_radio.cpp': '693c3b5cd19da17184671a0dd31f60ad1a87fcee2cea10ef725767c62a6cd6e6',
         'panel_radio.h': 'e673b8dc7628e82277a4933a0a53f9def74fa7090deda9256e7a4eb83c64a88c',
         'panel_types.h': 'ae9d11b2c13ca295b93916938dbff06957aaf4532e7ba8ef4d528da4921ecd2e'},
 'v031': {'main.cpp': '86ed146c2ec9c5be37efc6a71ad694a6590071080e7d50afb474d4d2b46077d4',
          'panel_radio.cpp': '693c3b5cd19da17184671a0dd31f60ad1a87fcee2cea10ef725767c62a6cd6e6',
          'panel_radio.h': 'e673b8dc7628e82277a4933a0a53f9def74fa7090deda9256e7a4eb83c64a88c',
          'panel_screen.cpp': '15b35a3c9e503037da7a7b7f88ce2cbf6df3dcbc18c0f8ba43df57be988c979a',
          'panel_screen.h': '0d881c291fd1ecd9173a1825c378c4e17586e7e4f791661eb792c85cce259647',
          'panel_types.h': '729c81814084a6d192353bc0887d1809a54ea71c78ccea8c92309b41d2b06178',
          'screen_state.h': '3e6c61cbc58abcf4979086e332054b8793b4d39b19aa825743060989faf9362c'},
 'v04_early': {'display_idle.h': 'be138fb88983cc53bca7a0fc4bdcf38da8a52bef12f9226d40a59b864d328962',
               'main.cpp': '7e1bc7bd3a584cfc759539b4ec270a4cdb43f7490a020dc5c5a4fbb78b59130d',
               'panel_radio.cpp': 'fd291e5895b1adaed1757ca2c4a767141acef4849ff787e6c25f139b7ba4e045',
               'panel_radio.h': '85ab2b2c3b952c21e1728cc877e5a856901abde2d9e2c68bf7b2b2b57964cae7',
               'panel_types.h': '4180eb3acd6dedec742430e95460510ece7716f02595ccde5b6c7324e8f88b5c'},
 # Exact b700ef6 baseline before the fuel-level UI/model addition.
 'v04_before_fuel': {'main.cpp': '724fa4a275696fa2c6e9a9d831f511b6645554c9fbe24844145ed72f0b8f3c47',
                     'panel_types.h': '4180eb3acd6dedec742430e95460510ece7716f02595ccde5b6c7324e8f88b5c',
                     'panel_radio.h': '85ab2b2c3b952c21e1728cc877e5a856901abde2d9e2c68bf7b2b2b57964cae7',
                     'panel_radio.cpp': '49a38d2cf26a0d115cf8dd251381457adb65ea89ced513a5e02ad5bb99c883a4',
                     'screen_state.h': 'ad8f6bb29b91027e2e279a7273336b2518120310c361051249962495887f09ba',
                     'panel_screen.h': '0d881c291fd1ecd9173a1825c378c4e17586e7e4f791661eb792c85cce259647',
                     'panel_screen.cpp': '7b528a618967616928df15cc71897e43258796e3902e085de01e4e206a9f4767'},
 # Exact 339073c source set before the audited UI/radio fixes.
 'v04_fuel': {'main.cpp': '1aea53152fb700406168643aa2f3bb13b6febd4bd5e355ad75572bc5579fb6af',
              'panel_types.h': 'f176b79c9953e5f0386525ca873f9f00b504ab6ddc651916a6bf818c44ea42f3',
              'panel_radio.h': '85ab2b2c3b952c21e1728cc877e5a856901abde2d9e2c68bf7b2b2b57964cae7',
              'panel_radio.cpp': '49a38d2cf26a0d115cf8dd251381457adb65ea89ced513a5e02ad5bb99c883a4',
              'screen_state.h': 'ad8f6bb29b91027e2e279a7273336b2518120310c361051249962495887f09ba',
              'panel_screen.h': '0d881c291fd1ecd9173a1825c378c4e17586e7e4f791661eb792c85cce259647',
              'panel_screen.cpp': '7b528a618967616928df15cc71897e43258796e3902e085de01e4e206a9f4767'},
 # Exact e09980a source set (unchanged since audited firmware commit 73639bd).
 'v04_audited': {'main.cpp': '85f45c794a1e5a1ffb4a6440dc21e8641e9df9c6f4ffcc2dd7a3c93354138143',
                 'panel_types.h': 'f176b79c9953e5f0386525ca873f9f00b504ab6ddc651916a6bf818c44ea42f3',
                 'panel_radio.h': '85ab2b2c3b952c21e1728cc877e5a856901abde2d9e2c68bf7b2b2b57964cae7',
                 'panel_radio.cpp': 'b8b23bb99256ada064cce45ea6a696d3504355614171b4f28d9db5ed7f96cfd2',
                 'screen_state.h': 'ad8f6bb29b91027e2e279a7273336b2518120310c361051249962495887f09ba',
                 'panel_screen.h': '0d881c291fd1ecd9173a1825c378c4e17586e7e4f791661eb792c85cce259647',
                 'panel_screen.cpp': '7b528a618967616928df15cc71897e43258796e3902e085de01e4e206a9f4767'}}
GLUE_INPUTS = {'21f57f84236abf46aa7b612ee08a5ab10c1b28cc18d375b14bb4045c35e56f3b': '9f4731067dbc35d9fa4b6de74d74c514d033b10e42a1547d967e8ec3a9c02b42',
 'd24840d05fd756805d70fc8f6980a3d35068706ab275aaaf24ff7e7b5cfae535': '83c1004b49d99913194f144b0eff6006d793aa7770788b757c844768ff83d478'}
BOARD_HASH = 'b6837425c6562a83b6ed1995a348470ef51e2ee886911663498c2a43a3d50898'
HEADER_HASH = 'a2f5be55891c0b56020692ddcdbc262d50c0c9df7c4c2855b47ef68df82ce69b'
FONT_HASH = '2bc21156983acec91e8bb89f992302057a25b09b38e600c6da258b3d2c82ea9f'

def required_codepoints(source: str) -> set[int]:
    # Strip comments before enumerating ordinary C/C++ string literals.
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/', source)
    result: set[int] = set()
    for token in tokens:
        if not token.startswith('"'):
            continue
        text = ast.literal_eval(token)
        result.update(ord(c) for c in text if c not in "\n\r\t")
    return result


def verify_fonts(path: Path, needed: set[int]) -> None:
    content = path.read_text(encoding="utf-8-sig")
    for size in (14, 19, 25, 55):
        match = re.search(rf"unicode_list_{size}\[\]\s*=\s*\{{(.*?)\}};", content, re.S)
        cmap = re.search(rf"cmaps_{size}\[\]\s*=\s*\{{(.*?)\}};", content, re.S)
        if match is None or cmap is None:
            raise ValueError(f"Unsupported existing font layout, size {size}. No files changed.")
        start = re.search(r"\.range_start\s*=\s*(\d+)", cmap.group(1))
        if start is None:
            raise ValueError("Font range_start is missing. No files changed.")
        available = {int(start.group(1)) + int(n) for n in re.findall(r"\d+", match.group(1))}
        missing = sorted(needed - available)
        if missing:
            codes = ", ".join(f"U+{n:04X}" for n in missing)
            raise ValueError(f"Installed font {size} is missing {codes}. No files changed.")


def _patch_glue_known(s):
    checks=(r'#define\s+LV_HOR\s+PANEL_NATIVE_W\b',r'#define\s+LV_VER\s+PANEL_NATIVE_H\b')
    if not all(re.search(x,s) for x in checks) or 'board_p4_flush_region' not in s:
        raise ValueError('Portrait LVGL adapter not recognized; nothing changed.')
    if MARKER in s:
        if s.count('panel_screen_filter_touch(pressed)')!=1 or s.count('panel_screen_touch_ready()')!=1 or 'board_p4_touch_init()' in s:
            raise ValueError('Incomplete or edited screen adapter; nothing changed.')
        return s
    if 'panel_screen' in s:raise ValueError('An unknown screen patch already exists.')
    if s.count('#include "board_p4.h"')!=1:raise ValueError('Unexpected glue includes.')
    s=s.replace('#include "board_p4.h"','#include "board_p4.h"\n#include "panel_screen.h"\n'+MARKER)
    # Only defer the known GT911 startup block. No pin, timing or flush changes.
    pattern=r'    if \(board_p4_touch_init\(\) != ESP_OK\) \{\n        ESP_LOGW\(TAG, [^\n]+\);\n    \}'
    s,n=re.subn(pattern,'    /* GT911 is initialized by main AFTER the first displayed frame. */',s)
    if n!=1:raise ValueError('Unexpected GT911 initialization block; nothing changed.')
    call='    board_p4_touch_read(&px, &py, &pressed);'
    if s.count(call)!=1:raise ValueError('Unexpected touch callback; nothing changed.')
    s=s.replace(call,'''    /* No read while main initializes GT911 outside the LVGL mutex. */
    if (!panel_screen_touch_ready()) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    board_p4_touch_read(&px, &py, &pressed);
    if (!panel_screen_filter_touch(pressed)) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }''')
    return s

def patch_glue(s):
    """Transform only the two exact, pinned portrait adapters, never substrings."""
    h = digest(s.encode())
    if h in GLUE_INPUTS.values(): return s
    if h not in GLUE_INPUTS:
        raise ValueError('Unknown or edited LVGL glue; preserved. No files changed.')
    result = _patch_glue_known(s)
    if digest(result.encode()) != GLUE_INPUTS[h]:
        raise ValueError('Screen adapter transformation verification failed.')
    return result


def verify_ini(s):
    """Read configuration, never rewrite it. Reject ambiguous overrides."""
    cfg=configparser.ConfigParser(interpolation=None,strict=True,inline_comment_prefixes=(';',))
    try: cfg.read_string(s)
    except configparser.Error as e: raise ValueError('Invalid platformio.ini: '+str(e)) from e
    if set(cfg.sections())!={'platformio','env:jc4880p4'} or cfg.defaults():
        raise ValueError('Expected one explicit env:jc4880p4; config inheritance unsupported.')
    env=cfg['env:jc4880p4']
    required={'platform':'https://github.com/pioarduino/platform-espressif32/releases/download/55.03.36-1/platform-espressif32.zip',
              'board':'jc4880p4','framework':'arduino','board_build.partitions':'default_16MB.csv',
              'board_build.f_cpu':'360000000L','board_build.flash_size':'16MB',
              'board_upload.flash_size':'16MB','board_build.flash_mode':'qio'}
    for key,value in required.items():
        if env.get(key,'').strip()!=value:
            raise ValueError('platformio.ini requires '+key+' = '+value+'; preserved unchanged. Configure and retry.')
    # The board JSON is hash-checked, so these are the effective defaults.
    # An explicit board_build override must preserve the same target.
    target={'board_build.mcu':'esp32p4','board_build.chip_variant':'esp32p4_es',
            'board_build.variant':'esp32p4'}
    for key,value in target.items():
        if env.get(key,value).strip()!=value:
            raise ValueError('Incompatible target override: '+key+'; expected '+value+'. platformio.ini preserved.')
    # Preserve both the recovered v0.3.1 range and the exact CI pin. Do not
    # silently permit a missing, duplicate, unpinned or incompatible LVGL.
    dependencies=[re.sub(r'\s+','',line) for line in env.get('lib_deps','').splitlines() if line.strip()]
    lvgl={'lvgl@~8.3.0','lvgl@8.3.11','lvgl/lvgl@~8.3.0','lvgl/lvgl@8.3.11'}
    if len(dependencies)!=2 or dependencies.count('symlink://../../')!=1 or sum(item in lvgl for item in dependencies)!=1:
        raise ValueError('Require the local BSP and LVGL ~8.3.0 or 8.3.11 in lib_deps; platformio.ini preserved.')
    for key in ('extends','extra_configs','extra_scripts','build_unflags'):
        if any(cfg.has_option(section,key) for section in cfg.sections()):
            raise ValueError('Unsupported configuration override: '+key)
    flags=env.get('build_flags','')
    definitions=re.findall(r'(?:-D|-U)\s*CORE_DEBUG_LEVEL\b(?:\s*=\s*[^\s;]+)?',flags)
    if definitions!=['-DCORE_DEBUG_LEVEL=0']:
        raise ValueError('Require exactly -DCORE_DEBUG_LEVEL=0; platformio.ini preserved.')
    if 'CORE_DEBUG_LEVEL' in env.get('build_src_flags',''):
        raise ValueError('Conflicting CORE_DEBUG_LEVEL override.')
    if cfg['platformio'].get('boards_dir','').strip()!='../../boards':
        raise ValueError('Unknown board definition location.')



def safe_path(root, relative):
    p = root / relative
    if Path(relative).is_absolute() or '..' in Path(relative).parts:
        raise ValueError('Unsafe relative path: '+str(relative))
    for candidate in (p, *p.parents):
        if candidate == root.parent: break
        if candidate.is_symlink(): raise ValueError('Symlink not supported: '+str(candidate))
    if p.exists() and not p.is_file(): raise ValueError('Expected regular file: '+str(p))
    return p


def preflight(root, package):
    prefix = 'examples/LvglUi/src/'
    src = root / prefix
    managed = {prefix+n for n in (*FILES, *RETIRED)}
    protected = {'examples/LvglUi/platformio.ini','boards/jc4880p4.json',
                 'src/board_p4.h',prefix+'lvgl_glue.c',prefix+'boiler_fonts.c'}
    for n in managed | protected: safe_path(root,n)
    for n in protected:
        if not (root/n).is_file(): raise ValueError('Missing required project file: '+n)
    for n in FILES:
        if not (package/'src'/n).is_file(): raise ValueError('Missing update payload: '+n)
    manifest = package/'SHA256SUMS.json'
    if manifest.exists():
        sums=json.loads(text(manifest))
        for n in FILES:
            if sums.get('src/'+n)!=sha(package/'src'/n): raise ValueError('Corrupt update payload: '+n)
    actual = {n:normalized_sha(src/n) for n in (*FILES,*RETIRED) if (src/n).exists()}
    current = {n:normalized_sha(package/'src'/n) for n in FILES}
    version = next((v for v,files in KNOWN_RELEASES.items() if actual==files),None)
    if actual==current: version='v05_current'
    if version is None:
        raise ValueError('Unsupported, edited, incomplete or mixed source set. Only complete known '
                         'v0.3.0/v0.3.1/v0.4.0 and the current v0.5.0 accepted. No files changed.')
    for n,expected in [('boards/jc4880p4.json',BOARD_HASH),('src/board_p4.h',HEADER_HASH),
                       (prefix+'boiler_fonts.c',FONT_HASH)]:
        if normalized_sha(root/n)!=expected: raise ValueError('Unknown or modified BSP/font file: '+n)
    glue=root/prefix/'lvgl_glue.c';ini=root/'examples/LvglUi/platformio.ini'
    glue_s=text(glue); ini_s=text(ini)
    verify_ini(ini_s)
    if version in ('v031','v04_before_fuel','v04_fuel','v04_audited','v05_current'):
        if digest(glue_s.encode()) not in GLUE_INPUTS.values():
            raise ValueError('Incomplete/mismatched installed screen adapter or CPU/debug configuration.')
    elif digest(glue_s.encode()) not in GLUE_INPUTS:
        raise ValueError('Mismatched baseline adapter/configuration. No files changed.')
    needed=set()
    for n in FILES: needed.update(required_codepoints(text(package/'src'/n)))
    verify_fonts(src/'boiler_fonts.c',needed)
    edits={prefix+n:(package/'src'/n).read_bytes() for n in FILES}
    # Keep already-correct glue/config byte-for-byte, including BOM and line endings.
    if digest(glue_s.encode()) in GLUE_INPUTS: edits[prefix+'lvgl_glue.c']=patch_glue(glue_s).encode()
    for n in RETIRED:
        if (src/n).exists(): edits[prefix+n]=None
    observed={n:(root/n).read_bytes() if (root/n).exists() else None for n in managed|protected}
    edits={n:b for n,b in edits.items() if observed[n]!=b}
    return src, {'version':version,'edits':edits,'observed':observed}


def atomic_write(path, content):
    temporary=path.with_name(path.name+'.standalone-update.tmp')
    created=False
    try:
        with temporary.open('xb') as out:
            created=True;out.write(content);out.flush();os.fsync(out.fileno())
        os.replace(temporary,path)
    finally:
        if created and temporary.exists(): temporary.unlink()


def current_bytes(root,n):
    p=safe_path(root,n)
    return p.read_bytes() if p.exists() else None


def transaction(root,plan,operation='update'):
    edits=plan['edits'];observed=plan['observed']
    for n,b in observed.items():
        if current_bytes(root,n)!=b: raise RuntimeError('Concurrent edit before update: '+n)
    if not edits:
        print('ALREADY CURRENT: v0.5.0. No changes made.' if operation=='update' else 'ALREADY RESTORED. No changes made.');return None
    backup_parent=root/'boiler_backups'
    if backup_parent.is_symlink(): raise ValueError('Symlink backup directory refused.')
    backup=backup_parent/(operation+'_'+datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
    backup.mkdir(parents=True,exist_ok=False)
    previous={n:observed[n] for n in edits}
    for n,b in previous.items():
        if b is not None:
            dest=backup/n;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(b)
    meta={'root':str(root.resolve()),'version':'v0.5.0','from':plan.get('version'),
          'state':'prepared','power_loss_atomic':False,'files':{
        n:{'old_sha256':None if previous[n] is None else digest(previous[n]),
           'new_sha256':None if b is None else digest(b)} for n,b in edits.items()}}
    def save(): atomic_write(backup/'manifest.json',(json.dumps(meta,indent=2)+'\n').encode())
    # A complete prepared manifest must exist before any source change.
    # Later status-write failures preserve that last valid manifest for restore.
    save();written=[]
    try:
        for n,b in edits.items():
            if current_bytes(root,n)!=previous[n]: raise RuntimeError('Concurrent edit: '+n)
            if b is None: (root/n).unlink()
            else: atomic_write(root/n,b)
            written.append(n)
        for n,b in observed.items():
            expected=edits.get(n,b)
            if current_bytes(root,n)!=expected: raise RuntimeError('Concurrent edit after update: '+n)
        meta['state']='applied';save()
    except Exception as failure:
        errors=[]
        for n in reversed(written):
            try:
                if current_bytes(root,n)!=edits[n]: raise RuntimeError('Concurrent edit; refusing overwrite: '+n)
                if previous[n] is None: (root/n).unlink(missing_ok=True)
                else: atomic_write(root/n,previous[n])
            except Exception as e: errors.append(str(e))
        meta['state']='rollback_failed' if errors else 'rolled_back'
        meta['errors']=errors
        status_failure=None
        try: save()
        except Exception as e: status_failure=e
        if errors or status_failure is not None:
            outcome='Rollback incomplete' if errors else 'Rollback completed; status could not be saved'
            details='; '.join(errors)
            if status_failure is not None: details+='; manifest status error: '+str(status_failure)
            raise RuntimeError(outcome+'; preserve backup '+str(backup)+
                               '. Original error: '+str(failure)+'. '+details) from failure
        raise
    print('SUCCESS: source '+operation+'. No device flashed. Backup: '+str(backup))
    return backup


def apply(src,root,package,plan):
    return transaction(root,plan)


def restore(root,backup):
    """Restore complete or interrupted update only when each byte is still known."""
    if backup.is_symlink(): raise ValueError('Symlink backup refused.')
    meta=json.loads(text(backup/'manifest.json'))
    if Path(meta['root']).resolve()!=root.resolve(): raise ValueError('Backup belongs to another project.')
    allowed={'examples/LvglUi/src/'+n for n in (*FILES,*RETIRED,'lvgl_glue.c')}
    allowed.add('examples/LvglUi/platformio.ini')
    edits={};observed={}
    for n,sums in meta['files'].items():
        if n not in allowed: raise ValueError('Unsafe backup path: '+n)
        current=current_bytes(root,n);h=None if current is None else digest(current)
        if h not in (sums['old_sha256'],sums['new_sha256']): raise ValueError('File changed since update: '+n)
        old=None
        if sums['old_sha256'] is not None:
            old=safe_path(backup,n).read_bytes()
            if digest(old)!=sums['old_sha256']: raise ValueError('Corrupt backup: '+n)
        observed[n]=current
        if current!=old: edits[n]=old
    return transaction(root,{'edits':edits,'observed':observed,'version':'restore'},'restore')


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('bsp',type=Path);ap.add_argument('--apply',action='store_true')
    ap.add_argument('--restore',type=Path,help='Restore known complete/interrupted update backup; requires --apply')
    a=ap.parse_args();root=a.bsp.expanduser().resolve();pkg=Path(__file__).resolve().parent
    try:
        if a.restore:
            if not a.apply: raise ValueError('Restore requires --apply.')
            restore(root,a.restore.absolute());return 0
        src,plan=preflight(root,pkg)
        print('CHECK OK: '+plan['version']+'; changed files: '+str(len(plan['edits'])))
        if a.apply: apply(src,root,pkg,plan)
        else: print('READ-ONLY CHECK. Add --apply to install. Nothing flashed.')
        return 0
    except (OSError,ValueError,RuntimeError,SyntaxError,KeyError) as e:
        print('ERROR: '+str(e),file=sys.stderr);return 1

if __name__=='__main__': raise SystemExit(main())
