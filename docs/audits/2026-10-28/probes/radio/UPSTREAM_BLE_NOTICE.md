# Upstream BLE excerpt notice

`stubs/ble_scan_blocking_wrapper.inc` contains the unchanged 20-line blocking
`BLEScan::start(uint32_t, bool)` function from Arduino-ESP32 3.3.6,
`libraries/BLE/src/BLEScan.cpp`, lines 790–809.

- Source: https://github.com/espressif/arduino-esp32/blob/3.3.6/libraries/BLE/src/BLEScan.cpp#L790-L809
- Source Git blob SHA-1: `54e60a2e4235f98d1d7aacf55d6dabb9b57444a5`.
- BLE library license: **Apache License, Version 2.0**.
- Applicable upstream license: https://github.com/espressif/arduino-esp32/blob/3.3.6/libraries/BLE/LICENSE
- License Git blob SHA-1: `4558f79ff9be236d994ebe8ea55ed382832b4d48`.
- A copy of that license is included in `UPSTREAM_BLE_LICENSE.txt`, including its warranty disclaimer.

The original source header is preserved here:

```cpp
/*
 * BLEScan.cpp
 *
 *  Created on: Jul 1, 2017
 *      Author: kolban
 *
 * 	Update: April, 2021
 * 		add BLE5 support
 *
 * 	Modified on: Feb 18, 2025
 * 		Author: lucasssvaz (based on kolban's and h2zero's work)
 * 		Description: Added support for NimBLE
 */
```

The function body is unchanged. It was extracted for this audit on 2026-10-28.
The surrounding classes and SDK calls in the local stubs are host substitutes,
not a distribution of the complete Arduino-ESP32 BLE implementation. The scan
start call deliberately returns `false` to exercise the application failure path.

No upstream NOTICE file was present in the `libraries/BLE` directory at tag
3.3.6. This file records the source attribution and extraction, not an additional
license grant or an assertion of authorship over the upstream function.
