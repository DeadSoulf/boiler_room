#pragma once
#include <stdint.h>
// Persistent definitions only. Hardware adapters are activated separately
// after verified schematics; this configuration never drives GPIO or relays.
namespace panel_config {
enum class Type:uint8_t { None=0,DS18B20=1,ReedNO=2,ReedNC=3,RemoteModbus=4,RemoteMQTT=5 };
enum class Channel:uint8_t { Room=0,Outside=1,Coolant=2,FuelTemp=3,FuelLower=4,FuelUpper=5 };
constexpr unsigned COUNT=6;
struct Assignment {
  Type type=Type::None;
  int16_t gpio=-1;
  uint8_t busAddress=0;
  uint16_t debounceMs=50;
};
struct State {
  uint32_t revision=0;
  Assignment item[COUNT];
  char message[96]={};
};
bool begin();
bool snapshot(State& out);
bool set(Channel channel,Type type,int16_t gpio,uint8_t address,uint16_t debounceMs);
bool reset(Channel channel);
const char* typeName(Type type);
// A pin can be allocated only if board profile explicitly permits its physical
// terminal. No unverified connector GPIOs are enabled by default.
const char* pinRestriction(int gpio);
} // namespace panel_config
