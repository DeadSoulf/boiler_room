#!/usr/bin/env python3
"""Test the packaged CMD launcher without importing esptool or opening a port.

Format checks run everywhere. Windows cases execute the real cmd.exe against a
temporary package containing a harmless Python recorder instead of the flasher.
"""
from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import venv


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("package_flash", ROOT / "ci" / "package_flash.py")
packaging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packaging)
LAUNCHER = ROOT / "scripts" / "flash_COM3.cmd"


class LauncherFormatTests(unittest.TestCase):
    def test_linux_checkout_is_packaged_as_crlf(self):
        linux = b"@echo off\nif exist file goto ready\n:ready\nexit /b 0\n"
        windows = linux.replace(b"\n", b"\r\n")
        self.assertEqual(packaging.windows_launcher_bytes(linux), windows)
        self.assertEqual(packaging.windows_launcher_bytes(windows), windows)

    def test_non_ascii_launcher_is_rejected(self):
        with self.assertRaises(UnicodeError):
            packaging.windows_launcher_bytes("@echo off\necho Готово\n".encode("utf-8"))

    def test_actual_packaged_launcher_is_ascii_crlf_without_bom(self):
        raw = packaging.windows_launcher_bytes(LAUNCHER.read_bytes())
        raw.decode("ascii")
        self.assertTrue(raw.startswith(b"@echo off\r\n"))
        self.assertTrue(raw.endswith(b"\r\n"))
        self.assertGreater(raw.count(b"\r\n"), 1)
        self.assertNotIn(b"\n", raw.replace(b"\r\n", b""))
        self.assertNotIn(b"\r", raw.replace(b"\r\n", b""))


@unittest.skipUnless(os.name == "nt", "Requires the actual Windows cmd.exe")
class WindowsLauncherTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="boiler-launcher-")
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.base = Path(cls.temporary.name)
        cls.package = cls.base / "Панель ! & (COM3 test)"
        cls.scripts = cls.package / "scripts"
        cls.scripts.mkdir(parents=True)
        cls.launcher = cls.scripts / "flash_COM3.cmd"
        cls.launcher.write_bytes(packaging.windows_launcher_bytes(LAUNCHER.read_bytes()))
        cls.recorder = cls.scripts / "flash_panel.py"
        cls.recorder.write_text(
            "import json, os, sys\n"
            "from pathlib import Path\n"
            "data = {'argv': sys.argv, 'executable': sys.executable}\n"
            "Path(os.environ['BOILER_TEST_CAPTURE']).write_text(\n"
            "    json.dumps(data, ensure_ascii=False), encoding='utf-8')\n"
            "print('BOILER_STUB_RAN')\n"
            "raise SystemExit(int(os.environ['BOILER_TEST_EXIT']))\n",
            encoding="utf-8",
        )
        # Both interpreters are real, isolated Python launchers; no pip packages.
        cls.selected_home = cls.base / "Python тест ! & (selected)"
        venv.EnvBuilder(with_pip=False).create(cls.selected_home)
        venv.EnvBuilder(with_pip=False).create(cls.package / ".venv")
        cls.selected_python = cls.selected_home / "Scripts" / "python.exe"
        cls.local_python = cls.package / ".venv" / "Scripts" / "python.exe"
        cls.capture = cls.base / "capture.json"
        cls.comspec = Path(os.environ["SystemRoot"]) / "System32" / "cmd.exe"

    def setUp(self):
        self.capture.unlink(missing_ok=True)

    def run_launcher(self, selected=None, exit_code=0):
        env = os.environ.copy()
        # Tests must not inherit a real upload selection or Python customization.
        for key in ("BOILER_FLASH_PYTHON", "PYTHONHOME", "PYTHONPATH"):
            env.pop(key, None)
        env["BOILER_TEST_CAPTURE"] = str(self.capture)
        env["BOILER_TEST_EXIT"] = str(exit_code)
        if selected is not None:
            env["BOILER_FLASH_PYTHON"] = str(selected)
        # Passing one string preserves cmd's required outer quotes around a
        # quoted batch path. shell=False avoids an additional command shell.
        command = f'"{self.comspec}" /d /s /v:off /c ""{self.launcher}""'
        return subprocess.run(
            command, shell=False, cwd=self.base, env=env, input="\n",
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, encoding="utf-8", errors="replace", timeout=30,
        )

    def assert_recorder(self, result, interpreter, exit_code):
        self.assertEqual(result.returncode, exit_code, result.stdout)
        self.assertEqual(result.stdout.count("BOILER_STUB_RAN"), 1, result.stdout)
        self.assertTrue(self.capture.is_file(), result.stdout)
        recorded = json.loads(self.capture.read_text(encoding="utf-8"))
        argv = recorded["argv"]
        self.assertEqual(len(argv), 6, recorded)
        self.assertEqual(Path(argv[0]).resolve(), self.recorder.resolve())
        self.assertEqual(argv[1], "--package")
        self.assertEqual(Path(argv[2]).resolve(), self.package.resolve())
        self.assertEqual(argv[3:], ["--port", "COM3", "--write"])
        self.assertEqual(Path(recorded["executable"]).resolve(), interpreter.resolve())

    def test_selected_python_has_priority_over_existing_venv(self):
        result = self.run_launcher(selected=self.selected_python)
        self.assert_recorder(result, self.selected_python, 0)

    def test_local_venv_is_used_without_explicit_python(self):
        result = self.run_launcher()
        self.assert_recorder(result, self.local_python, 0)

    def test_python_failure_is_returned_unchanged(self):
        result = self.run_launcher(selected=self.selected_python, exit_code=17)
        self.assert_recorder(result, self.selected_python, 17)

    def test_missing_selected_python_refuses_without_running_stub(self):
        result = self.run_launcher(selected=self.base / "Missing ! & (Python)" / "python.exe")
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertFalse(self.capture.exists(), result.stdout)
        self.assertNotIn("BOILER_STUB_RAN", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
