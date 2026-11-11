#pragma once
#include <stdint.h>

// DHW tank settings only. Nothing in this module authorizes or starts heating.
namespace panel_boiler {
struct Settings {
  bool configured=false;
  uint8_t onC=0,offC=0;
  uint32_t revision=0;
};
enum class Result { Saved, Invalid, Conflict, Unavailable };
bool begin(); // Called in setup before starting either UI.
bool snapshot(Settings& out); // false means unavailable, not "no settings".
Result save(uint32_t expectedRevision,int onC,int offC);
} // namespace panel_boiler
