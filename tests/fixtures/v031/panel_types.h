#pragma once
// Application-only data. No sensor model or GPIO is assigned by this project.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

namespace panel {
constexpr const char* VERSION = "v0.3.1";
constexpr unsigned CHANNELS = 4;
constexpr unsigned MAX_AP = 16;
constexpr unsigned MAX_BLE = 16;
constexpr uint32_t STALE_MS = 30000;
struct Measurement {
  bool valid = false;
  float value = 0, minimum = 0, maximum = 0;
  uint32_t updated = 0;
  void put(float v, uint32_t now) {
    if (!isfinite(v)) { valid = false; return; }
    if (!valid) minimum = maximum = v;
    if (v < minimum) minimum = v;
    if (v > maximum) maximum = v;
    value = v; updated = now; valid = true;
  }
  bool fresh(uint32_t now) const { return valid && uint32_t(now - updated) < STALE_MS; }
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
  else snprintf(out, cap, "%.1f", fabsf(m.value) < 0.05f ? 0.0f : m.value);
}
inline bool parseLocalTime(const char* input, time_t& out) {
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
enum class Job : uint8_t { None, WifiScan, Connect, Disconnect, Forget, BleScan, Zone, SetClock };
struct Command {
  Job job = Job::None;
  char ssid[33] = {}, password[65] = {}, input[48] = {};
  bool open = false;
  uint8_t zone = 0;
};
struct AccessPoint { char ssid[33] = {}; int32_t rssi=0; uint8_t auth=0; bool supported=false; };
struct BleDevice { char name[64] = {}, address[20] = {}; int32_t rssi=0; };
struct RadioState {
  uint32_t revision=0;
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
