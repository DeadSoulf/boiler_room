#pragma once
// Application-only state. No GPIO or sensor electrical polarity is assigned.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

namespace panel {
constexpr const char* VERSION = "v0.5.0";
constexpr unsigned CHANNELS = 4;
constexpr unsigned MAX_AP = 16;
constexpr unsigned MAX_BLE = 16;
constexpr uint32_t STALE_MS = 30000;
constexpr unsigned HISTORY_POINTS=24;
struct Measurement {
  bool valid = false;
  float value = 0, minimum = 0, maximum = 0;
  uint32_t updated=0, revision=0;
  float history[HISTORY_POINTS]={};uint32_t sampled[HISTORY_POINTS]={};
  unsigned count=0,next=0;
  float at(unsigned index) const {return history[(next+HISTORY_POINTS-count+index)%HISTORY_POINTS];}
  uint32_t timeAt(unsigned index) const {return sampled[(next+HISTORY_POINTS-count+index)%HISTORY_POINTS];}
  double delta() const {return count>=2?double(at(count-1))-double(at(count-2)):0;}
  unsigned freshness(uint32_t now) const {return fresh(now)?unsigned((STALE_MS-uint32_t(now-updated))*100U/STALE_MS):0;}
  void put(float v, uint32_t now) {
    ++revision;
    if (!isfinite(v)) { valid = false; return; }
    if (!count) minimum = maximum = v;
    if (v < minimum) minimum = v;
    if (v > maximum) maximum = v;
    value = v; updated = now; valid = true;
    history[next]=v;sampled[next]=now;next=(next+1)%HISTORY_POINTS;
    if(count<HISTORY_POINTS)++count;
  }
  bool fresh(uint32_t now) const { return valid && uint32_t(now - updated) < STALE_MS; }
};
enum class FuelLevelStatus : uint8_t {
  NoData, Stale, BelowLower, Between, UpperReached, Inconsistent
};
// Accept stable, normalized threshold readings only: true means the fuel has
// reached this sensor's height. A future hardware adapter owns GPIO, NO/NC
// polarity and debounce; a raw reed-contact state is not this logical value.
struct ReedLevelReading {
  bool valid=false, reached=false;
  uint32_t updated=0;
  void put(bool levelReached, uint32_t now) {
    reached=levelReached;updated=now;valid=true;
  }
  void clear() { *this=ReedLevelReading{}; }
  bool fresh(uint32_t now) const { return valid && uint32_t(now-updated)<STALE_MS; }
};
struct FuelLevel {
  ReedLevelReading upper,lower;
  // Publish both normalized readings from one observation; lower is first.
  void put(bool lowerReached, bool upperReached, uint32_t now) {
    lower.put(lowerReached,now);upper.put(upperReached,now);
  }
  void clear() { lower.clear();upper.clear(); }
  FuelLevelStatus status(uint32_t now) const {
    if(!lower.valid || !upper.valid)return FuelLevelStatus::NoData;
    if(!lower.fresh(now) || !upper.fresh(now))return FuelLevelStatus::Stale;
    if(upper.reached)return lower.reached?FuelLevelStatus::UpperReached:FuelLevelStatus::Inconsistent;
    return lower.reached?FuelLevelStatus::Between:FuelLevelStatus::BelowLower;
  }
};
inline int adjacent(int page, int direction) { return (page + direction + 4) % 4; }
inline bool passwordValid(const char* password, bool open) {
  const size_t n = strlen(password);
  if (open) return n == 0;
  if (n >= 8 && n <= 63) return true;
  if (n != 64) return false;
  for (size_t i=0;i<n;i++) {
    const char c = password[i];
    if (!((c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F'))) return false;
  }
  return true;
}
inline void temperature(char* out, size_t cap, const Measurement& m, uint32_t now) {
  if (!m.fresh(now)) snprintf(out, cap, "--.-");
  else if(fabsf(m.value)>=10000)snprintf(out,cap,"%.3g",double(m.value));
  else snprintf(out, cap, "%.1f", fabsf(m.value) < 0.05f ? 0.0f : m.value);
}
inline bool parseLocalTime(const char* input, time_t& out) {
  if(!input || strlen(input)!=16 || input[4]!='-' || input[7]!='-' || input[10]!=' ' || input[13]!=':')return false;
  for(unsigned i=0;i<16;i++)if(i!=4 && i!=7 && i!=10 && i!=13 && (input[i]<'0'||input[i]>'9'))return false;
  int year, mon, day, hour, minute; char extra;
  if (sscanf(input, "%d-%d-%d %d:%d%c", &year,&mon,&day,&hour,&minute,&extra) != 5) return false;
  if (year<2024 || year>2099 || mon<1 || mon>12 || day<1 || day>31 || hour<0 || hour>23 || minute<0 || minute>59) return false;
  tm t = {}; t.tm_year=year-1900; t.tm_mon=mon-1; t.tm_mday=day;
  t.tm_hour=hour; t.tm_min=minute; t.tm_isdst=-1;
  const time_t candidate = mktime(&t);
  // mktime normalises invalid dates and nonexistent DST times; reject them.
  if (candidate == time_t(-1) || t.tm_year != year-1900 || t.tm_mon != mon-1 || t.tm_mday != day || t.tm_hour != hour || t.tm_min != minute) return false;
  out=candidate; return true;
}
enum class Job : uint8_t { None, WifiScan, Connect, Disconnect, Forget, BleScan, Zone, SetClock, SetDisplayTimeout };
struct Command {
  Job job = Job::None;
  char ssid[33] = {}, password[65] = {}, input[48] = {};
  uint16_t displayTimeoutSec=0;
  bool open = false;
  uint8_t zone = 0;
};
struct AccessPoint { char ssid[33] = {}; int32_t rssi=0; uint8_t auth=0; bool supported=false; };
struct BleDevice { char name[64] = {}, address[20] = {}; int32_t rssi=0; };
struct RadioState {
  uint32_t revision=0;
  uint16_t displayTimeoutSec=0;
  char displayMessage[120]={};
  bool ready=false, busy=false, wifiSupported=false, bleSupported=false;
  bool connected=false, hasSaved=false, autoConnect=false;
  Job job=Job::None;
  uint8_t zone=0, apCount=0, bleCount=0;
  int32_t rssi=0;
  char ssid[33] = {}, ip[24] = {}, wifiMessage[150] = {}, bleMessage[150] = {}, timeMessage[100] = {};
  char hostVersion[24] = {}, c6Version[24] = {};
  AccessPoint aps[MAX_AP]; BleDevice ble[MAX_BLE];
};
} // namespace panel
