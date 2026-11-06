#include <cstdint>
#include <cassert>
#include <cstdio>
uint32_t mock_now=0;
#include "../src/panel_sensors.cpp"
#include "panel_types.h"
namespace panel_config {
bool isLocal(Type t){return t==Type::DS18B20||t==Type::ReedNO||t==Type::ReedNC;}
bool isValid(Channel,const Assignment&){return true;}
bool snapshot(State&){return false;}
}
int main(){
  using namespace panel_sensors;
  uint8_t rom[8]={0x28,1,2,3,4,5,6,0};rom[7]=crc8(rom,7);
  for(unsigned i=0;i<8;i++)OneWire32::rom|=uint64_t(rom[i])<<(8*i);
  guard=xSemaphoreCreateMutex();samples=xQueueCreate(24,sizeof(Event));activeRevision=7;
  OneWire32 bus(52);driver[0].bus=&bus;driver[0].setting.type=panel_config::Type::DS18B20;
  panel::Measurement model;
  Event e;
  auto step=[](){mock_now=driver[0].due;thermometer(0,mock_now);};
  auto good=[&](float value){
    OneWire32::scratchpad=value;
    step();assert(driver[0].converting);const auto reads=OneWire32::reads;
    mock_now=driver[0].due-1;thermometer(0,mock_now);
    assert(!poll(e)&&OneWire32::reads==reads);
    step();assert(!driver[0].converting);assert(poll(e));
    assert(e.revision==7&&e.atMs==mock_now&&e.temperature==value);
    model.put(e.temperature,e.atMs);assert(model.fresh(mock_now));
    Snapshot s;assert(snapshot(s));assert(s.channels[0].valid&&s.channels[0].lastGoodMs==mock_now);
  };
  good(25.0f);
  const uint32_t original=model.updated;
  auto rejected=[&](){
    assert(!driver[0].converting&&!poll(e));
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
    step();assert(driver[0].converting);
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
  puts("PASS: real sensor adapter rejects request/poll/scratchpad faults; preserves timestamps; recovers incl. 85C; stale data does not revive");
}
