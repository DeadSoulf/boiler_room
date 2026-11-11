"""Compile the firmware heating-settings adapter and verify persisted bytes in a fresh process."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("archive", nargs="?", type=Path, help="prebuilt host libheating_core.a")
archive = parser.parse_args().archive
if archive is None:
    subprocess.run([sys.executable, str(root / "ci/build_heating_rust.py"), "host"], check=True)
    archive = root / ".build/heating-rust/host/libheating_core.a"

# Captured by running example() against the C++ writer shipped in v0.11.0.
legacy_record = bytes.fromhex(
    "01 01 54 48 01 00 00 00 b0 30 01 00 b9 00 d7 00 "
    "e1 00 62 02 b3 01 5e 01 95 01 1c 02 3c 00 14 00 "
    "0e 0d 0c 0b 0a 09 08 07 06 05 04 03 02 01 02 02 "
    "01 00 00 00 d9 88 3d 34"
)
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
                    str(root / "tests/heating_settings_tests.cpp"), str(archive.resolve()),
                    "-o", str(binary)], check=True)
    record = temp / "nvs.bin"
    subprocess.run([binary, "write", record], check=True)
    if record.read_bytes() != legacy_record:
        raise ValueError("Rust wrote a different NVS record from firmware v0.11.0")
    record.write_bytes(legacy_record)
    subprocess.run([binary, "restart", record], check=True)
print("PASS: v0.11.0 NVS bytes, legacy read, independent readback, rollback, CAS and reboot")
