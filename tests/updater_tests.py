"""Isolated exact recovered releases + pinned real BSP, never a user project."""
import importlib.util
from pathlib import Path
import tempfile, unittest, shutil, subprocess, sys, json
from unittest.mock import patch
PACKAGE=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('updater',PACKAGE/'apply_update.py')
u=importlib.util.module_from_spec(spec);spec.loader.exec_module(u)
F=PACKAGE/'tests/fixtures'

class Tests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)/'bsp'
  self.src=self.root/'examples/LvglUi/src';self.src.mkdir(parents=True)
  (self.root/'boards').mkdir();(self.root/'src').mkdir()
  for name,dest in [('jc4880p4.json','boards/jc4880p4.json'),('board_p4.h','src/board_p4.h'),('screen031.ini','examples/LvglUi/platformio.ini')]:shutil.copy2(F/'bsp'/name,self.root/dest)
  shutil.copy2(PACKAGE/'src/boiler_fonts.c',self.src/'boiler_fonts.c')
  self.install('v031')
 def tearDown(self):self.temp.cleanup()
 def install(self,version):
  for n in (*u.FILES,*u.RETIRED):(self.src/n).unlink(missing_ok=True)
  for p in (F/version).iterdir():shutil.copy2(p,self.src/p.name)
  shutil.copy2(F/'bsp'/('screen031.c' if version=='v031' else 'portrait.c'),self.src/'lvgl_glue.c')
 def snapshot(self):return {str(p.relative_to(self.root)):p.read_bytes() for p in self.root.rglob('*') if p.is_file() and 'boiler_backups' not in p.parts}
 def apply(self):
  src,plan=u.preflight(self.root,PACKAGE);return u.apply(src,self.root,PACKAGE,plan)
 def test_v031_and_idempotence(self):
  ini=(self.src.parent/'platformio.ini').read_bytes();glue=(self.src/'lvgl_glue.c').read_bytes()
  backup=self.apply();self.assertIsNone(self.apply())
  self.assertEqual((self.src.parent/'platformio.ini').read_bytes(),ini)
  self.assertEqual((self.src/'lvgl_glue.c').read_bytes(),glue)
  self.assertEqual(len(list((self.root/'boiler_backups').iterdir())),1)
  for n in u.FILES:self.assertEqual((self.src/n).read_bytes(),(PACKAGE/'src'/n).read_bytes())
  for n in json.loads((backup/'manifest.json').read_text())['files']:
   self.assertTrue((backup/n).is_file())
 def test_v03_safe_transform_and_backup(self):
  self.install('v03');before=self.snapshot();backup=self.apply()
  self.assertEqual((self.src/'lvgl_glue.c').read_bytes(),(F/'bsp/screen031.c').read_bytes())
  for n,meta in json.loads((backup/'manifest.json').read_text())['files'].items():
   if meta['old_sha256']:self.assertEqual((backup/n).read_bytes(),before[n])
  u.restore(self.root,backup);self.assertEqual(before,self.snapshot())
 def test_earlier_v04_retires_only_known_idle(self):
  self.install('v04_early');old=(self.src/'display_idle.h').read_bytes();backup=self.apply()
  self.assertFalse((self.src/'display_idle.h').exists())
  self.assertEqual((backup/'examples/LvglUi/src/display_idle.h').read_bytes(),old)
  u.restore(self.root,backup);self.assertEqual((self.src/'display_idle.h').read_bytes(),old)
 def test_default_cli_read_only(self):
  before=self.snapshot();result=subprocess.run([sys.executable,str(PACKAGE/'apply_update.py'),str(self.root)],capture_output=True,text=True)
  self.assertEqual(result.returncode,0,result.stderr);self.assertIn('READ-ONLY',result.stdout)
  self.assertEqual(before,self.snapshot());self.assertFalse((self.root/'boiler_backups').exists())
 def test_modified_each_owned_source_rejected(self):
  for n in u.FILES:
   with self.subTest(file=n):
    old=(self.src/n).read_bytes();(self.src/n).write_bytes(old+b'\n// edit\n')
    with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
    (self.src/n).write_bytes(old)
 def test_partial_source_set_rejected(self):
  (self.src/'panel_screen.cpp').unlink()
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_mixed_source_set_rejected(self):
  shutil.copy2(PACKAGE/'src/main.cpp',self.src/'main.cpp')
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_unknown_retired_file_rejected(self):
  (self.src/'display_idle.h').write_text('// user file')
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_modified_glue_rejected_even_marker_intact(self):
  p=self.src/'lvgl_glue.c';p.write_text(p.read_text().replace('s_disp_drv.full_refresh = 1','s_disp_drv.full_refresh = 0'))
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_unpatched_v031_glue_rejected(self):
  shutil.copy2(F/'bsp/portrait.c',self.src/'lvgl_glue.c')
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_fonts_mismatch_rejected(self):
  p=self.src/'boiler_fonts.c';p.write_bytes(p.read_bytes()+b'// changed')
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_config_mismatch_rejected(self):
  p=self.src.parent/'platformio.ini';s=p.read_text()
  for old,new in [('360000000L','400000000L'),('-DCORE_DEBUG_LEVEL=0','-DCORE_DEBUG_LEVEL=3'),('default_16MB.csv','other.csv'),('55.03.36-1','55.03.37-1')]:
   with self.subTest(old=old):
    p.write_text(s.replace(old,new))
    with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
  p.write_text(s)
 def test_config_noncritical_customization_preserved(self):
  p=self.src.parent/'platformio.ini';s=p.read_text().replace('monitor_speed = 115200','monitor_speed = 9600');p.write_text(s)
  self.apply();self.assertEqual(p.read_text(),s)
 def test_board_mismatch_rejected(self):
  p=self.root/'boards/jc4880p4.json';p.write_text(p.read_text().replace('esp32p4_es','esp32p4'))
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_crlf_bom_exact_semantics(self):
  for p in self.src.iterdir():p.write_bytes(b'\xef\xbb\xbf'+p.read_bytes().replace(b'\n',b'\r\n'))
  glue=(self.src/'lvgl_glue.c').read_bytes();self.apply();self.assertEqual((self.src/'lvgl_glue.c').read_bytes(),glue)
 def test_missing_payload_rejected(self):
  with tempfile.TemporaryDirectory() as tmp:
   pkg=Path(tmp);shutil.copytree(PACKAGE/'src',pkg/'src');(pkg/'src/screen_state.h').unlink()
   with self.assertRaises(ValueError):u.preflight(self.root,pkg)
 def test_rollback_write_failure_all_positions(self):
  for failed in (1,2,4,7,8):
   with self.subTest(failed=failed):
    self.install('v03');before=self.snapshot();src,plan=u.preflight(self.root,PACKAGE);original=u.atomic_write;calls=0
    def fail_once(p,b):
     nonlocal calls
     calls+=1
     if calls==failed:raise OSError('injected write failure')
     original(p,b)
    with patch.object(u,'atomic_write',side_effect=fail_once):
     with self.assertRaises(OSError):u.apply(src,self.root,PACKAGE,plan)
    self.assertEqual(self.snapshot(),before)
 def test_concurrent_change_before_apply_refused(self):
  src,plan=u.preflight(self.root,PACKAGE);(self.src/'main.cpp').write_text('// concurrent edit')
  with self.assertRaises(RuntimeError):u.apply(src,self.root,PACKAGE,plan)
  self.assertEqual((self.src/'main.cpp').read_text(),'// concurrent edit')
 def test_interrupted_apply_refused_and_explicit_restore(self):
  before=self.snapshot();src,plan=u.preflight(self.root,PACKAGE);original=u.atomic_write;calls=0
  def interrupt(p,b):
   nonlocal calls
   calls+=1
   if calls==3:raise KeyboardInterrupt()
   original(p,b)
  with patch.object(u,'atomic_write',side_effect=interrupt):
   with self.assertRaises(KeyboardInterrupt):u.apply(src,self.root,PACKAGE,plan)
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
  backup=next((self.root/'boiler_backups').iterdir());u.restore(self.root,backup)
  self.assertEqual(self.snapshot(),before)
 def test_restore_refuses_post_update_edit(self):
  backup=self.apply();(self.src/'main.cpp').write_text('// new edit')
  with self.assertRaises(ValueError):u.restore(self.root,backup)
 def test_restore_refuses_corrupted_backup(self):
  backup=self.apply();(backup/'examples/LvglUi/src/main.cpp').write_text('// corrupted')
  with self.assertRaises(ValueError):u.restore(self.root,backup)
 def test_symlink_owned_file_rejected(self):
  p=self.src/'main.cpp';p.unlink();p.symlink_to(F/'v031/main.cpp')
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_current_missing_screen_file_rejected(self):
  self.apply();(self.src/'panel_screen.cpp').unlink()
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_config_duplicate_or_inheritance_rejected(self):
  p=self.src.parent/'platformio.ini';original=p.read_text()
  for extra in ('\nboard_build.f_cpu = 400000000L\n','\n[env:other]\nboard = other\n','\nextra_scripts = alter.py\n'):
   p.write_text(original+extra)
   with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_backup_path_injection_rejected(self):
  backup=self.apply();p=backup/'manifest.json';m=json.loads(p.read_text());m['files']['../outside']={'old_sha256':None,'new_sha256':None};p.write_text(json.dumps(m))
  with self.assertRaises(ValueError):u.restore(self.root,backup)
 def test_payload_manifest_corruption_rejected(self):
  with tempfile.TemporaryDirectory() as tmp:
   pkg=Path(tmp);shutil.copytree(PACKAGE/'src',pkg/'src');(pkg/'SHA256SUMS.json').write_text('{}')
   with self.assertRaises(ValueError):u.preflight(self.root,pkg)
 def test_symlink_source_directory_rejected(self):
  other=self.root/'moved';self.src.rename(other);self.src.symlink_to(other,target_is_directory=True)
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_protected_concurrent_edit_rolls_back_sources_only(self):
  before=self.snapshot();src,plan=u.preflight(self.root,PACKAGE);original=u.atomic_write;calls=0
  ini=self.src.parent/'platformio.ini'
  def change_protected(p,b):
   nonlocal calls
   calls+=1;original(p,b)
   if calls==2:ini.write_text(ini.read_text()+'\n; concurrent user note\n')
  with patch.object(u,'atomic_write',side_effect=change_protected):
   with self.assertRaises(RuntimeError):u.apply(src,self.root,PACKAGE,plan)
  after=self.snapshot();before.pop('examples/LvglUi/platformio.ini');after.pop('examples/LvglUi/platformio.ini')
  self.assertEqual(before,after);self.assertIn('concurrent user note',ini.read_text())
 def test_ci_portrait_exact_transform(self):
  self.assertEqual(u.patch_glue((F/'bsp/ci_portrait.c').read_text()),(F/'bsp/ci_screen031.c').read_text())
  s=(F/'bsp/screen031.c').read_text();self.assertEqual(u.patch_glue(s),s)
 def test_v01_v02_unsupported(self):
  self.install('v03');shutil.copy2(F/'v01/main.cpp',self.src/'main.cpp')
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)

if __name__=='__main__':unittest.main(verbosity=2)
