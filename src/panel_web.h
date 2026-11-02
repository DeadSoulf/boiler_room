#pragma once
// Device HTTP visualization with protected Wi-Fi, BLE, clock and screen settings.
// No equipment simulation, synthetic commands, or actuator control.
#include <stdint.h>
#include "panel_types.h"

namespace panel_web {
bool begin();
void publish(const panel::Measurement (&temps)[panel::CHANNELS],
             const panel::FuelLevel &fuel,uint32_t atMs);
// Physical pairing controls are only called from the touchscreen.
bool armPairing();
bool pairingCode(char out[7],uint32_t& remaining);
void revokePairing();
} // namespace panel_web
