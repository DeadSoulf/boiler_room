#pragma once
#include <stdint.h>
#include "panel_boiler.h"

// Independent, user-selected temperature settings. They are not sensor readings.
namespace panel_targets {
constexpr int MIN_C=-55, MAX_C=125;
struct Setting {
  bool configured=false;
  int valueC=0;
  uint32_t revision=0;
};
using Result=panel_boiler::Result;
bool begin();
bool snapshot(unsigned channel,Setting& out);
Result save(unsigned channel,uint32_t expectedRevision,int valueC);
} // namespace panel_targets
