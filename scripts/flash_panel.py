#!/usr/bin/env python3
"""Checked application-only upload for the known JC4880P443 / ESP32-P4 ES v1.3.

No flash writes without --write. Bootloader, partition table, otadata, NVS and
C6 are never written. Backups remain on the user's computer. esptool is imported
only by the real backend, so the guards can be tested without a serial device.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import zlib

ESPTOOL_VERSION = "5.1.0"
FLASH_BYTES = 16 * 1024 * 1024
REVISION = 103  # ESP32-P4 ES v1.3, the explicitly selected board in this project.
BOOT_OFFSET = 0x2000
TABLE_OFFSET = 0x8000
TABLE_BYTES = 0x1000
OTA_OFFSET = 0xE000
OTA_BYTES = 0x2000
APP_OFFSETS = (0x10000, 0x650000)
APP_BYTES = 0x640000
EXPECTED_PARTITIONS = (
    (1, 2, 0x9000, 0x5000, "nvs", 0),
    (1, 0, OTA_OFFSET, OTA_BYTES, "otadata", 0),
    (0, 0x10, APP_OFFSETS[0], APP_BYTES, "app0", 0),
    (0, 0x11, APP_OFFSETS[1], APP_BYTES, "app1", 0),
    (1, 0x82, 0xC90000, 0x360000, "spiffs", 0),
    (1, 3, 0xFF0000, 0x10000, "coredump", 0),
)
PAYLOAD_FILES = ("firmware.bin", "bootloader.bin", "partitions.bin", "binary-verification.json")


class Refusal(RuntimeError):
    """A failed precondition; the runner must not guess or override it."""


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise Refusal(message)


def image_revision(data: bytes, revision: int = REVISION) -> tuple[int, int, int]:
    require(len(data) >= 24 and data[0] == 0xE9, "Некорректный заголовок образа ESP.")
    require(struct.unpack_from("<H", data, 12)[0] == 18, "Образ предназначен не для ESP32-P4.")
    legacy, minimum, maximum = data[14], *struct.unpack_from("<HH", data, 15)
    # Same legacy/full-revision selection as pinned esptool v5.1.0 write_flash.
    use_full = maximum != 0 and not (maximum == 65535 and minimum == 0 and legacy != 0)
    compatible = minimum <= revision <= maximum if use_full else revision // 100 >= legacy
    require(compatible, "Образ несовместим с ревизией ESP32-P4 v1.3.")
    return legacy, minimum, maximum


def partition_records(data: bytes) -> tuple[tuple, ...]:
    require(32 <= len(data) <= TABLE_BYTES and len(data) % 32 == 0,
            "Некорректный размер таблицы разделов.")
    records = []
    for offset in range(0, len(data), 32):
        row = data[offset:offset + 32]
        if row[:2] == b"\xeb\xeb":
            require(row[2:16] == b"\xff" * 14 and row[16:] == hashlib.md5(data[:offset]).digest(),
                    "Контрольная сумма таблицы разделов не совпала.")
            require(all(b == 255 for b in data[offset + 32:]), "Неизвестный хвост таблицы разделов.")
            return tuple(records)
        require(row[:2] == b"\xaa\x50", "Таблица разделов без ожидаемого MD5 или с неизвестной записью.")
        _, kind, subtype, start, size, label, flags = struct.unpack("<HBBII16sI", row)
        try:
            name = label.split(b"\0", 1)[0].decode("ascii")
        except UnicodeDecodeError as exc:
            raise Refusal("Неизвестное имя раздела.") from exc
        records.append((kind, subtype, start, size, name, flags))
    raise Refusal("В таблице отсутствует MD5.")


def selected_slot(data: bytes) -> int:
    """Conservative ESP-IDF OTA selector: only erased, VALID or UNDEFINED entries.

    NEW/PENDING_VERIFY/INVALID/ABORTED, corrupt CRC and unknown layouts are refused.
    The CRC seed and max-sequence selection follow ESP-IDF v5.5.2 otatool and
    bootloader_common_loader.c. This function never changes otadata.
    """
    require(len(data) == OTA_BYTES, "Не удалось полностью прочитать otadata.")
    valid = []
    for offset in (0, 0x1000):
        sector = data[offset:offset + 0x1000]
        if sector == b"\xff" * 0x1000:
            continue
        require(sector[32:] == b"\xff" * (0x1000 - 32), "Неизвестный формат otadata.")
        seq = struct.unpack_from("<I", sector)[0]
        state, crc = struct.unpack_from("<II", sector, 24)
        require(0 < seq < 0xFFFFFFFF and zlib.crc32(sector[:4], 0xFFFFFFFF) == crc,
                "Повреждена запись otadata; автоматическая запись отменена.")
        require(state in (2, 0xFFFFFFFF),
                "OTA не в подтверждённом состоянии. Автоматическая запись отменена.")
        valid.append(seq)
    return (max(valid) - 1) % 2 if valid else 0


@dataclass(frozen=True)
class Package:
    firmware: bytes
    bootloader: bytes
    table: bytes

    @classmethod
    def load(cls, directory: Path) -> Package:
        try:
            manifest = json.loads((directory / "SHA256SUMS.json").read_text(encoding="utf-8-sig"))
            require(isinstance(manifest, dict), "SHA256SUMS.json должен содержать объект имя: SHA-256.")
            files = {name: (directory / name).read_bytes() for name in PAYLOAD_FILES}
            for name, raw in files.items():
                require(manifest.get(name) == sha(raw), f"Не совпала SHA-256: {name}.")
            firmware, bootloader, table = (files[name] for name in PAYLOAD_FILES[:3])
            require(24 <= len(firmware) <= APP_BYTES, "Размер приложения выходит за границы раздела.")
            require(24 <= len(bootloader) <= TABLE_OFFSET - BOOT_OFFSET, "Неожиданный размер загрузчика.")
            revisions = image_revision(firmware)
            image_revision(bootloader)
            records = partition_records(table)
            require(records == EXPECTED_PARTITIONS, "Пакет содержит другую схему разделов.")
            report = json.loads(files["binary-verification.json"])
            require(report.get("chip") == "ESP32-P4" and report.get("image_chip_id") == 18
                    and report.get("image_bytes") == len(firmware), "Отчёт проверки не соответствует образу.")
            require(tuple(report.get(k) for k in ("min_chip_revision_legacy", "min_chip_revision_full",
                                                 "max_chip_revision_full")) == revisions,
                    "Ревизии в отчёте не соответствуют образу.")
            reported = tuple(tuple(p[k] for k in ("type", "subtype", "offset", "size", "label", "flags"))
                             for p in report.get("partitions", []))
            require(reported == records, "Отчёт проверки не соответствует таблице разделов.")
            return cls(firmware, bootloader, table.ljust(TABLE_BYTES, b"\xff"))
        except (OSError, ValueError, KeyError, TypeError) as exc:
            raise Refusal(f"Не удалось проверить комплект: {exc}") from exc


@dataclass(frozen=True)
class DeviceInfo:
    chip: str
    revision: int
    flash_size: str | None
    secure_download: bool
    secure_boot: bool
    encrypted: bool
    mac: str


def check_device(info: DeviceInfo) -> None:
    require(info.chip == "ESP32-P4", "На выбранном порту обнаружен не ESP32-P4.")
    require(info.revision == REVISION, "Пакет проверяет только выбранную плату ESP32-P4 ES v1.3.")
    require(info.flash_size == "16MB", "Не подтверждена Flash размером 16 МБ.")
    require(not (info.secure_download or info.secure_boot or info.encrypted),
            "Включён защищённый режим. Этот пакет не меняет и не обходит защиты.")
    require(len(info.mac) == 12 and all(c in "0123456789abcdef" for c in info.mac),
            "Не удалось получить идентификатор платы для резервной копии.")


def save_bytes(path: Path, data: bytes) -> None:
    with path.open("xb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    require(sha(path.read_bytes()) == sha(data), f"Не удалось проверить резервную копию {path.name}.")


def save_backup(root: Path, device: DeviceInfo, offset: int, originals: dict[str, bytes]) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix=datetime.now().strftime("%Y%m%d-%H%M%S-"), dir=root))
    for name, raw in originals.items():
        save_bytes(folder / name, raw)
    metadata = {"format": 1, "created_utc": datetime.now(timezone.utc).isoformat(),
                "device": asdict(device), "app_offset": offset, "app_size": APP_BYTES,
                "sha256": {name: sha(raw) for name, raw in originals.items()}}
    save_bytes(folder / "metadata.json", (json.dumps(metadata, indent=2) + "\n").encode())
    return folder


def restore_payload(folder: Path, device: DeviceInfo, offset: int,
                    bootloader: bytes, table: bytes, ota: bytes) -> bytes:
    try:
        meta = json.loads((folder / "metadata.json").read_text(encoding="utf-8"))
        require(meta.get("format") == 1 and meta.get("device") == asdict(device),
                "Резервная копия принадлежит другой плате или имеет неизвестный формат.")
        require(meta.get("app_offset") == offset and meta.get("app_size") == APP_BYTES,
                "Активный раздел отличается от резервной копии.")
        values = {name: (folder / name).read_bytes() for name in
                  ("application.bin", "bootloader.bin", "partitions.bin", "otadata.bin")}
        require(all(meta.get("sha256", {}).get(name) == sha(raw) for name, raw in values.items()),
                "Контрольные суммы резервной копии не совпали.")
        require(values["bootloader.bin"] == bootloader and values["partitions.bin"] == table
                and values["otadata.bin"] == ota, "Загрузчик, разделы или OTA изменились после резервирования.")
        require(len(values["application.bin"]) == APP_BYTES, "Неполная резервная копия приложения.")
        image_revision(values["application.bin"])
        return values["application.bin"]
    except (OSError, ValueError, TypeError) as exc:
        raise Refusal(f"Не удалось проверить резервную копию: {exc}") from exc


def execute(package: Package, device, *, write: bool, backup_root: Path,
            restore: Path | None = None) -> Path | None:
    """Use one connected bootloader session from inspection through verification."""
    check_device(device.info)
    table = device.read(TABLE_OFFSET, TABLE_BYTES)
    require(table == package.table, "Таблица разделов на панели отличается. Запись не выполнена.")
    bootloader = device.read(BOOT_OFFSET, len(package.bootloader))
    require(bootloader == package.bootloader, "Загрузчик на панели отличается. Запись не выполнена.")
    ota = device.read(OTA_OFFSET, OTA_BYTES)
    slot = selected_slot(ota)
    offset = APP_OFFSETS[slot]
    payload = restore_payload(restore, device.info, offset, bootloader, table, ota) if restore else package.firmware
    print(f"Проверено: ESP32-P4 v1.3, 16 МБ, раздел app{slot}, адрес {offset:#x}.")
    if not write:
        print("Проверка завершена. Flash не изменялась. Для записи нужен --write.")
        device.reset()
        return None
    old_app = device.read(offset, APP_BYTES)
    require(len(old_app) == APP_BYTES, "Не удалось полностью прочитать приложение для резервирования.")
    # A failed write can erase the old header. Recovery must still be possible:
    # restore_payload already checked the saved image and this exact board/slot.
    if restore is None:
        image_revision(old_app)
    backup = save_backup(backup_root, device.info, offset,
                         {"application.bin": old_app, "bootloader.bin": bootloader,
                          "partitions.bin": table, "otadata.bin": ota})
    print(f"Резервная копия: {backup}", flush=True)
    print("Записывается только приложение. Не отключайте питание.", flush=True)
    device.write(offset, payload)
    require(device.read(offset, len(payload)) == payload, "Проверка записанного приложения не прошла. Сохраните резервную копию.")
    require(device.read(TABLE_OFFSET, TABLE_BYTES) == table and device.read(OTA_OFFSET, OTA_BYTES) == ota,
            "Не совпали контрольные данные разделов после записи. Панель не перезапущена.")
    print("Запись приложения проверена побайтным чтением. Перезапуск панели.", flush=True)
    device.reset()
    return backup


class EsptoolBackend:
    def __init__(self, port: str, baud: int):
        try:
            found = importlib.metadata.version("esptool")
        except importlib.metadata.PackageNotFoundError as exc:
            raise Refusal('Нужен esptool 5.1.0: python -m pip install "esptool==5.1.0"') from exc
        require(found == ESPTOOL_VERSION, f"Нужен esptool {ESPTOOL_VERSION}, установлен {found}.")
        from esptool import cmds
        self.cmds = cmds
        self.port, self.baud, self.esp = port, baud, None

    def __enter__(self):
        self.esp = self.cmds.detect_chip(self.port, baud=115200)
        try:
            esp = self.esp
            require(esp.CHIP_NAME == "ESP32-P4", "Выбранный порт принадлежит не ESP32-P4.")
            require(not esp.secure_download_mode, "Secure Download Mode: автоматическая запись отменена.")
            revision = esp.get_chip_revision()
            secure_boot, encrypted = bool(esp.get_secure_boot_enabled()), bool(esp.get_flash_encryption_enabled())
            require(revision == REVISION and not secure_boot and not encrypted,
                    "Не подтверждена ESP32-P4 v1.3 без Secure Boot / Flash Encryption.")
            mac = "".join(f"{part:02x}" for part in esp.read_mac())
            self.esp = esp = self.cmds.run_stub(esp)
            if self.baud != 115200:
                esp.change_baud(self.baud)
            self.cmds.attach_flash(esp)
            self.info = DeviceInfo(esp.CHIP_NAME, revision, self.cmds.detect_flash_size(esp),
                                   False, secure_boot, encrypted, mac)
            return self
        except BaseException:
            self.esp.__exit__(*sys.exc_info())
            raise

    def __exit__(self, *exc):
        return self.esp.__exit__(*exc)

    def read(self, offset: int, size: int) -> bytes:
        raw = self.cmds.read_flash(self.esp, offset, size, flash_size="16MB")
        require(isinstance(raw, bytes) and len(raw) == size, "Неполное чтение Flash.")
        return raw

    def write(self, offset: int, raw: bytes) -> None:
        require(offset in APP_OFFSETS and 24 <= len(raw) <= APP_BYTES, "Запись вне раздела приложения запрещена.")
        self.cmds.write_flash(self.esp, [(offset, raw)], flash_freq="keep", flash_mode="keep",
                              flash_size="keep", erase_all=False, encrypt=False, force=False,
                              ignore_flash_enc_efuse=False)

    def reset(self) -> None:
        self.cmds.reset_chip(self.esp, "hard-reset")


def default_backups() -> Path:
    base = Path(os.environ["LOCALAPPDATA"]) if os.environ.get("LOCALAPPDATA") else Path.home() / ".local" / "share"
    return base / "BoilerPanel" / "backups"


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--package", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--baud", type=int, choices=(115200, 230400, 460800), default=460800)
    parser.add_argument("--write", action="store_true", help="Записать приложение после проверок и резервирования")
    parser.add_argument("--backup-dir", type=Path, default=default_backups())
    parser.add_argument("--restore", type=Path, help="Каталог ранее созданной резервной копии приложения")
    args = parser.parse_args(argv)
    try:
        require(sys.version_info >= (3, 10), "Требуется Python 3.10 или новее.")
        require(args.port.upper().startswith("COM") and args.port[3:].isdigit() and int(args.port[3:]) > 0,
                "Укажите локальный Windows COM-порт, например COM3.")
        package = Package.load(args.package)
        with EsptoolBackend(args.port, args.baud) as device:
            execute(package, device, write=args.write, backup_root=args.backup_dir, restore=args.restore)
        return 0
    except Exception as exc:
        print(f"Остановлено: {exc}", file=sys.stderr)
        print("Автоматический обход проверок не выполняется. Если панель осталась в загрузчике, нажмите RESET.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
