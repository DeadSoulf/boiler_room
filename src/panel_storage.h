#pragma once
// JC4880P443 microSD: capacity, bounded logs, physically confirmed formatting.
// ALL filesystem and format operations execute on one low-priority SD task.
// Never automatically format on boot; never touch P4 Flash, NVS, C6, or PC disks.
#include <stdint.h>
#include "panel_types.h"

namespace panel_storage {
enum class FormatPhase : uint8_t {
  Idle=0, AwaitingPhysical=1, Queued=2, Running=3,
  Verifying=4, Complete=5, Failed=6, Cancelled=7, Expired=8
};
struct Snapshot {
  bool started=false;
  bool mounted=false;
  bool logging=false;
  uint64_t cardBytes=0,totalBytes=0,usedBytes=0,freeBytes=0;
  uint32_t writtenEvents=0,writtenSamples=0,dropped=0;
  uint32_t refreshedAt=0;
  char message[112]={};
  FormatPhase formatPhase=FormatPhase::Idle;
  bool formatAttempted=false;
  uint32_t formatRemainingSeconds=0;
  char formatMessage[112]={};
};
bool begin();
bool snapshot(Snapshot& out);
bool event(const char* category,const char* description);
bool measurements(const panel::Measurement (&temps)[panel::CHANNELS],uint32_t atMs);

// Only authenticated browser requests can prepare. NO formatting here.
// Returns false if already pending, busy, or attempted this boot.
bool prepareFormat();
// Confirm/cancel are called ONLY from LVGL touchscreen event handlers.
// Confirmation is valid for 90 seconds and can be used just once.
bool confirmFormatPhysically();
bool cancelFormatPhysically();
} // namespace panel_storage
