// Compare every externally visible decision at every tick using the production C++/Rust bridge.
#include "heating_reference.h"
#include "panel_heating_rust.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace panel_heating;

namespace {
unsigned samples = 0;

struct Comparison {
  Controller reference;
  RustController rust;
  const char* name;
  unsigned trace;
  unsigned step = 0;

  Comparison(const char* name, unsigned trace) : name(name), trace(trace) {}

  void reset() {
    reference = Controller{};
    rust.reset();
  }

  template<class T>
  void same(uint32_t now, const char* field, T expected, T actual, int loop = -1) const {
    if (expected == actual) return;
    std::fprintf(stderr, "%s trace=%u step=%u time=%u field=%s loop=%d C++=%d Rust=%d\n",
                 name, trace, step, static_cast<unsigned>(now), field, loop,
                 static_cast<int>(expected), static_cast<int>(actual));
    std::exit(EXIT_FAILURE);
  }

  void tick(uint32_t now, const Config& config, const Inputs& in) {
    const Output expected = reference.tick(now, config, in);
    const Output actual = rust.tick(now, config, in);
    for (unsigned i = 0; i < LOOP_COUNT; ++i) {
      same(now, "loop", expected.loop[i], actual.loop[i], static_cast<int>(i));
    }
    same(now, "demand", expected.demand, actual.demand);
    same(now, "source", expected.source, actual.source);
    same(now, "stages", expected.stages, actual.stages);
    same(now, "alarm", expected.alarm, actual.alarm);
    same(now, "tankPower", expected.tankPower, actual.tankPower);
    same(now, "coilPump", expected.coilPump, actual.coilPump);
    ++step;
    ++samples;
  }
};

// The temperatures and contacts in this fixture are synthetic test inputs.
struct Plant {
  Config config;
  Inputs in;

  Plant() {
    for (unsigned i = 0; i < LOOP_COUNT; ++i) config.roomForLoop[i] = i + 1;
    for (auto& t : in.roomC) t = 18;
    config.supplyTargetC = 60;
    config.returnTargetC = 45;
    config.returnRestartC = 38;
    config.tankOnC = 40;
    config.tankOffC = 52;
    config.coilStartDeltaC = 6;
    config.coilStopDeltaC = 2;
    config.reheat = Reheat::OneByOne;
    config.reheatStepMs = 5000;
    in.season = Season::Winter;
    in.presence = Presence::Away;
    in.diesel = in.electric = Health::Ready;
    in.circulation = in.plantHeatAvailable = Flag::Yes;
    in.supplyC = 40;
    in.returnC = 34;
    in.tankC = 39;
    in.coilSupplyC = 48;
  }
};

void roomAssignments() {
  Comparison test("room-assignments", 0);
  Plant p;
  uint32_t now = 0;
  for (unsigned i = 0; i < ROOM_COUNT; ++i) {
    for (auto& t : p.in.roomC) t = 24;
    for (float t : {18.f, 19.f, 21.f, 22.f, 21.f, 23.f, NAN, 21.f, 19.f}) {
      p.in.roomC[i] = t;
      test.tick(now++, p.config, p.in);
    }
  }
  for (unsigned i = 0; i < LOOP_COUNT; ++i) {
    p.config.roomForLoop[i] = static_cast<uint8_t>(1 + i % FLOOR_COUNT);
  }
  for (float t : {19.f, 20.f, 22.f, 21.f, 23.f}) {
    for (auto& value : p.in.roomC) value = t;
    test.tick(now++, p.config, p.in);
  }
  for (unsigned i = 0; i < LOOP_COUNT; ++i) {
    p.config.roomForLoop[i] = 0;
    test.tick(now++, p.config, p.in);
    p.config.roomForLoop[i] = 255;
    test.tick(now++, p.config, p.in);
    p.config.roomForLoop[i] = ROOM_COUNT;
    test.tick(now++, p.config, p.in);
  }
}

void sourceAndTankContacts() {
  Comparison test("source-and-tank", 0);
  Plant p;
  uint32_t now = 0;
  for (bool allow : {false, true}) {
    p.config.allowAwayDiesel = allow;
    for (auto season : {Season::Unknown, Season::Winter, Season::Summer}) {
      p.in.season = season;
      for (auto presence : {Presence::Unknown, Presence::Home, Presence::Away}) {
        p.in.presence = presence;
        for (auto diesel : {Health::Unknown, Health::Ready, Health::Fault}) {
          p.in.diesel = diesel;
          for (auto electric : {Health::Unknown, Health::Ready, Health::Fault}) {
            p.in.electric = electric;
            for (auto heat : {Flag::Unknown, Flag::No, Flag::Yes}) {
              p.in.plantHeatAvailable = heat;
              test.reset();
              test.tick(now++, p.config, p.in);
              // Same contacts during recovery/retuning must retain the right hysteresis.
              for (float tank : {40.f, 46.f, 52.f, 44.f, NAN, 39.f}) {
                p.in.tankC = tank;
                for (float delta : {2.f, 4.f, 6.f}) {
                  p.in.coilSupplyC = tank + delta;
                  test.tick(now++, p.config, p.in);
                }
              }
            }
          }
        }
      }
    }
  }
}

template<class T, std::size_t N>
T pick(std::mt19937& random, const T (&values)[N]) {
  return values[random() % N];
}

void mutate(Plant& p, std::mt19937& random) {
  static const float room[] = {18.f, 19.f, 19.01f, 21.99f, 22.f, 22.99f, 23.f, 25.f, NAN, INFINITY};
  static const float water[] = {29.f, 34.f, 37.99f, 38.f, 40.f, 44.99f, 45.f, 52.f, 59.99f, 60.f, 72.f, NAN, -INFINITY};
  static const float delta[] = {-1.f, 0.f, 2.f, 6.f, 9.f, NAN};
  static const uint32_t interval[] = {0, 1, 4999, 5000, 300000, 0x7fffffff, 0x80000000, UINT32_MAX};
  const unsigned index = random() % ROOM_COUNT;
  const unsigned action = random() % 29;
  switch (action) {
    case 0: case 1: case 2: case 3: p.in.roomC[index] = pick(random, room); break;
    case 4: case 5: p.in.supplyC = pick(random, water); break;
    case 6: case 7: p.in.returnC = pick(random, water); break;
    case 8: p.in.tankC = pick(random, water); break;
    case 9: p.in.coilSupplyC = pick(random, water); break;
    case 10: p.in.presence = static_cast<Presence>(random() % 3); break;
    case 11: p.in.season = static_cast<Season>(random() % 3); break;
    case 12: p.in.diesel = static_cast<Health>(random() % 3); break;
    case 13: p.in.electric = static_cast<Health>(random() % 3); break;
    case 14: p.in.circulation = static_cast<Flag>(random() % 3); break;
    case 15: p.in.plantHeatAvailable = static_cast<Flag>(random() % 3); break;
    case 16: p.config.roomForLoop[index] = static_cast<uint8_t>(random() % (ROOM_COUNT + 2)); break;
    case 17: p.config.roomOnC = pick(random, room); break;
    case 18: p.config.roomRadiatorOffC = pick(random, room); break;
    case 19: p.config.roomOffC = pick(random, room); break;
    case 20: p.config.supplyTargetC = pick(random, water); break;
    case 21: p.config.returnRestartC = pick(random, water); break;
    case 22: p.config.returnTargetC = pick(random, water); break;
    case 23: p.config.tankOnC = pick(random, water); break;
    case 24: p.config.tankOffC = pick(random, water); break;
    case 25: p.config.coilStartDeltaC = pick(random, delta); break;
    case 26: p.config.coilStopDeltaC = pick(random, delta); break;
    case 27: p.config.allowAwayDiesel = !p.config.allowAwayDiesel; break;
    case 28:
      p.config.reheat = static_cast<Reheat>(random() % 3);
      p.config.reheatStepMs = pick(random, interval);
      break;
  }
}

void electricClocks() {
  Plant p;
  uint32_t now = UINT32_MAX - 1000;
  for (const auto mode : {Reheat::Unset, Reheat::All, Reheat::OneByOne}) {
    for (const uint32_t dwell : {0u, 1u, 5000u, 0x7fffffffu, 0x80000000u}) {
      Comparison test("electric-clock", static_cast<unsigned>(mode));
      p.config.reheat = mode;
      p.config.reheatStepMs = dwell;
      p.in.supplyC = 40; p.in.returnC = 34;
      test.tick(now, p.config, p.in);
      p.in.supplyC = 60;
      test.tick(++now, p.config, p.in);
      now += SHED_INTERVAL_MS - 1;
      test.tick(now, p.config, p.in);
      test.tick(++now, p.config, p.in);
      p.in.supplyC = 40;
      test.tick(++now, p.config, p.in);
      now += dwell;
      test.tick(now, p.config, p.in);
      p.in.returnC = 45;
      test.tick(++now, p.config, p.in);
      p.in.returnC = 38;
      test.tick(++now, p.config, p.in);
      now += dwell - 1;
      test.tick(now, p.config, p.in);
      test.tick(++now, p.config, p.in);
      p.in.circulation = Flag::Unknown;
      test.tick(++now, p.config, p.in);
      p.in.circulation = Flag::Yes;
      test.tick(++now, p.config, p.in);
      now += dwell;
      test.tick(now, p.config, p.in);
    }
  }
}

void interleavedTraces() {
  // Two simultaneous callers detect shared Rust storage or a reset of the wrong instance.
  static const uint32_t elapsed[] = {0, 1, 30, 999, 4999, 5000, 299999, 300000, 300001, 0x80000001};
  for (unsigned trial = 0; trial < 64; ++trial) {
    Comparison first("interleaved-A", trial), second("interleaved-B", trial);
    std::mt19937 random(0xd4e1u + trial);
    uint32_t now = UINT32_MAX - 1000;
    Plant a, b;
    b.in.presence = Presence::Home;
    for (unsigned frame = 0; frame < 512; ++frame) {
      if (frame % 64 == 0) {
        // Revisit operational ranges after stretches of contradictory or missing data.
        a = Plant{};
        b = Plant{};
        b.in.presence = Presence::Home;
      }
      mutate(a, random);
      mutate(b, random);
      now += pick(random, elapsed);
      first.tick(now, a.config, a.in);
      if (frame % 131 == 0) second.reset();
      second.tick(now, b.config, b.in);
    }
  }
}
} // namespace

int main() {
  roomAssignments();
  sourceAndTankContacts();
  electricClocks();
  interleavedTraces();
  std::printf("PASS: C++ and production Rust matched on 20 outputs at each of %u ticks.\n", samples);
}
