#pragma once
#include "panel_types.h"
namespace panel_radio {
bool begin();
// Normal busy submissions reject; Disconnect/Forget have priority.
// Connect polls stops at 100 ms; blocking SDK scan/startup calls finish first.
bool submit(const panel::Command& command);
// Cancel connection and disable autoconnect (same as Disconnect).
bool cancel();
bool snapshot(panel::RadioState& out);
const char* zoneName(uint8_t zone);
} // namespace panel_radio
