# Boiler Panel — GUITION JC4880P443 (ESP32-P4 + ESP32-C6)

**Current firmware: v0.5.6.** This repository is an independent boiler-room touch-panel project. It is not the heater-control firmware or a relay controller.

[Инструкция на русском](README_RU.md) · [microSD](docs/microsd-storage.md) · [Web interface](docs/web-on-panel.md) · [Safe flashing](docs/flash-panel.md)

## What works

- 480×800 LVGL portrait display, horizontal swipe navigation, first-frame startup, touch wake, configurable backlight timeout.
- Four temperature channels and two fuel-level reed switch channels, without invented data. Actual electrical sensors are not yet connected.
- Wi-Fi through the ESP32-C6 ESP-Hosted co-processor (host/C6 firmware 2.9.6), Wi-Fi scan/connect, BLE discovery, time and timezone.
- Embedded Russian HTTP browser interface with live status, physically initiated pairing for configuration changes. Use **trusted local LAN only**, never expose HTTP port 80 to the internet.
- Equipment inventory: indirect water heater, electric boiler and three circuits, diesel heater, fuel heater. None is connected to a confirmed actuator controller, so **no physical commands and no simulations** are provided.
- microSD monitor and bounded logging under `/boiler-panel/logs/` only when mounting a supported filesystem succeeds. No automatic formatting; storage access runs outside the LVGL and networking tasks.

## microSD status

The card is connected to P4 SDMMC **Slot 0** (CLK43/CMD44/D0–D3 GPIO39–42), powered by on-chip LDO4. The ESP32-C6 SDIO link uses Slot 1. The BSP already owns LDO4, so the Arduino SD_MMC wrapper must use `setPowerChannel(-1)`.

**On-device test (Nov 2, 2026):** v0.5.6 boots and Wi-Fi/Web still work, but the inserted card returns `vfs_fat_sdmmc: mount_to_vfs failed (0xffffffff)`. It may use an unsupported filesystem such as exFAT, or there may be a filesystem/read error; capacity and free space are not yet verified, and no log files were written. No card format or erase was attempted. Follow [issue #5](https://github.com/DeadSoulf/boiler_room/issues/5) for nondestructive diagnosis.

## Next milestone

[Issue #4](https://github.com/DeadSoulf/boiler_room/issues/4): Tasmota-like hardware configuration page, exposing only verified, physically available GPIOs, supported sensor types, bus assignments, driver-specific settings, conflict validation, NVS-backed active configuration with rollback, and optional SD backup once the card mounts reliably.

**Never assign GPIOs reserved for the display, GT911 touch, SDMMC, audio, or ESP-Hosted SDIO. Never enable live boiler/heater outputs without verified wiring, an actual control module and independent safety limits.**

Build environment: PIOArduino 55.03.36-1 / Arduino-ESP32 3.3.6 / LVGL 8.3.11 / ESP32-P4 ES rev1.3 at 360 MHz. The safe flasher only writes the P4 app partition after chip, layout and backup checks; it never overwrites the C6 firmware or NVS.
