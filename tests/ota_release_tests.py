"""Fault checks for the exact assets used by USB and GitHub OTA.

Synthetic images/partition tables only; never accesses a device or the network.
"""
from pathlib import Path
import copy
import hashlib
import importlib.util
import json
import shutil
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'ci'))
import ota_manifest as ota


class PublisherBodyTests(unittest.TestCase):
    def test_json_manifest_is_uploaded_as_exact_bytes(self):
        spec = importlib.util.spec_from_file_location('checked_publisher', ROOT / 'scripts/publish_github_release.py')
        publisher = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(publisher)
        manifest = b'{"schema": 1, "version": "v0.10.0"}\n'
        self.assertIs(publisher.request_body(manifest, upload=True), manifest)
        for content in (b'PK\x03\x04\x00\xff', b'0123456789  firmware.bin\n'):
            self.assertIs(publisher.request_body(content, upload=True), content)
        self.assertEqual(json.loads(publisher.request_body({'draft': True})), {'draft': True})
        self.assertIsNone(publisher.request_body(None))
        with self.assertRaises(TypeError):
            publisher.request_body({'schema': 1}, upload=True)


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for folder in ('src', 'scripts', 'artifacts'):
            (self.root / folder).mkdir()
        shutil.copy2(ROOT / 'scripts/flash_panel.py', self.root / 'scripts/flash_panel.py')
        self.image = bytearray(65536)
        self.image[0] = 0xe9
        struct.pack_into('<H', self.image, 12, 18)
        struct.pack_into('<HH', self.image, 15, 0, 65535)
        self.image = bytes(self.image)
        self.boot = self.image[:256]
        (self.root / 'src/panel_update_build.h').write_text(
            f'#define BOILER_BOOTLOADER_SHA256 "{hashlib.sha256(self.boot).hexdigest()}"\n'
            f'#define BOILER_BOOTLOADER_BYTES {len(self.boot)}\n', encoding='ascii')
        self.checks = ota._flash_checks(self.root)
        records = b''.join(struct.pack('<HBBII16sI', 0x50aa, kind, subtype, offset,
                                     size, name.encode(), flags)
                           for kind, subtype, offset, size, name, flags
                           in self.checks.EXPECTED_PARTITIONS)
        self.table = records + b'\xeb\xeb' + b'\xff' * 14 + hashlib.md5(records).digest()
        self.tag = 'v0.10.0'
        self.commit = 'e' * 40

    def write(self, **change):
        args = dict(root=self.root, version=self.tag, commit=self.commit,
                    firmware=self.image, bootloader=self.boot, partitions=self.table)
        args.update(change)
        return ota.write_assets(**args)

    def validate(self):
        return ota.validate_assets(self.root, self.tag, self.commit, self.image, self.boot, self.table)

    def test_complete_assets_identify_the_exact_build(self):
        manifest = self.write()
        self.assertEqual(self.validate(), manifest)
        self.assertEqual(manifest['source_commit'], self.commit)
        self.assertEqual(manifest['firmware']['size'], len(self.image))
        self.assertEqual(manifest['firmware']['sha256'], hashlib.sha256(self.image).hexdigest())
        self.assertEqual(manifest['firmware']['url'],
                         'https://github.com/DeadSoulf/boiler_room/releases/download/'
                         'v0.10.0/BoilerPanel-v0.10.0-firmware.bin')
        self.assertEqual((self.root / 'artifacts' / manifest['firmware']['name']).read_bytes(), self.image)

    def test_bad_version_or_nonimmutable_source_is_refused(self):
        for version in ('v01.2.3', 'v1.2.3-rc1', '1.2.3', 'v1.2.3/x', 'v4294967296.0.0'):
            with self.subTest(version=version), self.assertRaises(ValueError):
                self.write(version=version)
        for commit in ('main', 'e' * 7, 'E' * 40, 'g' * 40):
            with self.subTest(commit=commit), self.assertRaises(ValueError):
                self.write(commit=commit)

    def test_wrong_image_chip_revision_or_slot_size_is_refused(self):
        wrong_chip = bytearray(self.image); struct.pack_into('<H', wrong_chip, 12, 9)
        wrong_revision = bytearray(self.image); struct.pack_into('<HH', wrong_revision, 15, 300, 399)
        for image in (bytes(wrong_chip), bytes(wrong_revision), self.image[:200],
                      self.image + b'\0' * self.checks.APP_BYTES):
            with self.subTest(size=len(image)), self.assertRaises((ValueError, self.checks.Refusal, RuntimeError)):
                self.write(firmware=image)

    def test_unknown_bootloader_and_modified_partition_table_are_refused(self):
        with self.assertRaises(ValueError):
            self.write(bootloader=self.boot[:-1] + b'x')
        table = bytearray(self.table); table[5] ^= 1
        with self.assertRaises(RuntimeError):
            self.write(partitions=bytes(table))
        records = bytearray(self.table[:-32]); records[8] ^= 1
        wrong_layout = bytes(records) + b'\xeb\xeb' + b'\xff' * 14 + hashlib.md5(records).digest()
        with self.assertRaises(ValueError):
            self.write(partitions=wrong_layout)

    def test_stale_manifest_substituted_url_and_extra_fields_are_refused(self):
        original = self.write()
        variants = []
        for key, value in [('source_commit', 'a' * 40), ('version', 'v0.9.1'),
                           ('schema', True), ('flash_bytes', 16777216.0), ('extra', 'unknown')]:
            altered = copy.deepcopy(original); altered[key] = value; variants.append(altered)
        for key, value in [('url', 'https://example.invalid/firmware.bin'),
                           ('size', len(self.image) - 1), ('sha256', '0' * 64)]:
            altered = copy.deepcopy(original); altered['firmware'][key] = value; variants.append(altered)
        for value in variants:
            with self.subTest(value=value):
                (self.root / 'artifacts' / ota.MANIFEST_NAME).write_text(json.dumps(value))
                with self.assertRaises(ValueError):
                    self.validate()

    def test_duplicate_fields_bom_and_changed_raw_image_are_refused(self):
        self.write()
        path = self.root / 'artifacts' / ota.MANIFEST_NAME
        original = path.read_bytes()
        for data in (b'\xef\xbb\xbf' + original,
                     original.replace(b'"schema": 1,', b'"schema": 1, "schema": 1,')):
            path.write_bytes(data)
            with self.assertRaises(ValueError):
                self.validate()
        manifest = self.write()
        (self.root / 'artifacts' / manifest['firmware']['name']).write_bytes(self.image[:-1] + b'x')
        with self.assertRaises(ValueError):
            self.validate()


if __name__ == '__main__':
    unittest.main(verbosity=2)
