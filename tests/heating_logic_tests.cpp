#include "panel_heating_logic.h"
#include <cassert>
#include <initializer_list>
#include <cstdio>

using namespace panel_heating;

#ifdef HEATING_TEST_RUST
#include "panel_heating_rust.h"
using TestController = panel_heating::RustController;
#else
#include "heating_reference.h"
using TestController = panel_heating::Controller;
#endif

struct Plant {
  TestController controller;
  Config config;
  Inputs in;
  Plant() {
    config.roomForLoop[0] = 1; config.roomForLoop[8] = 1;
    config.roomForLoop[1] = 2; config.roomForLoop[9] = 3;
    // Water limits/differentials below are test fixtures, not plant defaults.
    config.supplyTargetC = 60; config.returnTargetC = 45;
    in.presence = Presence::Away; in.season = Season::Winter;
    in.diesel = in.electric = Health::Ready; in.circulation = Flag::Yes;
    in.supplyC = 40; in.returnC = 30;
    in.roomC[0] = 19; in.roomC[1] = in.roomC[2] = 23;
  }
  Output tick(uint32_t time = 0) { return controller.tick(time, config, in); }
};

static void rooms() {
  Plant p;
  p.config.roomForLoop[7] = 1; // A second floor loop in the same room.
  p.in.roomC[0] = p.in.roomC[1] = p.in.roomC[2] = 21;
  auto out = p.tick();
  assert(out.demand == Flag::Unknown && out.source == Source::Unknown && out.stages == -1);
  p.in.roomC[0] = p.in.roomC[1] = p.in.roomC[2] = 19;
  out = p.tick();
  for (unsigned i : {0u, 7u, 8u, 1u, 9u}) assert(out.loop[i] == Flag::Yes);
  assert(out.demand == Flag::Yes && out.loop[6] == Flag::Unknown);
  p.in.roomC[0] = p.in.roomC[1] = p.in.roomC[2] = 22;
  out = p.tick();
  assert(out.loop[0] == Flag::Yes && out.loop[7] == Flag::Yes && out.loop[8] == Flag::No);
  assert(out.loop[1] == Flag::Yes && out.loop[9] == Flag::Yes);
  p.in.roomC[0] = 21.5f; p.in.roomC[1] = p.in.roomC[2] = 23;
  out = p.tick();
  assert(out.loop[8] == Flag::No && out.loop[0] == Flag::Yes && out.demand == Flag::Yes);
  assert(out.loop[1] == Flag::No && out.loop[9] == Flag::No);
  p.in.roomC[0] = 24;
  out = p.tick();
  assert(out.demand == Flag::No && out.source == Source::None);
  p.in.roomC[0] = NAN;
  assert(p.tick().demand == Flag::Unknown);
  p.in.roomC[1] = 18;
  assert(p.tick().demand == Flag::Yes); // Missing room 1 does not block room 2.
  p.config.roomForLoop[1] = 15;
  assert(p.tick().demand == Flag::Unknown);
  p.in.season = Season::Summer;
  assert(p.tick().demand == Flag::No && p.tick().stages == 0);
}

static void retuneRooms() {
  Plant p;
  assert(p.tick().loop[0] == Flag::Yes); // Cold at 19.
  p.in.roomC[0] = 21.5f;
  auto out = p.tick();
  assert(out.loop[0] == Flag::Yes && out.loop[8] == Flag::Yes);
  p.config.roomOnC = 20; p.config.roomRadiatorOffC = 23; p.config.roomOffC = 24;
  out = p.tick();
  assert(out.loop[0] == Flag::Yes && out.loop[8] == Flag::Yes && out.demand == Flag::Yes);
  p.in.roomC[0] = 23;
  out = p.tick();
  assert(out.loop[0] == Flag::Yes && out.loop[8] == Flag::No);
  p.in.roomC[0] = 24;
  out = p.tick();
  assert(out.loop[0] == Flag::No && out.loop[8] == Flag::No);

  p.in.roomC[0] = 20; p.tick();
  p.in.roomC[0] = 22.5f;
  assert(p.tick().loop[8] == Flag::Yes);
  p.config.roomOnC = 19; p.config.roomRadiatorOffC = 22; p.config.roomOffC = 23;
  out = p.tick(); // Crosses the new middle threshold without a change of reading.
  assert(out.loop[0] == Flag::Yes && out.loop[8] == Flag::No);
  p.config.roomOnC = 18; p.config.roomRadiatorOffC = 21; p.config.roomOffC = 22;
  out = p.tick();
  assert(out.loop[0] == Flag::No && out.loop[8] == Flag::No);

  p.in.roomC[0] = p.in.roomC[1] = 18; p.tick();
  p.in.roomC[0] = p.in.roomC[1] = 20;
  p.config.roomForLoop[7] = 1; // Added another floor loop, same set of emitter types.
  out = p.tick();
  assert(out.loop[0] == Flag::Unknown && out.loop[7] == Flag::Unknown && out.loop[8] == Flag::Unknown);
  assert(out.loop[1] == Flag::Yes);
  p.in.roomC[1] = NAN; assert(p.tick().loop[1] == Flag::Unknown);
  p.in.roomC[1] = 20; assert(p.tick().loop[1] == Flag::Unknown);
}

static void sources() {
  Plant p;
  p.in.presence = Presence::Home;
  assert(p.tick().source == Source::Diesel);
  p.in.diesel = Health::Unknown;
  assert(p.tick().source == Source::Unknown && p.tick().alarm == Flag::Unknown);
  p.in.diesel = Health::Fault;
  assert(p.tick().source == Source::Electric && p.tick().alarm == Flag::Yes);
  p.in.diesel = Health::Ready;
  assert(p.tick().source == Source::Electric); // No automatic handback mid-request.
  p.in.roomC[0] = 23;
  assert(p.tick().source == Source::None);
  p.in.roomC[0] = 19;
  assert(p.tick().source == Source::Diesel);
  p.in.roomC[0] = 21; p.in.presence = Presence::Away; p.in.electric = Health::Fault;
  assert(p.tick().demand == Flag::Yes && p.tick().source == Source::None && p.tick().alarm == Flag::Yes);
  p.config.allowAwayDiesel = true;
  assert(p.tick().source == Source::Diesel);
  p.config.allowAwayDiesel = false;
  assert(p.tick().source == Source::None);
  p.in.presence = Presence::Unknown;
  assert(p.tick().tankPower == Flag::Unknown && p.tick().source == Source::Unknown);
}

static void electric() {
  Plant p;
  const uint32_t t = UINT32_MAX - 1000U;
  assert(p.tick(t - 1).stages == 3);
  p.in.supplyC = 60;
  assert(p.tick(t).stages == 2);
  assert(p.tick(t + SHED_INTERVAL_MS - 1).stages == 2);
  assert(p.tick(t + SHED_INTERVAL_MS).stages == 1);
  assert(p.tick(t + 2 * SHED_INTERVAL_MS).stages == 1);
  p.in.returnC = 45;
  assert(p.tick(t + 2 * SHED_INTERVAL_MS + 1).stages == 0);
  p.in.returnC = 35; p.in.supplyC = 45;
  assert(p.tick(t + 2 * SHED_INTERVAL_MS + 2).stages == -1);
  p.config.returnRestartC = 38;
  assert(p.tick(t + 2 * SHED_INTERVAL_MS + 3).stages == -1); // Reheat method unknown.
  p.config.reheat = Reheat::OneByOne; p.config.reheatStepMs = 2000;
  const uint32_t s = t + 2 * SHED_INTERVAL_MS + 4;
  assert(p.tick(s).stages == 1);
  assert(p.tick(s + 1999).stages == 1);
  assert(p.tick(s + 2000).stages == 2);
  assert(p.tick(s + 4000).stages == 3);
  p.in.returnC = 45; p.in.supplyC = NAN;
  assert(p.tick(s + 4001).stages == 0); // Hot return wins even if supply reading is lost.
  p.config.reheat = Reheat::All; p.in.returnC = 39; p.in.supplyC = 40;
  assert(p.tick(s + 4002).stages == 0);
  p.in.returnC = 38; p.in.circulation = Flag::No;
  assert(p.tick(s + 4003).stages == 0);
  p.in.circulation = Flag::Yes;
  assert(p.tick(s + 4004).stages == 3);
  p.in.supplyC = 60;
  assert(p.tick(s + 4005).stages == 2);
  p.in.returnC = 40; // Above restart: a recovered sample cannot increase power by itself.
  p.in.supplyC = NAN;
  assert(p.tick(s + 4006).stages == -1);
  p.in.supplyC = 55;
  assert(p.tick(s + 4007).stages == 2);
  p.in.circulation = Flag::No;
  assert(p.tick(s + 4008).stages == 0);
  p.in.circulation = Flag::Yes;
  assert(p.tick(s + 4009).stages == 2);
  p.in.returnC = 45;
  assert(p.tick(s + 4010).stages == 0); // No five-minute wait to stop the remaining two.
}

static void partialRecovery() {
  Plant p;
  p.config.returnRestartC = 35;
  p.config.reheat = Reheat::OneByOne;
  p.config.reheatStepMs = 1000;
  assert(p.tick().stages == 3);
  p.in.supplyC = 60; p.in.returnC = 40;
  assert(p.tick(100).stages == 2);
  assert(p.tick(100 + SHED_INTERVAL_MS).stages == 1);
  p.in.returnC = 34; p.in.supplyC = 55;
  const uint32_t s = SHED_INTERVAL_MS + 200;
  assert(p.tick(s).stages == 1);
  assert(p.tick(s + 999).stages == 1);
  assert(p.tick(s + 1000).stages == 2);
  assert(p.tick(s + 2000).stages == 3);
  p.in.supplyC = 60;
  assert(p.tick(s + 2001).stages == 3); // Shedding events stay five minutes apart.
  p.in.returnC = 45;
  assert(p.tick(s + 2002).stages == 0); // Return target ignores the shedding timer.
  p.in.roomC[0] = 23; p.in.returnC = 43;
  assert(p.tick(s + 2003).source == Source::None);
  p.in.roomC[0] = 19; p.in.supplyC = 50;
  assert(p.tick(s + 2004).stages == 0); // A new room request cannot bypass the restart threshold.
  p.in.returnC = 34;
  assert(p.tick(s + 2005).stages == 1);
  p.in.circulation = Flag::Unknown;
  assert(p.tick(s + 2100).stages == -1);
  p.in.circulation = Flag::Yes;
  assert(p.tick(s + 5000).stages == 1); // Interrupted data cannot satisfy a dwell.
  assert(p.tick(s + 6000).stages == 2);
  p.config.reheat = Reheat::All;
  assert(p.tick(s + 6001).stages == 3); // Low return also applies when two stages remain on.
}

static void tank() {
  Plant p;
  p.in.roomC[0] = 23; p.in.presence = Presence::Home;
  p.config.tankOnC = 40; p.config.tankOffC = 52;
  p.config.coilStartDeltaC = 6; p.config.coilStopDeltaC = 2;
  p.in.tankC = 39; p.in.coilSupplyC = 48;
  p.in.plantHeatAvailable = Flag::No;
  auto out = p.tick();
  assert(out.tankPower == Flag::Yes && out.coilPump == Flag::No && out.source == Source::None);
  p.in.plantHeatAvailable = Flag::Yes;
  assert(p.tick().coilPump == Flag::Yes);
  p.in.tankC = 44; p.in.coilSupplyC = 47;
  assert(p.tick().coilPump == Flag::Yes);
  p.in.coilSupplyC = 46;
  assert(p.tick().coilPump == Flag::No);
  p.in.coilSupplyC = 52;
  assert(p.tick().coilPump == Flag::Yes);
  p.in.tankC = 52;
  assert(p.tick().coilPump == Flag::No);
  p.in.tankC = 39; p.in.coilSupplyC = NAN;
  assert(p.tick().coilPump == Flag::Unknown && p.tick().tankPower == Flag::Yes);
  p.in.season = Season::Summer;
  assert(p.tick().coilPump == Flag::No && p.tick().tankPower == Flag::Yes);
  p.in.presence = Presence::Away; p.in.season = Season::Unknown;
  assert(p.tick().tankPower == Flag::No && p.tick().coilPump == Flag::No);
}

int main() {
  rooms(); retuneRooms(); sources(); electric(); partialRecovery(); tank();
  std::puts("Rooms, source selection, electric stages and DHW decisions passed.");
}
