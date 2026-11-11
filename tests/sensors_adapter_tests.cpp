#include <cstdint>
#include <cassert>
#include <cstdio>
uint32_t mock_now=0;
#include "../src/panel_sensors.cpp"
#include "panel_types.h"
namespace panel_config {
bool isLocal(Type t){return t==Type::DS18B20||t==Type::ReedNO||t==Type::ReedNC||t==Type::Relay;}
bool isValid(Channel,const Assignment&){return true;}
void withSensorAssignments(void (*)(const State&)){}
}
int main(){
  using namespace panel_sensors;
  uint8_t rom[8]={0x28,1,2,3,4,5,6,0};rom[7]=crc8(rom,7);
  for(unsigned i=0;i<8;i++)OneWire32::rom|=uint64_t(rom[i])<<(8*i);
  guard=xSemaphoreCreateMutex();samples=xQueueCreate(24,sizeof(Event));activeRevision=7;
  driver[0].setting.gpio=52;driver[0].setting.type=panel_config::Type::DS18B20;
  panel::Measurement model;
  Event e;
  auto step=[](){mock_now=driver[0].due;pollThermometers(mock_now);};
  auto good=[&](float value){
    OneWire32::scratchpad=value;
    step();assert(wire);const auto reads=OneWire32::reads;
    mock_now=driver[0].due-1;pollThermometers(mock_now);
    assert(!poll(e)&&OneWire32::reads==reads);
    step();assert(!wire);assert(poll(e));
    assert(e.revision==7&&e.atMs==mock_now&&e.temperature==value);
    model.put(e.temperature,e.atMs);assert(model.fresh(mock_now));
    Snapshot s;assert(snapshot(s));assert(s.channels[0].valid&&s.channels[0].lastGoodMs==mock_now);
  };
  good(25.0f);
  const uint32_t original=model.updated;
  auto rejected=[&](){
    assert(!wire&&!poll(e));
    Snapshot s;assert(snapshot(s));assert(!s.channels[0].valid);
    assert(s.channels[0].lastGoodMs==original&&model.updated==original);
  };
  OneWire32::scratchpad=85.0f;
  // Failed reset, Skip ROM, and Convert T must never enter conversion state.
  for(unsigned fault=0;fault<3;fault++){
    OneWire32::resetOk=fault!=0;OneWire32::failWrite=fault;
    const auto temperatures=OneWire32::temperatures;
    step();rejected();assert(OneWire32::temperatures==temperatures);
  }
  OneWire32::resetOk=true;OneWire32::failWrite=0;
  // Timeout / incomplete conversion must not read the retained scratchpad.
  for(unsigned fault=0;fault<2;fault++){
    OneWire32::readOk=fault!=0;OneWire32::completion=fault==0?1:0;
    step();assert(wire);
    const auto temperatures=OneWire32::temperatures;
    step();rejected();assert(OneWire32::temperatures==temperatures);
  }
  assert(mock_now-original>=panel::STALE_MS&&!model.fresh(mock_now));
  OneWire32::readOk=true;OneWire32::completion=1;
  // Scratchpad CRC/no-response/driver errors and out-of-range/NaN are rejected.
  for(uint8_t err=1;err<=4;err++){
    OneWire32::tempError=err;step();step();rejected();
  }
  OneWire32::tempError=0;
  for(float value:{NAN,-55.1f,125.1f}){
    OneWire32::scratchpad=value;step();step();rejected();
  }
  // Valid 85°C is not mistaken for the power-on sentinel; recovery refreshes data.
  good(85.0f);good(-55.0f);good(125.0f);
  assert(OneWire32::requests==0);
  const uint32_t last=model.updated;
  mock_now=last+panel::STALE_MS;
  Snapshot s;assert(snapshot(s)&&!s.channels[0].valid);
  assert(!model.fresh(mock_now));
  mock_now=uint32_t(uint64_t(last)+(1ULL<<32)+1);
  assert(snapshot(s)&&!s.channels[0].valid&&!model.fresh(mock_now));
  // Six separately wired sensors must work with only a single RMT pair.
  // A missing supply sensor must not prevent return or fuel readings.
  panel_config::State config;
  const unsigned ids[]={0,1,2,3,6,7};
  const uint8_t pins[]={52,33,51,31,50,49};
  config.revision=8;
  for(unsigned n=0;n<panel::CHANNELS;n++){
    config.item[ids[n]].type=panel_config::Type::DS18B20;
    config.item[ids[n]].gpio=pins[n];
  }
  mock_now=1000;reconfigure(config);
  unsigned seen[panel_config::COUNT]={};
  uint32_t first[panel_config::COUNT]={};
  auto runUntil=[&](uint32_t end){
    for(;mock_now<end;mock_now+=45){
      if(wire)OneWire32::scratchpad=float(wire->pin);
      pollThermometers(mock_now);
      while(poll(e)){
        assert(e.channel<panel_config::COUNT && e.revision==config.revision);
        assert(e.temperature==config.item[e.channel].gpio);
        if(!seen[e.channel])first[e.channel]=e.atMs;
        ++seen[e.channel];
      }
    }
  };
  OneWire32::missingPin=51;
  runUntil(13000);
  assert(seen[2]==0 && seen[6]>=2 && seen[3]>=2 && seen[7]>=2);
  OneWire32::missingPin=-1;
  runUntil(28000);
  for(unsigned id:ids)assert(seen[id]>=2 && first[id]<panel::STALE_MS);
  assert(!seen[4]&&!seen[5]);
  const unsigned tankSamples=seen[3],outletSamples=seen[7];
  OneWire32::missingPin=pins[3];
  runUntil(44000);
  // A conversion already running when the pin disappears can finish in this fixture.
  assert(seen[3]<=tankSamples+1 && seen[7]>=outletSamples+2);
  OneWire32::missingPin=-1;
  while(!wire){mock_now+=45;pollThermometers(mock_now);}
  // The previous conversion is still pending when that GPIO is reassigned to a
  // relay. The new revision drops the old conversion and queued events. Neither
  // relay may be polled as a thermometer or configured as a pulled-up reed.
  const unsigned old=probe;
  const int reserved=driver[old].setting.gpio;
  const auto opens=OneWire32::opens[reserved],otherOpens=OneWire32::opens[34];
  config.item[old]=panel_config::Assignment{};
  config.item[8].gpio=reserved;config.item[8].type=panel_config::Type::Relay;
  config.item[9].gpio=34;config.item[9].type=panel_config::Type::Relay;
  ++config.revision;
  pollInputs(config); // remaining thermometers start after the new assignment settles
  assert(!wire && !OneWire32::live && !poll(e));
  for(unsigned tick=0;tick<300;tick++){
    mock_now+=45;pollInputs(config);
    while(poll(e))assert(e.revision==config.revision&&e.channel<panel_config::INPUT_COUNT&&e.channel!=old);
  }
  assert(OneWire32::opens[reserved]==opens && OneWire32::opens[34]==otherOpens);
  assert(!pulls[reserved]&&!pulls[34]&&!reads[reserved]&&!reads[34]);
  puts("PASS: real sensor adapter rejects request/poll/scratchpad faults; preserves timestamps; recovers incl. 85C; stale data does not revive");
}
