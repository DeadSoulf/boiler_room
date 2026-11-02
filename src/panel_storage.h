#pragma once
// JC4880P443 microSD (SDMMC 4-bit): capacity, free space, bounded logging.
// A single low-priority task owns SD_MMC and all filesystem operations.
// No formatting, erasure of user files, or writes outside /boiler-panel/logs.
#include <stdint.h>
#include "panel_types.h"

namespace panel_storage {
struct Snapshot {
  bool started=false;
  bool mounted=false;
  bool logging=false;
  uint64_t cardBytes=0, totalBytes=0, usedBytes=0, freeBytes=0;
  uint32_t writtenEvents=0, writtenSamples=0, dropped=0;
  uint32_t refreshedAt=0;
  char message[112]={};
};
// Nonblocking: SD mounting happens after display startup in its own task.
bool begin();
// Copies cached status only; safe for LVGL and WebServer tasks.
bool snapshot(Snapshot& out);
// Nonblocking and best-effort. Never include passwords, pairing codes or SSIDs.
bool event(const char* category,const char* description);
// Only actual fresh sensor readings are logged. Max one row per minute.
bool measurements(const panel::Measurement (&temps)[panel::CHANNELS],uint32_t atMs);
} // namespace panel_storage
