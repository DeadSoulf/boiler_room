#pragma once
#include "panel_heating_logic.h"

// The LVGL refresh task owns samples and the controller. Readers receive one
// coherent, read-only set of calculated requests; none of these are relay feedback.
namespace panel_heating_runtime {
struct Snapshot {
  bool configured=false;
  bool settingsAvailable=false;
  uint32_t settingsRevision=0,sampledAtMs=0;
  panel_heating::Inputs input{};
  panel_heating::Output output{};
};
void resetSensors();
void tick(bool pinConfigKnown,uint32_t pinRevision);
bool snapshot(Snapshot& out);
}
