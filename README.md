# Boiler Room Touch Panel — v0.6.2

A standalone GUITION JC4880P443C_I_W boiler-room panel, ESP32-P4 rev v1.3 + ESP32-C6 (ESP-Hosted 2.9.6), 480×800 LVGL portrait display. Source of truth: [GitHub](https://github.com/DeadSoulf/boiler_room).

[Русская документация](README_RU.md) · [Real DS18B20/reed wiring and JP1 GPIOs](docs/real-sensors-v061.md) · [Web UI](docs/web-on-panel.md) · [microSD](docs/microsd-storage.md) · [Safe flashing](docs/flash-panel.md)

## v0.6.2 maintenance update

Retires the unused demo simulator code, keeps seven real-equipment entries explicitly disconnected, and updates regression tests. The v0.6.1 sensor drivers, safety gates and pin assignments are unchanged.

## Implemented

- Four temperature channels (room, outside, coolant, fuel), freshness, history, minimum/maximum, and two reed fuel-level switches. No fabricated measurements.
- **v0.6.1 real input drivers:** hardware RMT-backed OneWire DS18B20 (single thermometer per assigned GPIO, ROM and scratchpad CRC checks, delayed conversion), and NO/NC reed contacts (input pull-up, configurable debounce, periodic state refresh). All electrical I/O runs in a separate low-priority task and feeds the LVGL/web data model through a bounded queue.
- Sensors can only use verified external JP1 header pins: GPIO **52, 33, 51, 31, 50, 30, 49, 29, 34, 32, 28**. Display/touch, ESP32-C6/SDIO, SDMMC, audio, RS-485, UART and BOOT-strapping pins are blocked. See [JP1 pin numbers and wiring](docs/real-sensors-v061.md).
- The local Russian HTTP UI offers authenticated **Devices** configuration (four temperature channels, two fuel-level contacts, type, GPIO, debounce), real sensor status and DS18B20 ROM, and rollback of the previous NVS configuration. Two NVS slots with integrity verification preserve earlier settings on write failure. The physical display's Date/Time settings and Symbol Check are removed; clock/timezone controls remain in the web UI.
- ESP32-C6 Wi-Fi and BLE scan, touch wake and screen sleep settings, real SDMMC microSD capacity, free space and bounded event/temperature logs. Manual SD formatting from HTTP always needs separate physical touchscreen confirmation; **no autoformat**.
- Equipment inventory (indirect hot-water boiler, electric boiler and its three circuits, diesel heater, fuel heater) is marked **not connected** until real external controllers are installed. There are **no simulated device commands** and no physical heat/pump/relay outputs.

## On-device status — November 3, 2026

**v0.6.2 is installed on the physical JC4880P443 panel.** An isolated PlatformIO build (PIOArduino 55.03.36-1 / Arduino-ESP32 3.3.6 / LVGL 8.3.11) succeeded. The ESP32-P4 app image is 1,608,224 bytes, SHA-256 `276d5041fb69dfb0babe0d58181564260a61bb5c171928c4d2fa4f1f14711986`.

On Asus COM3 (115200 baud), the updater validated the ESP32-P4 v1.3, 16 MB Flash, the current bootloader, partition table and OTA data. A complete previous app0 backup was saved before writing. The initial read-back was interrupted by a transient USB transfer failure; an independent full read of the written image subsequently confirmed **byte-for-byte identity** with the firmware. Partition and OTA metadata were read back unchanged.

After boot, `GET /`, `GET /health`, and `GET /api/status` returned HTTP 200: firmware version `v0.6.2`, Wi-Fi connected, microSD mounted and logging. All six sensor inputs remain unassigned (`revision=0`, `drivers_active=false`); real DS18B20 and reed wiring is **not yet hardware validated**. There are no physical heating commands or simulated outputs. The installation did not write NVS, the bootloader, partition table, OTA, ESP32-C6 firmware, or the microSD card.

[Complete hardware verification and SHA-256 evidence](docs/verification/v0.6.2-hardware-2026-11-03.md). Separate sensor, configuration, storage/web and Windows launcher CI checks pass. The general legacy native build workflow still fails on outdated `esp_wifi.h` host mocks; that CI failure is not a successful general CI run.

## Remaining integration

DS18B20 and reed switches require real 3.3 V wiring, a 4.7 kΩ DS18B20 pull-up and physically validated JP1 pin assignments. The present driver requires one DS18B20 per GPIO. Unsupervised reed switches cannot distinguish an open contact from a broken wire. Modbus RTU/RS-485 and MQTT source types are configuration-only; hardware/network adapters are not active. Live heating controls remain absent until an external actuator controller and safety protections are verified.

## Development

The **default branch `main`** contains the v0.6.2 source that was installed and verified on the panel. Experimental Neon/exFAT work remains in a separate draft PR and must not be confused with the installed release. Other project firmware must not be mixed into this board. For manufacturing or destructive SD operations, follow the safety documentation; never flash the ESP32-C6 image directly to the P4 COM3 port.
