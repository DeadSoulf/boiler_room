"""Prove the heating adapters call the Rust functions defined in the final ESP32-P4 ELF."""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
build_dir = Path(sys.argv[1])
elf = build_dir / 'firmware.elf'
archive = ROOT / '.build/heating-rust/esp32p4/libheating_core.a'
required_calls = {
    'panel_heating_runtime.cpp.o': ('boiler_heating_init', 'boiler_heating_tick'),
    'panel_heating_settings.cpp.o': ('boiler_heating_settings_encode', 'boiler_heating_settings_decode'),
}


def readelf(option, path):
    return subprocess.check_output(['readelf', '-W', option, str(path)], text=True)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


header = elf.read_bytes()[:52]
if (header[:6] != b'\x7fELF\x01\x01' or struct.unpack_from('<H', header, 18)[0] != 243
        or struct.unpack_from('<I', header, 36)[0] & 0x6 != 0x2):
    raise ValueError('Expected an ELF32 little-endian RISC-V executable with the ILP32F ABI')

# An archive on the command line alone is insufficient. Require actual call relocations
# from each production translation unit and definitions retained in the final firmware.
definitions = readelf('-s', elf)
for obj, symbols in required_calls.items():
    relocations = readelf('-r', build_dir / 'src' / obj)
    for symbol in symbols:
        if not re.search(r'\bR_RISCV_(?:CALL(?:_PLT)?|JAL)\s+[\da-fA-F]+\s+' + symbol + r'\b', relocations):
            raise ValueError(f'{obj} does not call {symbol}')
        if not re.search(r'\bFUNC\s+GLOBAL\s+\w+\s+\d+\s+' + symbol + r'$', definitions, re.MULTILINE):
            raise ValueError(f'Firmware does not contain a definition of {symbol}')

proof = {'target': 'riscv32imafc-unknown-none-elf', 'abi': 'ILP32F',
         'runtime_calls': required_calls['panel_heating_runtime.cpp.o'],
         'settings_calls': required_calls['panel_heating_settings.cpp.o'],
         'archive_sha256': sha256(archive), 'elf_sha256': sha256(elf)}
report = json.dumps(proof, indent=2) + '\n'
(ROOT / 'artifacts/heating-rust-link.json').write_text(report, encoding='utf-8')
print(report, end='')
