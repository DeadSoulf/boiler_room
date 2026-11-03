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

Built for ESP32-P4/PIOArduino 55.03.36-1 (Arduino-ESP32 3.3.6; LVGL 8.3.11). The real DS18B20 driver uses the MIT-licensed [esp32-ds18b20](https://github.com/junkfix/esp32-ds18b20) dependency pinned to commit `c2aa0147aa7414419fce61e89ed07ef6ff1d535b`.

**v0.6.1 successfully flashed** to the P4 application partition via Asus COM3 at 115200 baud after backing up the previous v0.6.0 app. The safe flasher verified the written image byte-by-byte. NVS, C6 firmware, bootloader, partition table and the inserted microSD were not erased.

From the physical panel's local address `http://192.168.144.125/` (may change with DHCP): GET `/`, `/health`, `/api/status` all returned HTTP 200, Wi-Fi connected, SD mounted with logging enabled. There are six unassigned sensor channels in NVS (revision 0); as expected, no sensor GPIOs are active. Unauthenticated POSTs to change sensors or roll back configuration return HTTP 401. [Independent sensor safety CI](https://github.com/DeadSoulf/boiler_room/actions/runs/41594675942) passed.

**Hardware sensor validation is still pending.** No DS18B20 or reed sensor was physically assigned or measured in that smoke test. Real wiring must be verified at 3.3 V; DS18B20 needs an external 4.7 kΩ data pull-up. The present driver requires one DS18B20 per GPIO. Unsupervised reed switches cannot distinguish an open contact from a broken wire.

Modbus RTU/RS-485 and MQTT appear as configuration source types but their hardware/network adapters are not active yet. Live heating outputs are intentionally absent until actual controller wiring and safety protections are verified.

## Development

The active feature branch is `feature/real-sensors-v0.6.1` (draft pull request against v0.6.0). Other project firmware must not be mixed into this board. For manufacturing or destructive SD operations, follow the safety documentation; never flash the ESP32-C6 image directly to the P4 COM3 port.
