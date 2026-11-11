#pragma once
// Logical requests only. Input adapters supply fresh readings or unknown values.
// No output of this controller represents feedback from a pump, valve or heater.
#include <math.h>
#include <stdint.h>

namespace panel_heating {
constexpr unsigned ROOM_COUNT = 14;
constexpr unsigned FLOOR_COUNT = 8;
constexpr unsigned LOOP_COUNT = 14;
constexpr uint32_t SHED_INTERVAL_MS = 5U * 60U * 1000U;

enum class Flag : uint8_t { Unknown, No, Yes };
enum class Presence : uint8_t { Unknown, Home, Away };
enum class Season : uint8_t { Unknown, Summer, Winter };
enum class Health : uint8_t { Unknown, Ready, Fault };
enum class Source : uint8_t { Unknown, None, Diesel, Electric };
enum class Reheat : uint8_t { Unset, All, OneByOne };

struct Config {
  uint8_t roomForLoop[LOOP_COUNT] = {}; // Room IDs 1..14. Zero means unassigned.
  float roomOnC = 19, roomRadiatorOffC = 22, roomOffC = 23;
  float supplyTargetC = NAN, returnTargetC = NAN, returnRestartC = NAN;
  float tankOnC = NAN, tankOffC = NAN, coilStartDeltaC = NAN, coilStopDeltaC = NAN;
  bool allowAwayDiesel = false;
  Reheat reheat = Reheat::Unset;
  uint32_t reheatStepMs = 0;
};

struct Inputs {
  float roomC[ROOM_COUNT];
  float supplyC = NAN, returnC = NAN, tankC = NAN, coilSupplyC = NAN;
  Presence presence = Presence::Unknown;
  Season season = Season::Unknown;
  Health diesel = Health::Unknown, electric = Health::Unknown;
  Flag circulation = Flag::Unknown, plantHeatAvailable = Flag::Unknown;
  Inputs() {
    for (auto& value : roomC) value = NAN;
  }
};

struct Output {
  Flag loop[LOOP_COUNT] = {};
  Flag demand = Flag::Unknown, alarm = Flag::Unknown;
  Flag tankPower = Flag::Unknown, coilPump = Flag::Unknown;
  Source source = Source::Unknown;
  int8_t stages = -1; // Number of requested 3 kW stages. -1 means undetermined.
};

} // namespace panel_heating
