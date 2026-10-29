"""Package an already built and verified application for the user's COM3 panel.

This script does not access serial ports. Run only after verify_binary.py.
The ZIP is part of the firmware CI artifact and inherits the repository access.
"""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import zipfile

def windows_launcher_bytes(data: bytes) -> bytes:
    """Keep cmd.exe launchers ASCII with CRLF, even after a Linux checkout."""
    text = data.decode("ascii")
    return text.replace("\r\n", "\n").replace("\r", "\n").replace("\n", "\r\n").encode("ascii")


def main():
    root = Path(__file__).resolve().parents[1]
    artifacts = root / 'artifacts'
    version = re.search(r'VERSION\s*=\s*"(v\d+\.\d+\.\d+)"',
                        (root / 'src/panel_types.h').read_text()).group(1)
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    payload = {name: (artifacts / name).read_bytes() for name in
               ('firmware.bin', 'bootloader.bin', 'partitions.bin', 'binary-verification.json')}
    report = json.loads(payload['binary-verification.json'])
    assert report['image_chip_id'] == 18 and report['image_bytes'] == len(payload['firmware.bin'])
    for name in ('scripts/flash_panel.py', 'scripts/flash_COM3.cmd', 'docs/flash-panel.md'):
        payload[name] = (root / name).read_bytes()
    payload['scripts/flash_COM3.cmd'] = windows_launcher_bytes(payload['scripts/flash_COM3.cmd'])
    for path in sorted((root / 'licenses').rglob('*')):
        if path.is_file():
            payload[path.relative_to(root).as_posix()] = path.read_bytes()
    payload['SOURCE.txt'] = (f'Boiler Panel {version}\nGitHub commit: {commit}\n'
                             'Target: GUITION JC4880P443 / ESP32-P4 ES v1.3 / 16MB\n'
                             'Equipment controls: LOCAL DEMONSTRATION ONLY\n'
                             'Hardware upload and display test: not performed by CI\n').encode()
    payload['START_HERE.txt'] = (
        f'Панель котельной {version} / COM3\n\n'
        '1. Распакуйте весь каталог ZIP. Закройте Serial Monitor и Arduino IDE.\n'
        '2. Прочитайте docs/flash-panel.md. Нужны Python 3.10+ и esptool 5.1.0.\n'
        '3. Запустите scripts/flash_COM3.cmd. Проверки и резервирование выполняются перед записью.\n'
        '4. После успешной записи проверьте версию в Сервисе и экраны Управления.\n\n'
        'Команды оборудования являются локальной демонстрацией. Реальные выходы не назначены.\n'
        'NVS, загрузчик, разделы и C6 не записываются. При несовпадении проверки запись отменяется.\n'
        'Резервные копии сохраняйте на своём компьютере; они могут содержать приватные данные.\n'
    ).encode('utf-8')
    hashes = {name: hashlib.sha256(data).hexdigest() for name, data in sorted(payload.items())}
    payload['SHA256SUMS.json'] = (json.dumps(hashes, ensure_ascii=False, indent=2) + '\n').encode()
    name = f'BoilerPanel-{version}-COM3'
    output = artifacts / (name + '.zip')
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path, data in sorted(payload.items()):
            info = zipfile.ZipInfo(name + '/' + path, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None
        for path, data in payload.items():
            assert archive.read(name + '/' + path) == data
    print(f'Created {output.name}: {len(payload)} files, SHA256={hashlib.sha256(output.read_bytes()).hexdigest()}')


if __name__ == "__main__":
    main()
