#pragma once
// JC4880P443C-I-W, JP1 external 2x13 header, manufacturer pin definitions.
// Source: b5463/kino-d4 docs/HARDWARE.md (measured pin 7 and pin 13),
// cross-checked with ultramcu/guition-jc4880p443c-i-w schematic sheet 03.
// No GPIO outside this list may be assigned to local sensors.
#include <stdint.h>

namespace panel_pins {
struct Pin { int gpio; unsigned jp1; };
constexpr Pin HEADER[]={
  {52,7},{33,8},{51,9},{31,10},{50,11},{30,12},
  {49,13},{29,14},{34,17},{32,19},{28,21}
};
constexpr unsigned HEADER_COUNT=sizeof(HEADER)/sizeof(HEADER[0]);
inline bool sensorPin(int gpio) {
  for(unsigned i=0;i<HEADER_COUNT;i++)if(HEADER[i].gpio==gpio)return true;
  return false;
}
inline unsigned connectorPin(int gpio) {
  for(unsigned i=0;i<HEADER_COUNT;i++)if(HEADER[i].gpio==gpio)return HEADER[i].jp1;
  return 0;
}
inline const char* blockReason(int gpio) {
  if(gpio<0||gpio>54)return "GPIO outside P4 range";
  if(sensorPin(gpio))return nullptr;
  switch(gpio){
    case 35:return "JP1 BOOT strapping input";
    case 3:case 5:case 7:case 8:case 23:return "Display or GT911 touch reserved";
    case 14:case 15:case 16:case 17:case 18:case 19:case 54:
      return "ESP32-C6 SDIO/reset reserved";
    case 39:case 40:case 41:case 42:case 43:case 44:
      return "microSD SDMMC reserved";
    case 9:case 10:case 11:case 12:case 13:case 48:return "Audio reserved";
    case 26:case 27:return "RS-485 reserved";
    case 37:case 38:return "UART console reserved";
    default:return "Not routed to validated JP1 sensor pin";
  }
}
} // namespace panel_pins
