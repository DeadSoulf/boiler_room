#pragma once
#include <stdint.h>

// Temperature settings for each appliance. Saving a pair never starts heating.
namespace panel_boiler {
enum class Target { Dhw, Electric };
struct Settings {
  bool configured=false;
  uint8_t onC=0,offC=0;
  uint32_t revision=0;
};
enum class Result { Saved, Invalid, Conflict, Unavailable };
bool begin(); // Called in setup before starting either UI.
bool snapshot(Settings& out,Target target=Target::Dhw); // false means unavailable, not "no settings".
Result save(uint32_t expectedRevision,int onC,int offC,Target target=Target::Dhw);
} // namespace panel_boiler
