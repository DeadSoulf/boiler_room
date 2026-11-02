#pragma once
// Local, read-only HTTP visualization. No physical or demo commands via HTTP.
// Every UI-owned value is copied under a short mutex from the LVGL thread.
#include <stdint.h>
#include "panel_types.h"
#include "equipment_demo.h"

namespace panel_web {
// Starts a separate worker; nonblocking. Returns false if resources unavailable.
bool begin();
// Call from the LVGL task, no more than once per second.
// Does not call Wi-Fi and never waits for HTTP clients.
void publish(const panel::Measurement (&temps)[panel::CHANNELS],
             const panel::FuelLevel &fuel,
             const panel::equipment_demo::State &equipment,
             uint32_t atMs);
} // namespace panel_web
