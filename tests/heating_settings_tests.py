"""Compile the firmware heating-settings adapter and verify persisted bytes in a fresh process."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="panel-heating-settings-") as directory:
    temp = Path(directory)
    for name in ("nvs.h", "freertos/FreeRTOS.h", "freertos/semphr.h"):
        header = temp / name
        header.parent.mkdir(parents=True, exist_ok=True)
        header.write_text('#include "heating_settings_stubs.h"\n')
    binary = temp / "heating-settings"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-pthread", "-Wall", "-Wextra",
                    "-Werror", "-pedantic", "-I", str(temp), "-I", str(root / "tests"),
                    "-I", str(root / "src"), str(root / "src/panel_heating_settings.cpp"),
                    str(root / "tests/heating_settings_tests.cpp"), "-o", str(binary)], check=True)
    for phase in ("write", "restart"):
        subprocess.run([binary, phase, temp / "nvs.bin"], check=True)
print("PASS: heating configuration, independent NVS readback, rollback, CAS and reboot")
