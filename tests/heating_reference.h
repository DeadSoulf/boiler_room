#pragma once
// Behavioral oracle from the v0.11.0 release for cross-language regression tests.
#include "panel_heating_logic.h"

namespace panel_heating {
class Controller {
 public:
  Output tick(uint32_t now, const Config& config, const Inputs& in) {
    Output out;
    evaluateRooms(config, in, out);
    chooseSource(config, in, out);
    out.stages = electricStages(now, config, in, out.source);
    out.tankPower = in.presence == Presence::Home ? Flag::Yes :
                    in.presence == Presence::Away ? Flag::No : Flag::Unknown;
    out.coilPump = tankPump(config, in, out.tankPower);
    return out;
  }

 private:
  static constexpr uint16_t FLOOR_LOOPS = (1U << FLOOR_COUNT) - 1U;
  struct Room {
    uint16_t loops = 0;
    Flag floor = Flag::Unknown, radiator = Flag::Unknown;
  };
  Room rooms_[ROOM_COUNT] = {};
  Presence lastPresence_ = Presence::Unknown;
  bool onBackup_ = false;

  int8_t activeStages_ = 0;
  bool returnSatisfied_ = false, shedRecorded_ = false, reheatTimer_ = false;
  uint32_t lastShedAt_ = 0, reheatFrom_ = 0;
  Flag tankDemand_ = Flag::Unknown, coilHot_ = Flag::Unknown;
  float tankLow_ = NAN, tankHigh_ = NAN, startDelta_ = NAN, stopDelta_ = NAN;

  static bool ordered(float low, float high) {
    return isfinite(low) && isfinite(high) && low < high;
  }
  static bool elapsed(uint32_t now, uint32_t since, uint32_t duration) {
    return uint32_t(now - since) >= duration;
  }

  void evaluateRooms(const Config& c, const Inputs& in, Output& out) {
    uint16_t loops[ROOM_COUNT] = {};
    bool assigned = false, invalidMap = false;
    for (unsigned i = 0; i < LOOP_COUNT; ++i) {
      const unsigned id = c.roomForLoop[i];
      if (!id) continue;
      if (id > ROOM_COUNT) { invalidMap = true; continue; }
      assigned = true;
      loops[id - 1] |= uint16_t(1U << i);
    }
    const bool validThresholds = ordered(c.roomOnC, c.roomRadiatorOffC) &&
                                 ordered(c.roomRadiatorOffC, c.roomOffC);
    for (unsigned n = 0; n < ROOM_COUNT; ++n) {
      Room& room = rooms_[n];
      // A new setpoint applies immediately; only a changed circuit assignment invalidates this room's history.
      if (room.loops != loops[n]) room = Room{};
      room.loops = loops[n];
      if (in.season == Season::Summer) {
        room.floor = room.radiator = Flag::No;
      } else if (in.season != Season::Winter || !validThresholds || !isfinite(in.roomC[n])) {
        room.floor = room.radiator = Flag::Unknown;
      } else if (in.roomC[n] >= c.roomOffC) {
        room.floor = room.radiator = Flag::No;
      } else if (in.roomC[n] <= c.roomOnC) {
        room.floor = room.radiator = Flag::Yes;
      } else if ((room.loops & FLOOR_LOOPS) && (room.loops >> FLOOR_COUNT) &&
                 in.roomC[n] >= c.roomRadiatorOffC) {
        room.radiator = Flag::No;
      }
    }
    bool anyOn = false, anyUnknown = invalidMap;
    for (unsigned i = 0; i < LOOP_COUNT; ++i) {
      const unsigned id = c.roomForLoop[i];
      if (!id || id > ROOM_COUNT) continue;
      const Room& room = rooms_[id - 1];
      out.loop[i] = i < FLOOR_COUNT ? room.floor : room.radiator;
      anyOn |= out.loop[i] == Flag::Yes;
      anyUnknown |= out.loop[i] == Flag::Unknown;
    }
    if (in.season == Season::Summer) out.demand = Flag::No;
    else if (in.season == Season::Winter && assigned && !invalidMap) {
      if (anyOn) out.demand = Flag::Yes;
      else if (!anyUnknown) out.demand = Flag::No;
    }
  }

  void chooseSource(const Config& c, const Inputs& in, Output& out) {
    if (in.presence != lastPresence_ || out.demand == Flag::No) onBackup_ = false;
    lastPresence_ = in.presence;
    const bool home = in.presence == Presence::Home;
    const bool away = in.presence == Presence::Away;
    const Health primary = home ? in.diesel : in.electric;
    if (home || away) {
      if (primary == Health::Fault) out.alarm = Flag::Yes;
      if (primary == Health::Ready) out.alarm = Flag::No;
    }
    if (out.demand == Flag::No) { out.source = Source::None; return; }
    if (out.demand != Flag::Yes || (!home && !away)) return;

    // Once backup was selected for this request, do not hand the plant back
    // mid-cycle merely because the primary's fault contact has cleared.
    if (!onBackup_ && primary == Health::Ready) {
      out.source = home ? Source::Diesel : Source::Electric;
      return;
    }
    if (!onBackup_ && primary != Health::Fault) return;
    if (away && !c.allowAwayDiesel) { out.source = Source::None; onBackup_ = false; return; }
    const Health backup = home ? in.electric : in.diesel;
    if (backup == Health::Fault) { out.source = Source::None; out.alarm = Flag::Yes; return; }
    if (backup == Health::Ready) {
      onBackup_ = true;
      out.source = home ? Source::Electric : Source::Diesel;
    }
  }

  void idleStages() {
    activeStages_ = 0;
    shedRecorded_ = false;
    reheatTimer_ = false;
  }

  int8_t electricStages(uint32_t now, const Config& c, const Inputs& in, Source source) {
    const bool returnKnown = isfinite(in.returnC) && isfinite(c.returnTargetC);
    if (returnKnown && in.returnC >= c.returnTargetC) {
      returnSatisfied_ = true;
      idleStages(); return 0;
    }
    if (source == Source::None || source == Source::Diesel) {
      idleStages(); return 0;
    }
    if (source != Source::Electric || in.circulation != Flag::Yes ||
        !returnKnown || !isfinite(in.supplyC) || !isfinite(c.supplyTargetC)) {
      // Suspend the request and timer. Recovering a sample or a flow contact
      // must not manufacture a new cold start at 9 kW mid-cycle.
      lastShedAt_ = now; reheatTimer_ = false;
      return in.circulation == Flag::No ? 0 : -1;
    }
    const bool hotSupply = in.supplyC >= c.supplyTargetC;
    const bool hasRestart = ordered(c.returnRestartC, c.returnTargetC);
    const bool lowReturn = hasRestart && in.returnC <= c.returnRestartC;
    const bool timedRise = c.reheat == Reheat::OneByOne && c.reheatStepMs && c.reheatStepMs <= INT32_MAX;
    if (!activeStages_) {
      if (hotSupply) return 0;
      if (hasRestart && !lowReturn) return 0;
      if (returnSatisfied_) {
        if (!hasRestart || (c.reheat != Reheat::All && !timedRise)) return -1;
        activeStages_ = timedRise ? 1 : 3;
        returnSatisfied_ = false; reheatTimer_ = timedRise; reheatFrom_ = now;
      } else {
        activeStages_ = 3; // Cold start.
      }
    } else if (hotSupply) {
      reheatTimer_ = false;
      if (activeStages_ > 1 && (!shedRecorded_ || elapsed(now, lastShedAt_, SHED_INTERVAL_MS))) {
        --activeStages_; lastShedAt_ = now; shedRecorded_ = true;
      }
    } else if (activeStages_ < 3 && lowReturn) {
      // A cold branch may open before the return ever reaches its upper target.
      // Apply the configured recovery policy from the current number of stages.
      if (c.reheat == Reheat::All) {
        activeStages_ = 3; reheatTimer_ = false;
      } else if (timedRise) {
        if (!reheatTimer_) { reheatTimer_ = true; reheatFrom_ = now; }
        else if (elapsed(now, reheatFrom_, c.reheatStepMs)) {
          ++activeStages_; reheatFrom_ = now;
        }
      } else reheatTimer_ = false;
    } else reheatTimer_ = false;
    return activeStages_;
  }

  Flag tankPump(const Config& c, const Inputs& in, Flag power) {
    const bool retuned = tankLow_ != c.tankOnC || tankHigh_ != c.tankOffC ||
                         startDelta_ != c.coilStartDeltaC || stopDelta_ != c.coilStopDeltaC;
    tankLow_ = c.tankOnC; tankHigh_ = c.tankOffC;
    startDelta_ = c.coilStartDeltaC; stopDelta_ = c.coilStopDeltaC;
    if (retuned || power != Flag::Yes || in.season != Season::Winter) {
      tankDemand_ = coilHot_ = Flag::Unknown;
    }
    if (power == Flag::No || in.season == Season::Summer) return Flag::No;
    if (power == Flag::Unknown || in.season != Season::Winter) return Flag::Unknown;
    if (!ordered(c.tankOnC,c.tankOffC) || !isfinite(in.tankC)) tankDemand_ = Flag::Unknown;
    else if (in.tankC <= c.tankOnC) tankDemand_ = Flag::Yes;
    else if (in.tankC >= c.tankOffC) tankDemand_ = Flag::No;
    if (in.plantHeatAvailable != Flag::Yes || !ordered(c.coilStopDeltaC,c.coilStartDeltaC) ||
        c.coilStopDeltaC < 0 || !isfinite(in.coilSupplyC) || !isfinite(in.tankC)) {
      coilHot_ = Flag::Unknown;
    } else {
      const float delta = in.coilSupplyC - in.tankC;
      if (delta >= c.coilStartDeltaC) coilHot_ = Flag::Yes;
      if (delta <= c.coilStopDeltaC) coilHot_ = Flag::No;
    }
    if (in.plantHeatAvailable == Flag::No || tankDemand_ == Flag::No || coilHot_ == Flag::No) return Flag::No;
    if (tankDemand_ == Flag::Yes && coilHot_ == Flag::Yes) return Flag::Yes;
    return Flag::Unknown;
  }
};
} // namespace panel_heating
