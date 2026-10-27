"""Updater tests use isolated fixtures; no user project files are modified."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import shutil
import hashlib
import sys
from unittest.mock import patch

PACKAGE=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('updater',PACKAGE/'apply_update.py')
u=importlib.util.module_from_spec(spec);spec.loader.exec_module(u)
# Supply the previously downloaded application sources, not a font bundled here.
OLD=Path(sys.argv[1]) if len(sys.argv)>1 else PACKAGE/"tests/fixtures/v01"
if len(sys.argv)>1:del sys.argv[1]
class Tests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)/'bsp';self.src=self.root/'examples/LvglUi/src';self.src.mkdir(parents=True)
  (self.root/'boards').mkdir()
  shutil.copy2(OLD/'main.cpp',self.src/'main.cpp');shutil.copy2(PACKAGE/'src/boiler_fonts.c',self.src/'boiler_fonts.c')
  (self.src/'lvgl_glue.c').write_text('#define LV_HOR PANEL_NATIVE_W\n#define LV_VER PANEL_NATIVE_H\nvoid board_p4_flush_region();\n')
  (self.root/'examples/LvglUi/platformio.ini').write_text('[env:jc4880p4]\nboard=jc4880p4\n')
  (self.root/'boards/jc4880p4.json').write_text('{"build":{"chip_variant":"esp32p4_es"}}')
 def tearDown(self):self.temp.cleanup()
 def apply(self):
  src,protected=u.preflight(self.root,PACKAGE);u.apply(src,self.root,PACKAGE,protected);return protected
 def test_apply_and_idempotence(self):
  protected=self.apply();self.apply()
  for p,h in protected.items():self.assertEqual(u.sha(Path(p)),h)
  for n in u.FILES:self.assertEqual((self.src/n).read_bytes(),(PACKAGE/'src'/n).read_bytes())
  self.assertEqual(len(list((self.root/'boiler_backups').iterdir())),1)
 def test_reject_modified_main(self):
  with (self.src/'main.cpp').open('a') as f:f.write('\n// user edit\n')
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_reject_wrong_chip(self):
  (self.root/'boards/jc4880p4.json').write_text('{"build":{"chip_variant":"esp32p4"}}')
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_reject_unknown_new_file(self):
  (self.src/'panel_radio.cpp').write_text('// another user program')
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_accept_crlf_and_bom(self):
  p=self.src/'main.cpp';p.write_bytes(b'\xef\xbb\xbf'+p.read_bytes().replace(b'\n',b'\r\n'));self.apply()
 def test_read_only(self):
  before=(self.src/'main.cpp').read_bytes();u.preflight(self.root,PACKAGE)
  self.assertEqual((self.src/'main.cpp').read_bytes(),before);self.assertFalse((self.root/'boiler_backups').exists())
 def test_missing_font(self):
  (self.src/'boiler_fonts.c').unlink()
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_reject_missing_payload(self):
  with tempfile.TemporaryDirectory() as folder:
   package=Path(folder);shutil.copytree(PACKAGE/'src',package/'src');(package/'src/display_idle.h').unlink()
   with self.assertRaises(ValueError):u.preflight(self.root,package)
 def test_rollback_on_write_failure(self):
  before={n:(self.src/n).read_bytes() if (self.src/n).exists() else None for n in u.FILES}
  src,protected=u.preflight(self.root,PACKAGE);original=u.atomic_write;calls=0
  def fail_once(p,b):
   nonlocal calls
   calls+=1
   if calls==2:raise OSError('injected write failure')
   return original(p,b)
  with patch.object(u,'atomic_write',side_effect=fail_once):
   with self.assertRaises(OSError):u.apply(src,self.root,PACKAGE,protected)
  for n,data in before.items():self.assertEqual((src/n).read_bytes() if (src/n).exists() else None,data)
  for p,h in protected.items():self.assertEqual(u.sha(Path(p)),h)
 def test_upgrade_v03(self):
  for n in u.OLD_V03:shutil.copy2(PACKAGE/'tests/fixtures/v03'/n,self.src/n)
  self.apply()
  for n in u.FILES:self.assertEqual((self.src/n).read_bytes(),(PACKAGE/'src'/n).read_bytes())
 def test_reject_edited_v03(self):
  for n in u.OLD_V03:shutil.copy2(PACKAGE/'tests/fixtures/v03'/n,self.src/n)
  with (self.src/'panel_radio.cpp').open('a') as file:file.write('// edit')
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_reject_partial_v03(self):
  shutil.copy2(PACKAGE/'tests/fixtures/v03/main.cpp',self.src/'main.cpp')
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_reject_incomplete_current(self):
  self.apply();(self.src/'display_idle.h').unlink()
  with self.assertRaises(ValueError):u.preflight(self.root,PACKAGE)
 def test_backup_matches_original(self):
  before=(self.src/'main.cpp').read_bytes();self.apply()
  backups=list((self.root/'boiler_backups').glob('*/main.cpp'));self.assertEqual(backups[0].read_bytes(),before)
if __name__=='__main__':unittest.main(verbosity=2)
