#pragma once
#include "panel_ha_protocol.h"
#include "panel_types.h"
namespace panel_ha {
enum class PasswordMode:uint8_t { Keep,Set,Clear };
enum class SaveResult:uint8_t { Saved,Invalid,Conflict,StorageError,Unavailable };
struct Snapshot {
  bool ready=false,supported=false,enabled=false,connected=false,passwordSet=false;
  uint16_t port=1883;
  uint32_t revision=0,connections=0,discoveryMessages=0,lastPublishMs=0;
  int errorCode=0;
  char host[129]={},username[65]={},prefix[65]="homeassistant",id[32]={};
  char message[120]={};
};
bool begin();
bool snapshot(Snapshot& out); // NEVER includes the password.
SaveResult configure(const Config& request,PasswordMode mode,uint32_t expectedRevision);
bool requestDiscovery();
// One-slot overwrite queue, called by LVGL after receiving real sensor events.
void publish(const panel::Measurement (&temps)[panel::CHANNELS],const panel::FuelLevel& fuel,uint32_t now);
}
