#pragma once
#include <cassert>
#include "panel_config.cpp"
namespace config_test {
using namespace panel_config;
inline void powerOff(){
  delete sensorIo;sensorIo=nullptr;delete lock;lock=nullptr;ready=false;current=State{};activeSlot=0;sawInvalidStored=false;
}
inline void reboot(){powerOff();assert(begin());}
inline void fresh(){
  config_host::resetFaults();config_host::nvs.clear();config_host::writes=config_host::removes=0;reboot();
}
inline State state(){State result;assert(snapshot(result));return result;}
inline bool sameBindings(const State& a,const State& b){
  for(unsigned i=0;i<COUNT;i++){
    if(a.item[i].type!=b.item[i].type||a.item[i].gpio!=b.item[i].gpio||
       a.item[i].busAddress!=b.item[i].busAddress||a.item[i].debounceMs!=b.item[i].debounceMs)return false;
  }
  return true;
}
inline void unchanged(const State& before){
  const State after=state();assert(after.revision==before.revision&&sameBindings(after,before));
}
inline void twoAccepted(State& x,State& y){
  fresh();assert(set(Channel::Room,Type::DS18B20,52,0,50));x=state();
  assert(set(Channel::Outside,Type::DS18B20,51,0,50));y=state();
  assert(x.revision==1&&y.revision==2);
}
}
