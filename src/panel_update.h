#pragma once
#include <stdint.h>

// The update worker owns HTTPS and inactive Flash. Callers only enqueue requests.
namespace panel_update {
enum class Phase : uint8_t {
  Idle, Checking, Current, Available, Downloading, Verifying, Installing,
  Rebooting, Success, Error, Cancelled, RolledBack
};
struct Snapshot {
  bool started=false,supported=false,busy=false,available=false;
  bool cancelAllowed=false,rollbackAvailable=false,bootPending=false;
  bool automaticRollback=false;
  Phase phase=Phase::Idle;
  uint32_t done=0,total=0,lastCheck=0;
  uint8_t percent=0;
  char latestVersion[24]={},previousVersion[24]={},message[192]={};
  int32_t errorCode=0;
};
bool begin();
bool snapshot(Snapshot& out);
bool requestCheck();
bool requestInstall(const char* expectedVersion);
bool requestCancel();
bool requestRollback(const char* expectedPreviousVersion);
// Feed from the real UI loop. A pending app is confirmed only after healthy ticks.
void uiHeartbeat(bool ready);
const char* phaseName(Phase phase);
} // namespace panel_update
