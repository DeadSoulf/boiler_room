#include "panel_heating_settings.h"
#include "heating_settings_stubs.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

namespace ps = panel_heating_settings;
namespace ph = panel_heating;
namespace f = heating_fixture;

static ps::State example() {
  ps::State s;
  s.season = ph::Season::Winter;
  auto& c = s.config;
  c.roomOnC = 18.5f; c.roomRadiatorOffC = 21.5f; c.roomOffC = 22.5f;
  c.returnTargetC = 43.5f; c.supplyTargetC = 61; c.returnRestartC = 35;
  c.tankOnC = 40.5f; c.tankOffC = 54;
  c.coilStartDeltaC = 6; c.coilStopDeltaC = 2;
  c.reheat = ph::Reheat::OneByOne; c.reheatStepMs = 78000; c.allowAwayDiesel = true;
  for (unsigned i = 0; i < ph::LOOP_COUNT; ++i) c.roomForLoop[i] = uint8_t(ph::ROOM_COUNT - i);
  return s;
}

static void verifyRecord(const ps::State& s) {
  assert(s.revision == 1 && s.season == ph::Season::Winter);
  const auto& c = s.config;
  assert(c.roomOnC == 18.5f && c.roomRadiatorOffC == 21.5f && c.roomOffC == 22.5f);
  assert(c.returnTargetC == 43.5f && c.supplyTargetC == 61 && c.returnRestartC == 35);
  assert(c.tankOnC == 40.5f && c.tankOffC == 54 && c.coilStartDeltaC == 6 && c.coilStopDeltaC == 2);
  assert(c.reheat == ph::Reheat::OneByOne && c.reheatStepMs == 78000 && c.allowAwayDiesel);
  for (unsigned i = 0; i < ph::LOOP_COUNT; ++i) assert(c.roomForLoop[i] == ph::ROOM_COUNT - i);
}

static void digest(std::vector<uint8_t>& blob) {
  assert(blob.size() == 56);
  uint32_t h = 2166136261U;
  for (size_t i = 0; i < 52; ++i) h = (h ^ blob[i]) * 16777619U;
  std::memcpy(blob.data()+52, &h, sizeof(h));
}

static void writePhase(const char* path) {
  ps::State s;
  assert(ps::begin() && ps::snapshot(s) && !s.revision && !f::writes);
  assert(s.season == ph::Season::Unknown && isnan(s.config.supplyTargetC));
  assert(s.config.reheat == ph::Reheat::Unset && !s.config.allowAwayDiesel);
  auto request = example();
  f::damageNextVerification = true;
  assert(ps::save(0, request) == ps::Result::Unavailable && f::blob.empty());
  assert(ps::snapshot(s) && !s.revision && isnan(s.config.tankOffC));
  assert(ps::save(0, request) == ps::Result::Saved && ps::snapshot(s));
  verifyRecord(s);
  const auto saved = f::blob;
  const auto writes = f::writes;
  assert(ps::save(0, request) == ps::Result::Conflict && f::writes == writes);
  assert(ps::save(1, request) == ps::Result::Saved && f::writes == writes);
  auto invalid = request; invalid.config.roomForLoop[13] = 15;
  assert(ps::save(1, invalid) == ps::Result::Invalid);
  invalid = request; invalid.config.roomOnC = NAN;
  assert(ps::save(1, invalid) == ps::Result::Invalid);
  invalid = request; invalid.config.returnTargetC = INFINITY;
  assert(ps::save(1, invalid) == ps::Result::Invalid);
  invalid = request; invalid.config.returnRestartC = invalid.config.returnTargetC;
  assert(ps::save(1, invalid) == ps::Result::Invalid);
  invalid = request; invalid.config.reheatStepMs = 0;
  assert(ps::save(1, invalid) == ps::Result::Invalid);
  invalid = request; invalid.config.coilStartDeltaC = 1;
  assert(ps::save(1, invalid) == ps::Result::Invalid);
  assert(f::blob == saved && f::writes == writes);
  auto changed = request; changed.season = ph::Season::Summer;
  f::damageNextVerification = true;
  assert(ps::save(1, changed) == ps::Result::Unavailable && f::blob == saved);
  f::failCommit = true;
  assert(ps::save(1, changed) == ps::Result::Unavailable && f::blob == saved);
  f::blockLock = true;
  assert(ps::save(1, changed) == ps::Result::Unavailable && !ps::snapshot(s));
  f::blockLock = false;
  assert(ps::snapshot(s)); verifyRecord(s);
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char*>(saved.data()), saved.size());
  assert(file.good());
}

static void rebootPhase(const char* path) {
  ps::State s;
  std::ifstream file(path, std::ios::binary);
  f::blob.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  assert(f::blob.size() == 56); f::space = true;
  assert(ps::begin() && ps::snapshot(s) && !f::writes); verifyRecord(s);
  const auto good = f::blob;
  // An unsupported format or a checksummed but invalid channel must not be cleared.
  f::blob[0] ^= 1; digest(f::blob);
  assert(!ps::begin() && !ps::snapshot(s) && ps::save(0, example()) == ps::Result::Unavailable);
  f::blob = good; f::blob[32] = 15; digest(f::blob);
  assert(!ps::begin() && !ps::snapshot(s));
  // Checksummed records with noncanonical flag or reserved bytes were
  // rejected by the v0.11.0 reader. Preserve them for diagnosis.
  for (unsigned offset : {48u, 49u}) {
    f::blob = good; f::blob[offset] = 2; digest(f::blob);
    const auto damaged = f::blob;
    assert(!ps::begin() && !ps::snapshot(s));
    assert(ps::save(1, example()) == ps::Result::Unavailable && f::blob == damaged);
  }
  // Revision zero represents an empty namespace, never a persisted record.
  f::blob = good; std::fill_n(f::blob.begin() + 4, 4, 0); digest(f::blob);
  assert(!ps::begin() && !ps::snapshot(s));
  f::blob = good; f::blob.back() ^= 1;
  assert(!ps::begin() && !ps::snapshot(s));
  f::blob.resize(30);
  assert(!ps::begin() && !ps::snapshot(s) && !f::writes);
  f::blob = good; f::unreadable = true;
  assert(!ps::begin() && ps::save(1, example()) == ps::Result::Unavailable && !f::writes);
  f::unreadable = false;
  assert(ps::begin() && ps::snapshot(s)); verifyRecord(s);
  // A saturated revision accepts the same settings but cannot wrap on a change.
  std::fill_n(f::blob.begin() + 4, 4, 0xff); digest(f::blob);
  assert(ps::begin() && ps::snapshot(s) && s.revision == UINT32_MAX);
  assert(ps::save(UINT32_MAX, example()) == ps::Result::Saved);
  auto altered = example(); altered.season = ph::Season::Summer;
  assert(ps::save(UINT32_MAX, altered) == ps::Result::Unavailable && !f::writes);
  f::blob = good;
  auto a = example(), b = a;
  a.config.tankOnC = NAN;
  b.config.returnTargetC = NAN;
  ps::Result one{}, two{};
  std::thread left([&] { one = ps::save(1, a); });
  std::thread right([&] { two = ps::save(1, b); });
  left.join(); right.join();
  assert((one == ps::Result::Saved && two == ps::Result::Conflict) ||
         (two == ps::Result::Saved && one == ps::Result::Conflict));
  assert(ps::begin() && ps::snapshot(s) && s.revision == 2 && f::writes == 1);
  assert(isnan(s.config.tankOnC) != isnan(s.config.returnTargetC));

  // Signed tenths and the absent marker must survive a read through the
  // production adapter. Include both limits and a negative fractional value.
  auto limits = ps::State{};
  limits.config.roomOnC = -55;
  limits.config.roomRadiatorOffC = -0.1f;
  limits.config.roomOffC = 125;
  limits.config.coilStartDeltaC = 180;
  limits.config.coilStopDeltaC = 0;
  assert(ps::save(2, limits) == ps::Result::Saved);
  assert(ps::begin() && ps::snapshot(s) && s.revision == 3);
  assert(s.config.roomOnC == -55 && s.config.roomRadiatorOffC == -0.1f && s.config.roomOffC == 125);
  assert(isnan(s.config.supplyTargetC) && isnan(s.config.tankOnC));
  assert(s.config.coilStartDeltaC == 180 && s.config.coilStopDeltaC == 0);
  limits.config.roomRadiatorOffC = -0.16f;
  assert(ps::save(3, limits) == ps::Result::Invalid);
}

int main(int argc, char** argv) {
  assert(argc == 3);
  if (std::string(argv[1]) == "write") writePhase(argv[2]);
  else { assert(std::string(argv[1]) == "restart"); rebootPhase(argv[2]); }
}
