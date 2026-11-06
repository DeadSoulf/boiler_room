"""Run the real configuration module with deterministic NVS and mutex faults."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run_host(source: str) -> str:
    with tempfile.TemporaryDirectory(prefix="boiler-config-pin-") as temporary:
        directory = Path(temporary)
        for filename in ("Arduino.h", "Preferences.h", "freertos/FreeRTOS.h", "freertos/semphr.h"):
            header = directory / filename
            header.parent.mkdir(parents=True, exist_ok=True)
            header.write_text('#include "config_pin_host.h"\n', encoding="utf-8")
        cpp, binary = directory / "host.cpp", directory / "host"
        cpp.write_text(source, encoding="utf-8")
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-I", str(directory), "-I", str(ROOT / "tests"), "-I", str(ROOT / "src"),
                        str(cpp), "-o", str(binary)], check=True)
        return subprocess.check_output([str(binary)], text=True)


if __name__ == "__main__":
    print(run_host((ROOT / "tests/config_pin_tests.cpp").read_text(encoding="utf-8")), end="")
