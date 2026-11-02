#pragma once
#include <stdint.h>
#include "panel_fs_signature.h"
namespace panel_fs_probe {
struct Result {
  panel_fs_signature::Type type=panel_fs_signature::Type::Unknown;
  uint64_t cardBytes=0;
  bool cardPresent=false;
  bool sectorReadable=false;
};
// Probe is read-only. Call only from SD worker AFTER SD_MMC.end().
Result inspect();
} // namespace panel_fs_probe
