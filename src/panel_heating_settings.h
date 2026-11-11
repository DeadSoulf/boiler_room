#pragma once
#include "panel_heating_logic.h"

namespace panel_heating_settings {
struct State {
  uint32_t revision=0;
  panel_heating::Config config{};
  panel_heating::Season season=panel_heating::Season::Unknown;
};
enum class Result {Saved,Invalid,Conflict,Unavailable};
bool begin();
bool snapshot(State& out);
Result save(uint32_t expectedRevision,const State& next);
}
