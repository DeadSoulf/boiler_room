#!/usr/bin/env python3
"""Offline guard tests. No esptool installation, serial port or device required."""
from __future__ import annotations

from contextlib import redirect_stdout
from dataclasses import replace
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("flash_panel", ROOT / "scripts" / "flash_panel.py")
flash = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = flash
spec.loader.exec_module(flash)


def image(length=4128, chip=18, revision=(0, 0, 65535)):
    raw = bytearray(b"\0" * length)
    raw[0] = 0xE9
    struct.pack_into("<H", raw, 12, chip)
    struct.pack_into("<BHH", raw, 14, *revision)
    return bytes(raw)


def table():
    rows = b"".join(struct.pack("<HBBII16sI", 0x50AA, kind, subtype, start, size, name.encode(), flags)
                    for kind, subtype, start, size, name, flags in flash.EXPECTED_PARTITIONS)
    return (rows + b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(rows).digest()).ljust(3072, b"\xff")


def ota_entry(seq, state=2):
    raw = bytearray(b"\xff" * 0x1000)
    struct.pack_into("<I", raw, 0, seq)
    struct.pack_into("<II", raw, 24, state, zlib.crc32(raw[:4], 0xFFFFFFFF))
    return bytes(raw)


def make_package(directory):
    firmware, bootloader, partitions = image(), image(1024), table()
    report = {"chip": "ESP32-P4", "image_chip_id": 18, "image_bytes": len(firmware),
              "min_chip_revision_legacy": 0, "min_chip_revision_full": 0, "max_chip_revision_full": 65535,
              "partitions": [dict(zip(("type", "subtype", "offset", "size", "label", "flags"), row))
                             for row in flash.EXPECTED_PARTITIONS]}
    blobs = {"firmware.bin": firmware, "bootloader.bin": bootloader, "partitions.bin": partitions,
             "binary-verification.json": json.dumps(report).encode()}
    for name, raw in blobs.items():
        (directory / name).write_bytes(raw)
    (directory / "SHA256SUMS.json").write_text(json.dumps({name: flash.sha(raw) for name, raw in blobs.items()}))
    return flash.Package.load(directory)


class FakeDevice:
    def __init__(self, package, ota=None):
        self.info = flash.DeviceInfo("ESP32-P4", 103, "16MB", False, False, False, "0123456789ab")
        self.memory = bytearray(b"\xff" * flash.FLASH_BYTES)
        self.memory[flash.TABLE_OFFSET:flash.TABLE_OFFSET + flash.TABLE_BYTES] = package.table
        self.memory[flash.BOOT_OFFSET:flash.BOOT_OFFSET + len(package.bootloader)] = package.bootloader
        if ota is not None:
            self.memory[flash.OTA_OFFSET:flash.OTA_OFFSET + flash.OTA_BYTES] = ota
        self.old_image = image(2048)
        for offset in flash.APP_OFFSETS:
            self.memory[offset:offset + len(self.old_image)] = self.old_image
        self.reads, self.writes, self.resets = [], [], 0
        self.corrupt_write = False

    def read(self, offset, length):
        self.reads.append((offset, length))
        return bytes(self.memory[offset:offset + length])

    def write(self, offset, raw):
        self.writes.append((offset, raw))
        self.memory[offset:offset + len(raw)] = raw
        if self.corrupt_write:
            self.memory[offset + 25] ^= 1

    def reset(self):
        self.resets += 1


class FlashTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.package = make_package(self.root)
        self.backups = self.root / "backups"
        self.dev = FakeDevice(self.package)

    def tearDown(self):
        self.temp.cleanup()

    def run_upload(self, write=True, restore=None):
        with redirect_stdout(io.StringIO()):
            return flash.execute(self.package, self.dev, write=write, backup_root=self.backups, restore=restore)

    def refuse(self):
        with self.assertRaises(flash.Refusal):
            self.run_upload()
        self.assertEqual(self.dev.writes, [])

    def test_package_hash_before_hardware(self):
        (self.root / "firmware.bin").write_bytes(b"modified")
        with self.assertRaisesRegex(flash.Refusal, "SHA-256"):
            flash.Package.load(self.root)

    def test_image_type_and_revision(self):
        for raw in (b"short", image(chip=13), image(revision=(0, 200, 65535)), image(revision=(2, 0, 65535))):
            with self.subTest(raw=raw[:24]):
                with self.assertRaises(flash.Refusal):
                    flash.image_revision(raw)
        flash.image_revision(image(revision=(1, 0, 65535)))
        flash.image_revision(image(revision=(0, 103, 103)))

    def test_table_md5_and_known_layout(self):
        broken = bytearray(table())
        broken[12] ^= 1
        with self.assertRaises(flash.Refusal):
            flash.partition_records(bytes(broken))
        self.assertEqual(flash.partition_records(table()), flash.EXPECTED_PARTITIONS)

    def test_ota_selection_erased_and_valid(self):
        blank = b"\xff" * 0x1000
        self.assertEqual(flash.selected_slot(blank * 2), 0)
        self.assertEqual(flash.selected_slot(ota_entry(1) + blank), 0)
        self.assertEqual(flash.selected_slot(ota_entry(2) + blank), 1)
        self.assertEqual(flash.selected_slot(ota_entry(5) + ota_entry(6)), 1)
        self.assertEqual(flash.selected_slot(ota_entry(7, 0xFFFFFFFF) + ota_entry(6)), 0)

    def test_ota_unconfirmed_corrupt_and_unknown_are_refused(self):
        blank = b"\xff" * 0x1000
        for state in (0, 1, 3, 4, 99):
            with self.subTest(state=state), self.assertRaises(flash.Refusal):
                flash.selected_slot(ota_entry(1, state) + blank)
        broken = bytearray(ota_entry(1) + blank)
        broken[28] ^= 1
        with self.assertRaises(flash.Refusal):
            flash.selected_slot(bytes(broken))
        broken = bytearray(ota_entry(1) + blank)
        broken[100] = 0
        with self.assertRaises(flash.Refusal):
            flash.selected_slot(bytes(broken))

    def test_wrong_chip_revision_flash_or_protections_never_write(self):
        good = self.dev.info
        for changes in ({"chip": "ESP32-C6"}, {"revision": 300}, {"flash_size": "8MB"},
                        {"secure_download": True}, {"secure_boot": True}, {"encrypted": True},
                        {"mac": ""}):
            with self.subTest(changes=changes):
                self.dev.info = replace(good, **changes)
                self.refuse()
                self.assertEqual(self.dev.reads, [])

    def test_partition_mismatch_never_write(self):
        self.dev.memory[flash.TABLE_OFFSET] ^= 1
        self.refuse()

    def test_bootloader_mismatch_never_write(self):
        self.dev.memory[flash.BOOT_OFFSET] ^= 1
        self.refuse()

    def test_pending_ota_never_write(self):
        self.dev.memory[flash.OTA_OFFSET:flash.OTA_OFFSET + flash.OTA_BYTES] = ota_entry(1, 1) + b"\xff" * 0x1000
        self.refuse()

    def test_read_only_has_no_backup_or_write(self):
        self.assertIsNone(self.run_upload(write=False))
        self.assertEqual(self.dev.writes, [])
        self.assertFalse(self.backups.exists())
        self.assertEqual(self.dev.resets, 1)

    def test_only_active_slot_written_after_complete_backup(self):
        self.dev = FakeDevice(self.package, ota_entry(2) + b"\xff" * 0x1000)
        original = bytes(self.dev.memory)
        original_write = self.dev.write

        def guarded_write(offset, raw):
            metadata = list(self.backups.glob("*/metadata.json"))
            self.assertEqual(len(metadata), 1)
            directory = metadata[0].parent
            self.assertEqual((directory / "application.bin").read_bytes(),
                             original[flash.APP_OFFSETS[1]:flash.APP_OFFSETS[1] + flash.APP_BYTES])
            return original_write(offset, raw)

        self.dev.write = guarded_write
        backup = self.run_upload()
        self.assertTrue(backup.is_dir())
        self.assertEqual(self.dev.writes, [(flash.APP_OFFSETS[1], self.package.firmware)])
        start, size = flash.APP_OFFSETS[1], len(self.package.firmware)
        self.assertEqual(bytes(self.dev.memory[:start]), original[:start])
        self.assertEqual(bytes(self.dev.memory[start + size:]), original[start + size:])
        self.assertEqual(self.dev.resets, 1)

    def test_backup_failure_prevents_write(self):
        with mock.patch.object(flash, "save_bytes", side_effect=OSError("disk full")):
            with self.assertRaises(OSError):
                self.run_upload()
        self.assertEqual(self.dev.writes, [])
        self.assertEqual(self.dev.resets, 0)

    def test_readback_mismatch_does_not_reset(self):
        self.dev.corrupt_write = True
        with self.assertRaisesRegex(flash.Refusal, "Проверка записанного"):
            self.run_upload()
        self.assertEqual(self.dev.resets, 0)
        self.assertEqual(len(list(self.backups.glob("*/metadata.json"))), 1)

    def test_restore_returns_original_bytes_and_creates_new_backup(self):
        original = bytes(self.dev.memory)
        saved = self.run_upload()
        second = self.run_upload(restore=saved)
        self.assertNotEqual(saved, second)
        self.assertEqual(bytes(self.dev.memory), original)
        self.assertEqual(len(self.dev.writes), 2)

    def test_restore_wrong_device_or_corrupt_file_cannot_write(self):
        saved = self.run_upload()
        self.dev.writes.clear()
        self.dev.info = replace(self.dev.info, mac="aabbccddeeff")
        with self.assertRaisesRegex(flash.Refusal, "другой плате"):
            self.run_upload(restore=saved)
        self.assertEqual(self.dev.writes, [])
        self.dev.info = replace(self.dev.info, mac="0123456789ab")
        (saved / "application.bin").write_bytes(b"broken")
        with self.assertRaisesRegex(flash.Refusal, "Контрольные суммы"):
            self.run_upload(restore=saved)
        self.assertEqual(self.dev.writes, [])

    def test_restore_can_recover_an_erased_current_app_header(self):
        original = bytes(self.dev.memory)
        saved = self.run_upload()
        offset = flash.APP_OFFSETS[0]
        self.dev.memory[offset:offset + 4096] = b"\xff" * 4096
        self.dev.writes.clear()
        recovery_backup = self.run_upload(restore=saved)
        self.assertEqual(bytes(self.dev.memory), original)
        self.assertEqual(self.dev.writes, [(offset, original[offset:offset + flash.APP_BYTES])])
        self.assertEqual((recovery_backup / "application.bin").read_bytes()[:4096], b"\xff" * 4096)

    def test_backend_write_cannot_target_metadata_or_force(self):
        backend = object.__new__(flash.EsptoolBackend)
        backend.esp = object()
        backend.cmds = mock.Mock()
        for address in (0, flash.BOOT_OFFSET, flash.TABLE_OFFSET, flash.OTA_OFFSET, 0x9000):
            with self.subTest(address=address), self.assertRaises(flash.Refusal):
                backend.write(address, self.package.firmware)
        backend.cmds.write_flash.assert_not_called()
        backend.write(flash.APP_OFFSETS[0], self.package.firmware)
        args, kwargs = backend.cmds.write_flash.call_args
        self.assertEqual(args[1], [(flash.APP_OFFSETS[0], self.package.firmware)])
        for flag in ("erase_all", "encrypt", "force", "ignore_flash_enc_efuse"):
            self.assertIs(kwargs[flag], False)


if __name__ == "__main__":
    unittest.main(verbosity=2)
