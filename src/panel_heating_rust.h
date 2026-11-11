#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "panel_heating_logic.h"

namespace panel_heating {
namespace rust_abi {
// Fixed-width scalars cross the C ABI. C++ enums and bool do not.
struct RawConfig {
  uint8_t room_for_loop[14], allow_away_diesel, reheat;
  float room_on_c, room_radiator_off_c, room_off_c;
  float supply_target_c, return_target_c, return_restart_c;
  float tank_on_c, tank_off_c, coil_start_delta_c, coil_stop_delta_c;
  uint32_t reheat_step_ms;
};
struct RawInputs {
  float room_c[14], supply_c, return_c, tank_c, coil_supply_c;
  uint8_t presence, season, diesel, electric, circulation, plant_heat_available;
};
struct RawOutput {
  uint8_t loops[14], demand, alarm, tank_power, coil_pump, source;
  int8_t stages;
};
static_assert(ROOM_COUNT == 14 && LOOP_COUNT == 14, "Rust control ABI dimensions");
static_assert(sizeof(RawConfig) == 60 && offsetof(RawConfig, room_on_c) == 16 &&
              offsetof(RawConfig, reheat_step_ms) == 56, "Rust config ABI");
static_assert(sizeof(RawInputs) == 80 && offsetof(RawInputs, presence) == 72, "Rust input ABI");
static_assert(sizeof(RawOutput) == 20 && offsetof(RawOutput, stages) == 19, "Rust output ABI");

extern "C" void boiler_heating_init(void* state);
extern "C" void boiler_heating_tick(void* state, uint32_t now, const RawConfig* config,
                                    const RawInputs* inputs, RawOutput* output);
// Historical NVS v1 payload, shared with previous panel firmware.
struct SettingsRecord { uint8_t bytes[56]; };
static_assert(sizeof(SettingsRecord) == 56, "Rust v1 settings ABI");
extern "C" uint8_t boiler_heating_settings_encode(const RawConfig* config, uint8_t season,
                                                   uint32_t revision, SettingsRecord* record);
extern "C" uint8_t boiler_heating_settings_decode(const SettingsRecord* record, RawConfig* config,
                                                   uint8_t* season, uint32_t* revision);

inline RawConfig toRaw(const Config& c) {
  RawConfig config{
    {}, uint8_t(c.allowAwayDiesel), uint8_t(c.reheat),
    c.roomOnC, c.roomRadiatorOffC, c.roomOffC,
    c.supplyTargetC, c.returnTargetC, c.returnRestartC,
    c.tankOnC, c.tankOffC, c.coilStartDeltaC, c.coilStopDeltaC,
    c.reheatStepMs
  };
  memcpy(config.room_for_loop, c.roomForLoop, sizeof(config.room_for_loop));
  return config;
}

inline Config fromRaw(const RawConfig& raw) {
  Config config{};
  memcpy(config.roomForLoop, raw.room_for_loop, sizeof(config.roomForLoop));
  config.allowAwayDiesel = raw.allow_away_diesel != 0;
  config.reheat = Reheat(raw.reheat);
  config.roomOnC = raw.room_on_c;
  config.roomRadiatorOffC = raw.room_radiator_off_c;
  config.roomOffC = raw.room_off_c;
  config.supplyTargetC = raw.supply_target_c;
  config.returnTargetC = raw.return_target_c;
  config.returnRestartC = raw.return_restart_c;
  config.tankOnC = raw.tank_on_c;
  config.tankOffC = raw.tank_off_c;
  config.coilStartDeltaC = raw.coil_start_delta_c;
  config.coilStopDeltaC = raw.coil_stop_delta_c;
  config.reheatStepMs = raw.reheat_step_ms;
  return config;
}
} // namespace rust_abi

class RustController {
 public:
  RustController() { reset(); }
  RustController(const RustController&) = delete;
  RustController& operator=(const RustController&) = delete;

  void reset() { rust_abi::boiler_heating_init(state_); }

  Output tick(uint32_t now, const Config& c, const Inputs& in) {
    const auto config = rust_abi::toRaw(c);
    rust_abi::RawInputs inputs{
      {}, in.supplyC, in.returnC, in.tankC, in.coilSupplyC,
      uint8_t(in.presence), uint8_t(in.season), uint8_t(in.diesel), uint8_t(in.electric),
      uint8_t(in.circulation), uint8_t(in.plantHeatAvailable)
    };
    memcpy(inputs.room_c, in.roomC, sizeof(inputs.room_c));
    rust_abi::RawOutput raw{};
    rust_abi::boiler_heating_tick(state_, now, &config, &inputs, &raw);
    Output out;
    for (unsigned i = 0; i < LOOP_COUNT; ++i) out.loop[i] = Flag(raw.loops[i]);
    out.demand = Flag(raw.demand);
    out.alarm = Flag(raw.alarm);
    out.tankPower = Flag(raw.tank_power);
    out.coilPump = Flag(raw.coil_pump);
    out.source = Source(raw.source);
    out.stages = raw.stages;
    return out;
  }

 private:
  // Rust checks the size/alignment of its concrete controller at compile time.
  alignas(4) unsigned char state_[128];
};
} // namespace panel_heating
