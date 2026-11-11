#include <Arduino.h>
#include <math.h>
#include <mutex>
#include "panel_heating_runtime.h"
#include "panel_heating_settings.h"
#include "panel_types.h"
#include "panel_sensors.h"

namespace panel_heating_runtime {
namespace {
using namespace panel_heating;
using Channel=panel_config::Channel;

struct Freshness {
  uint32_t sampleAt=0;
  bool expired=false;
};
Freshness ages[panel_config::COUNT];
panel_heating::Controller controller;
std::mutex resultMutex;
Snapshot published;
bool haveResult=false;

float readSample(Channel channel,uint32_t now,bool known,const panel_sensors::Readings& readings,bool contactValue=false){
  const unsigned id=unsigned(channel);
  const auto& sample=readings.channels[id];
  if(!known || !sample.valid)return NAN;
  auto& age=ages[id];
  if(age.sampleAt!=sample.lastGoodMs){age.sampleAt=sample.lastGoodMs;age.expired=false;}
  if(uint32_t(now-sample.lastGoodMs)>=panel::STALE_MS)age.expired=true;
  if(age.expired)return NAN;
  return contactValue?(sample.reached?1.0f:0.0f):sample.temperature;
}
Flag contact(Channel channel,uint32_t now,bool known,const panel_sensors::Readings& readings){
  const float sample=readSample(channel,now,known,readings,true);
  if(!isfinite(sample))return Flag::Unknown;
  return sample!=0?Flag::Yes:Flag::No;
}
Health health(Flag fault){
  if(fault==Flag::Unknown)return Health::Unknown;
  return fault==Flag::Yes?Health::Fault:Health::Ready;
}
}

void resetSensors(){
  for(auto& age:ages)age=Freshness{};
  controller=Controller{};
  std::lock_guard<std::mutex> lock(resultMutex);
  haveResult=false;
}
void tick(bool pinConfigKnown,uint32_t pinRevision){
  Snapshot next{};
  panel_sensors::Readings readings{};
  const bool validDrivers=pinConfigKnown && panel_sensors::readings(readings) &&
    readings.started && readings.configRevision==pinRevision;
  const uint32_t now=millis();
  next.sampledAtMs=now;
  auto& in=next.input;
  in.roomC[0]=readSample(Channel::Room,now,validDrivers,readings);
  for(unsigned n=1;n<14;n++){
    const auto id=static_cast<Channel>(unsigned(Channel::Room2)+n-1);
    in.roomC[n]=readSample(id,now,validDrivers,readings);
  }
  in.supplyC=readSample(Channel::Coolant,now,validDrivers,readings);
  in.returnC=readSample(Channel::CoolantReturn,now,validDrivers,readings);
  in.tankC=readSample(Channel::TankWater,now,validDrivers,readings);
  in.coilSupplyC=readSample(Channel::CoilSupply,now,validDrivers,readings);
  const auto present=contact(Channel::Presence,now,validDrivers,readings);
  if(present!=Flag::Unknown)in.presence=present==Flag::Yes?Presence::Home:Presence::Away;
  in.circulation=contact(Channel::Circulation,now,validDrivers,readings);
  in.plantHeatAvailable=contact(Channel::PlantHeat,now,validDrivers,readings);
  in.diesel=health(contact(Channel::DieselFault,now,validDrivers,readings));
  in.electric=health(contact(Channel::ElectricFault,now,validDrivers,readings));
  panel_heating_settings::State settings{};
  next.settingsAvailable=panel_heating_settings::snapshot(settings);
  if(next.settingsAvailable){
    next.settingsRevision=settings.revision;
    next.configured=settings.revision!=0;
    in.season=settings.season;
  }
  next.output=controller.tick(now,settings.config,in);
  std::lock_guard<std::mutex> lock(resultMutex);
  published=next;
  haveResult=true;
}
bool snapshot(Snapshot& out){
  std::lock_guard<std::mutex> lock(resultMutex);
  if(!haveResult || uint32_t(millis()-published.sampledAtMs)>=panel::STALE_MS){
    haveResult=false;
    out=Snapshot{};
    return false;
  }
  out=published;
  return true;
}
} // namespace panel_heating_runtime
