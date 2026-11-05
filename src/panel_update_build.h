#pragma once

// Pinned pioarduino 55.03.36-1 / Arduino 3.3.6 / IDF 5.5.2 bootloader.
// esp32p4_es/bin/bootloader_qio_80m.elf -> dio/80m/16MB image.
// Exact bytes were reproduced and compared with the installed bootloader.
// OTA must refuse an unknown bootloader rather than assume rollback support.
#define BOILER_BOOTLOADER_SHA256 "6a88d429ece0ae20f8ae18493b9f3dee05c35f4abaa22f0320e44c34d30631c2"
#define BOILER_BOOTLOADER_BYTES 21616
