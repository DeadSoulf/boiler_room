#pragma once
#include "panel_types.h"
namespace panel_radio {
bool begin();
bool submit(const panel::Command& command);
bool snapshot(panel::RadioState& out);
const char* zoneName(uint8_t zone);
} // namespace panel_radio
