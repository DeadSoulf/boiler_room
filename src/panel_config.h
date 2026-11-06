#pragma once
#include <stdint.h>
// Persistent channel bindings. Real sensor inputs are restricted to
// externally routed, validated JP1 GPIOs. Never controls actuator outputs.
namespace panel_config {
enum class Type:uint8_t { None=0,DS18B20=1,ReedNO=2,ReedNC=3,RemoteModbus=4,RemoteMQTT=5 };
enum class Channel:uint8_t { Room=0,Outside=1,Coolant=2,FuelTemp=3,FuelLower=4,FuelUpper=5 };
constexpr unsigned COUNT=6;
// INPUT_PULLUP: open contact = HIGH. Reached is a normalized fuel-height state,
// NOT physical contact closure. User's open=full float arrangement uses ReedNC.
inline bool reedLevelReached(Type type,bool inputHigh) {
  return type==Type::ReedNO ? !inputHigh : inputHigh;
}
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
bool rollback();
enum class PinResult:uint8_t { Saved,Invalid,Conflict,StorageError,Unavailable };
// Compare the revision and change both owners in one verified configuration commit.
// Saved also covers an unchanged assignment; savedRevision then stays the same.
PinResult setPin(int16_t gpio,Channel channel,Type type,uint16_t debounceMs,
                 uint32_t expectedRevision,uint32_t& savedRevision);
PinResult clearPin(int16_t gpio,uint32_t expectedRevision,uint32_t& savedRevision);
const char* typeName(Type type);
const char* pinRestriction(int gpio);
bool isLocal(Type type);
bool isValid(Channel ch,const Assignment& a);
} // namespace panel_config
