"""Build the same heating static library for native tests and the ESP32-P4."""
from pathlib import Path
import argparse
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
TOOLCHAIN = '1.88.0'
# ESP-IDF's P4 C/C++ toolchain uses ILP32F. The bare-metal target supplies
# the same calling convention and requires no second HAL, startup or OS.
P4_TARGET = 'riscv32imafc-unknown-none-elf'


def build(platform):
    target_dir = ROOT / '.build/heating-rust/cargo'
    manifest = ROOT / 'rust/heating-core/Cargo.toml'
    target = ['--target', P4_TARGET] if platform == 'esp32p4' else []
    subprocess.run(['rustup', 'toolchain', 'install', TOOLCHAIN, '--profile', 'minimal', *target],
                   check=True)
    subprocess.run(['cargo', '+' + TOOLCHAIN, 'build', '--release', '--locked', '--offline',
                    '--manifest-path', str(manifest), '--target-dir', str(target_dir), *target],
                   check=True)
    output = ROOT / '.build/heating-rust' / platform / 'libheating_core.a'
    output.parent.mkdir(parents=True, exist_ok=True)
    build_dir = target_dir / P4_TARGET if target else target_dir
    shutil.copyfile(build_dir / 'release/libheating_core.a', output)
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('platform', choices=('host', 'esp32p4'))
    print(build(parser.parse_args().platform))
