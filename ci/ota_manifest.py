"""Build and validate the public, version-bound GitHub OTA release assets.

No network, credentials or device access. Uses the same image/partition checks
as the checked USB installer. The manifest is authenticated by GitHub HTTPS;
it is not a firmware signature or a replacement for ESP secure boot.
"""
from pathlib import Path
import hashlib
import importlib.util
import json
import re
import sys

REPOSITORY = 'DeadSoulf/boiler_room'
MANIFEST_NAME = 'boiler-panel-ota.json'
LAYOUT = 'guition-16mb-ota-640000-v1'


def _require(ok, message):
    if not ok:
        raise ValueError(message)


def _flash_checks(root):
    name = '_boiler_ota_package_checks'
    spec = importlib.util.spec_from_file_location(name, root / 'scripts/flash_panel.py')
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def bootloader_pin(root):
    header = (root / 'src/panel_update_build.h').read_text(encoding='ascii')
    digest = re.search(r'^#define BOILER_BOOTLOADER_SHA256 "([0-9a-f]{64})"$', header, re.M)
    size = re.search(r'^#define BOILER_BOOTLOADER_BYTES ([0-9]+)$', header, re.M)
    _require(digest is not None and size is not None, 'Missing checked bootloader pin')
    return int(size.group(1)), digest.group(1)


def make_manifest(root, version, commit, firmware, bootloader, partitions):
    _require(re.fullmatch(r'v(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)', version),
             'OTA requires a stable canonical version')
    _require(len(version) < 24, 'Version does not fit the device protocol')
    _require(all(int(n) <= 0xffffffff for n in version[1:].split('.')),
             'Version component exceeds the device protocol')
    _require(re.fullmatch(r'[0-9a-f]{40}', commit), 'A full immutable source commit is required')
    checks = _flash_checks(root)
    _require(65536 <= len(firmware) <= checks.APP_BYTES, 'Firmware does not fit an OTA slot')
    checks.image_revision(firmware)
    checks.image_revision(bootloader)
    _require(checks.partition_records(partitions) == checks.EXPECTED_PARTITIONS,
             'OTA requires the checked two-slot 16MB partition layout')
    size, digest = bootloader_pin(root)
    _require(len(bootloader) == size and hashlib.sha256(bootloader).hexdigest() == digest,
             'Bootloader differs from the rollback-verified build pin')
    name = f'BoilerPanel-{version}-firmware.bin'
    return {
        'schema': 1,
        'project': REPOSITORY,
        'board': 'jc4880p443',
        'chip': 'esp32p4',
        'chip_revision_min': 103,
        'chip_revision_max': 103,
        'flash_bytes': 16777216,
        'partition_layout': LAYOUT,
        'version': version,
        'source_commit': commit,
        'firmware': {
            'name': name,
            'size': len(firmware),
            'sha256': hashlib.sha256(firmware).hexdigest(),
            'url': f'https://github.com/{REPOSITORY}/releases/download/{version}/{name}',
        },
    }


def write_assets(root, version, commit, firmware, bootloader, partitions):
    manifest = make_manifest(root, version, commit, firmware, bootloader, partitions)
    artifacts = root / 'artifacts'
    (artifacts / manifest['firmware']['name']).write_bytes(firmware)
    (artifacts / MANIFEST_NAME).write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    return manifest


def validate_assets(root, version, commit, firmware, bootloader, partitions):
    expected = make_manifest(root, version, commit, firmware, bootloader, partitions)
    artifacts = root / 'artifacts'
    raw = (artifacts / MANIFEST_NAME).read_bytes()
    _require(len(raw) <= 4096, 'OTA manifest exceeds the device bound')
    def unique_object(pairs):
        value = dict(pairs)
        _require(len(value) == len(pairs), 'Duplicate OTA manifest field')
        return value
    _require(not raw.startswith(b'\xef\xbb\xbf'), 'OTA manifest must be UTF-8 without BOM')
    actual = json.loads(raw, object_pairs_hook=unique_object)
    _require(actual == expected, 'OTA manifest does not describe this exact checked build')
    _require(all(type(actual[k]) is int for k in
                 ('schema', 'chip_revision_min', 'chip_revision_max', 'flash_bytes'))
             and type(actual['firmware']['size']) is int,
             'OTA numeric fields must be integers')
    _require((artifacts / expected['firmware']['name']).read_bytes() == firmware,
             'Public OTA image differs from the checked COM3 package')
    return expected
