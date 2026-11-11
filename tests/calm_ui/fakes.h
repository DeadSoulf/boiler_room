#pragma once
// Test-only fixtures. No GPIO, networking, serial ports, NVS or real SD.
#include <cstring>
#include <deque>
#include "Arduino.h"
#include "panel_radio.h"
#include "panel_web.h"
#include "panel_pairing.h"
#include "panel_storage.h"
#include "panel_update.h"
#include "panel_config.h"
#include "panel_boiler.h"
#include "panel_targets.h"
#include "panel_sensors.h"
#include "panel_ha.h"
uint32_t fakeMillis=1000;SerialStub Serial;ESPStub ESP;
namespace panel_radio {
 panel::RadioState fixture;panel::Command last;unsigned submitted=0;bool accept=true;
 bool begin(){return true;}bool cancel(){return true;}
 bool snapshot(panel::RadioState& s){s=fixture;return true;}
 bool submit(const panel::Command& c){if(!accept)return false;last=c;++submitted;return true;}
 const char* zoneName(uint8_t){return "UTC";}
}
namespace panel_config {State fixture;bool begin(){return true;}bool snapshot(State& s){s=fixture;return true;}}
namespace panel_boiler {
 Settings fixture,electric;bool available=true;unsigned attempts=0;
 Settings& settings(Target target){return target==Target::Electric?electric:fixture;}
 bool begin(){return true;}
 bool snapshot(Settings& out,Target target){if(!available)return false;out=settings(target);return true;}
 Result save(uint32_t revision,int on,int off,Target target){
   ++attempts;if(!available)return Result::Unavailable;
   Settings& selected=settings(target);
   if(revision!=selected.revision)return Result::Conflict;
   if(on<0||off>99||on>=off)return Result::Invalid;
   selected.configured=true;selected.onC=uint8_t(on);selected.offC=uint8_t(off);++selected.revision;
   return Result::Saved;
 }
}
namespace panel_targets {
 Setting fixture[panel::CHANNELS]{};
 unsigned writes=0;
 bool available=true;
 bool begin(){return true;}
 bool snapshot(unsigned channel,Setting& out){
   if(!available||channel>=panel::CHANNELS)return false;
   out=fixture[channel];return true;
 }
 Result save(unsigned channel,uint32_t revision,int value){
   if(!available)return Result::Unavailable;
   if(channel>=panel::CHANNELS||value<MIN_C||value>MAX_C)return Result::Invalid;
   auto& item=fixture[channel];
   if(revision!=item.revision)return Result::Conflict;
   item.configured=true;item.valueC=value;++item.revision;++writes;
   return Result::Saved;
 }
}
namespace panel_sensors {std::deque<Event> queue;bool begin(){return true;}bool poll(Event& e){if(queue.empty())return false;e=queue.front();queue.pop_front();return true;}}
namespace panel_storage {
 Snapshot fixture;unsigned accepted=0,cancelled=0;
 bool begin(){return true;}bool snapshot(Snapshot& s){s=fixture;return true;}
 bool event(const char*,const char*){return true;}
 bool measurements(const panel::Measurement (&)[panel::CHANNELS],uint32_t){return true;}
 bool confirmFormatPhysically(){if(fixture.formatPhase!=FormatPhase::AwaitingPhysical)return false;++accepted;fixture.formatPhase=FormatPhase::Queued;return true;}
 bool cancelFormatPhysically(){if(fixture.formatPhase!=FormatPhase::AwaitingPhysical)return false;++cancelled;fixture.formatPhase=FormatPhase::Cancelled;return true;}
}
namespace panel_web {
 panel_pairing::State challenge;
 bool begin(){return true;}void publish(const panel::Measurement (&)[panel::CHANNELS],const panel::FuelLevel&,uint32_t){}
 bool pairingCode(char out[7],uint32_t& seconds,uint32_t* generation){uint32_t g=0;bool active=challenge.display(out,seconds,g,millis());if(generation)*generation=g;return active;}
 void revokePairing(){challenge.revoke();}
}

namespace panel_update {
 Snapshot fixture;unsigned checked=0,installed=0,cancelled=0,rolledBack=0,heartbeats=0;bool healthy=false,accept=true;
 char requestedVersion[24]={};
 bool begin(){fixture.started=fixture.supported=true;return true;}
 bool snapshotAvailable=true;
 bool snapshot(Snapshot& s){if(!snapshotAvailable){s={};return false;}s=fixture;return true;}
 bool requestCheck(){if(!accept||fixture.busy||fixture.bootPending)return false;++checked;fixture.busy=true;fixture.phase=Phase::Checking;return true;}
 bool requestInstall(const char* expected){
   if(!accept||fixture.busy||fixture.bootPending||!fixture.available||strcmp(expected,fixture.latestVersion))return false;
   ++installed;strcpy(requestedVersion,expected);fixture.busy=fixture.cancelAllowed=true;fixture.phase=Phase::Downloading;return true;
 }
 bool requestCancel(){if(!accept||!fixture.busy||!fixture.cancelAllowed)return false;++cancelled;fixture.busy=fixture.cancelAllowed=false;fixture.phase=Phase::Cancelled;return true;}
 bool requestRollback(const char* expected){
   if(!accept||fixture.busy||fixture.bootPending||!fixture.rollbackAvailable||strcmp(expected,fixture.previousVersion))return false;
   ++rolledBack;strcpy(requestedVersion,expected);fixture.busy=true;fixture.cancelAllowed=false;fixture.phase=Phase::Rebooting;return true;
 }
 void uiHeartbeat(bool ready){++heartbeats;healthy=ready;}
 const char* phaseName(Phase phase){const char* names[]={"idle","checking","current","available","downloading","verifying","installing","rebooting","success","error","cancelled","rolled_back"};return names[unsigned(phase)];}
}

namespace panel_ha {Snapshot fixture;bool begin(){return true;}bool snapshot(Snapshot& s){s=fixture;return true;}void publish(const panel::Measurement (&)[panel::CHANNELS],const panel::FuelLevel&,uint32_t){}}
