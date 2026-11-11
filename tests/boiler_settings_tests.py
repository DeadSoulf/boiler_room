"""Exercise the firmware's DHW settings storage, without hardware or heater outputs."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="boiler-settings-") as tmp:
    work = Path(tmp)
    for name in ("nvs.h", "freertos/FreeRTOS.h", "freertos/semphr.h"):
        alias = work / name
        alias.parent.mkdir(parents=True, exist_ok=True)
        alias.write_text('#include "fake.h"\n')
    binary = work / "test"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-pthread", "-Wall", "-Wextra", "-Werror",
                    "-I", str(work), "-I", str(root / "tests/boiler_settings_stubs"), "-I", str(root / "src"),
                    str(root / "src/panel_boiler.cpp"), str(root / "tests/boiler_settings_tests.cpp"),
                    "-o", str(binary)], check=True)
    for phase in ("write", "reboot"):
        subprocess.run([binary, phase, work / "stored-u64.bin"], check=True)
print("PASS: empty/corrupt/unreadable DHW settings, atomic pair and readback faults, conflicts, fresh-process restart")
