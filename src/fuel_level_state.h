#pragma once
#include <stdint.h>

// Boiler Room panel: two reed-switch inputs, not a continuous fuel sensor.
// 'active' is the already debounced logical input level (polarity is configured
// by the eventual hardware adapter), never a raw GPIO voltage.
namespace fuel_level {
enum class Level : uint8_t { Unknown, Low, Half, Full, Invalid };
struct Sensors {
  bool connected=false;       // Both inputs configured and being sampled
  bool lowerActive=false;
  bool upperActive=false;
  bool stale=false;           // Missing or expired observations
};
inline Level interpret(const Sensors& s) {
  if(!s.connected || s.stale) return Level::Unknown;
  if(s.lowerActive) return Level::Low;  // Safety warning takes priority
  if(s.upperActive) return Level::Half;
  return Level::Full;
}
inline const char* label(Level v) {
  switch(v) {
    case Level::Low: return "Низкий уровень";
    case Level::Half: return "Средний уровень";
    case Level::Full: return "Полный бак";
    case Level::Invalid: return "Ошибка датчиков";
    default: return "Нет данных";
  }
}
// Level::Half is a schematic visualization, NOT an actual 50% measurement.
// Do not drive actuators directly from this display model.
} // namespace fuel_level
