#pragma once
// Device HTTP UI: LVGL-only writes are marshalled back to LVGL using a bounded mailbox.
#include <stdint.h>
#include "panel_types.h"
#include "equipment_demo.h"
namespace panel_web {
enum class DemoActionType : uint8_t { Enable=1, Disable=2, Outcome=3, Reset=4 };
struct DemoAction { DemoActionType type=DemoActionType::Reset; uint8_t id=0; uint8_t outcome=0; };
bool begin();
void publish(const panel::Measurement (&temps)[panel::CHANNELS],
             const panel::FuelLevel &fuel,
             const panel::equipment_demo::State &equipment,
             uint32_t atMs);
bool takeDemoAction(DemoAction& out);
void recordDemoResult(bool accepted);
bool armPairing();  // Physically invoked from panel Settings, never by HTTP.
bool pairingCode(char out[7],uint32_t& secondsRemaining); // LVGL display ONLY.
void revokePairing(); // Physically invoked from panel Settings.
} // namespace panel_web
