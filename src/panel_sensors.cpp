#include <Arduino.h>
#include <OneWireESP32.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <new>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "panel_config.h"
#include "panel_pin_profile.h"
#include "panel_sensors.h"

namespace panel_sensors {
namespace {
constexpr uint32_t PERIOD_MS=5000,CONVERSION_MS=850,RETRY_MS=6000;
Snapshot state;
SemaphoreHandle_t guard=nullptr;
QueueHandle_t samples=nullptr;
TaskHandle_t task=nullptr;
struct Driver {
  panel_config::Assignment setting;
  OneWire32* bus=nullptr;
  uint64_t rom=0;
  uint32_t due=0;
  uint32_t rawChangedAt=0;
  bool converting=false;
  bool raw=false,debounced=false,haveRaw=false,haveStable=false;
};
Driver driver[panel_config::COUNT];
uint32_t activeRevision=0xffffffffU;
uint8_t crc8(const uint8_t* data,unsigned len){
  uint8_t crc=0;
  for(unsigned i=0;i<len;i++){
    uint8_t byte=data[i];
    for(unsigned bit=0;bit<8;bit++){
      const uint8_t mix=(crc^byte)&1U;
      crc>>=1;
      if(mix)crc^=0x8c;
      byte>>=1;
    }
  }
  return crc;
}
void status(unsigned i,const char* label,bool valid,uint8_t found=0){
  if(!guard||xSemaphoreTake(guard,pdMS_TO_TICKS(8))!=pdTRUE)return;
  auto& c=state.channels[i];
  c.active=panel_config::isLocal(driver[i].setting.type);
  c.valid=valid;
  c.found=found;
  snprintf(c.status,sizeof(c.status),"%s",label);
  xSemaphoreGive(guard);
}
void fail(unsigned i,const char* label,uint8_t found=0){
  if(!guard||xSemaphoreTake(guard,pdMS_TO_TICKS(8))!=pdTRUE)return;
  auto& c=state.channels[i];
  c.valid=false;
  c.found=found;
  ++c.errors;
  snprintf(c.status,sizeof(c.status),"%s",label);
  xSemaphoreGive(guard);
}
void success(unsigned i,const char* label,uint32_t now,uint8_t found=1){
  if(!guard||xSemaphoreTake(guard,pdMS_TO_TICKS(8))!=pdTRUE)return;
  auto& c=state.channels[i];
  c.active=true;c.valid=true;c.found=found;c.lastGoodMs=now;
  snprintf(c.status,sizeof(c.status),"%s",label);
  xSemaphoreGive(guard);
}
void publish(unsigned i,uint32_t when,float temperature,bool reached){
  if(!samples)return;
  Event e;
  e.revision=activeRevision;e.atMs=when;
  e.channel=uint8_t(i);
  e.temperature=temperature;e.reedReached=reached;
  xQueueSend(samples,&e,0);
}
bool due(uint32_t now,uint32_t target){
  return int32_t(now-target)>=0;
}
void destroy(unsigned i){
  Driver& d=driver[i];
  if(d.bus){d.bus->~OneWire32();free(d.bus);d.bus=nullptr;}
  if(panel_config::isLocal(d.setting.type) &&
     panel_pins::sensorPin(d.setting.gpio))pinMode(d.setting.gpio,INPUT);
  d=Driver{};
}
void reconfigure(const panel_config::State& config){
  for(unsigned i=0;i<panel_config::COUNT;i++)destroy(i);
  if(samples)xQueueReset(samples);
  if(guard&&xSemaphoreTake(guard,pdMS_TO_TICKS(30))==pdTRUE){
    state=Snapshot{};state.started=true;state.configRevision=config.revision;
    xSemaphoreGive(guard);
  }
  activeRevision=config.revision;
  for(unsigned i=0;i<panel_config::COUNT;i++){
    const auto a=config.item[i];
    if(!panel_config::isValid(static_cast<panel_config::Channel>(i),a)){
      status(i,"Invalid GPIO assignment ignored",false);continue;
    }
    driver[i].setting=a;
    if(guard&&xSemaphoreTake(guard,pdMS_TO_TICKS(10))==pdTRUE){
      state.channels[i].gpio=a.gpio;
      state.channels[i].type=uint8_t(a.type);
      state.channels[i].configured=a.type!=panel_config::Type::None;
      xSemaphoreGive(guard);
    }
    if(a.type==panel_config::Type::None){
      status(i,"Not assigned",false);continue;
    }
    if(!panel_config::isLocal(a.type)){
      status(i,"Remote adapter not configured",false);continue;
    }
    if(!panel_pins::sensorPin(a.gpio)){
      fail(i,"GPIO is not routed to verified JP1");continue;
    }
    if(a.type==panel_config::Type::DS18B20){
      void* memory=calloc(1,sizeof(OneWire32));
      if(!memory){fail(i,"OneWire memory allocation failed");continue;}
      driver[i].bus=new(memory) OneWire32(uint8_t(a.gpio));
      driver[i].due=millis()+1000U;
      status(i,"OneWire RMT started; awaiting sensor",false);
    }else{
      pinMode(a.gpio,INPUT_PULLUP);
      driver[i].haveRaw=false;
      driver[i].haveStable=false;
      status(i,"Reed input enabled (unsupervised contact)",false);
    }
  }
  Serial.printf("[BP-SENSOR] config revision %lu applied; JP1 only; no outputs\n",
                (unsigned long)config.revision);
}
void reed(unsigned i,uint32_t now){
  Driver& d=driver[i];
  const bool inputHigh=digitalRead(d.setting.gpio)!=LOW;
  if(!d.haveRaw || inputHigh!=d.raw){
    d.raw=inputHigh;d.rawChangedAt=now;d.haveRaw=true;
  }
  if(uint32_t(now-d.rawChangedAt)<d.setting.debounceMs)return;
  if(d.haveStable&&d.debounced==d.raw&&!due(now,d.due))return;
  d.debounced=d.raw;d.haveStable=true;
  d.due=now+PERIOD_MS; // Refresh stable level within the model's 30s stale window.
  const bool reached=panel_config::reedLevelReached(d.setting.type,inputHigh);
  publish(i,now,0,reached);
  success(i,"Reed stable; wiring not fault-supervised",now);
}
void setRom(unsigned i,uint64_t rom){
  if(!guard||xSemaphoreTake(guard,pdMS_TO_TICKS(8))!=pdTRUE)return;
  snprintf(state.channels[i].rom,sizeof(state.channels[i].rom),
           "%016llX",(unsigned long long)rom);
  xSemaphoreGive(guard);
}
void thermometer(unsigned i,uint32_t now){
  Driver& d=driver[i];
  if(!d.bus){
    if(due(now,d.due)){
      fail(i,"RMT bus not available");
      d.due=now+RETRY_MS;
    }
    return;
  }
  if(!due(now,d.due))return;
  if(d.converting){
    float c=NAN;
    const uint8_t err=d.bus->getTemp(d.rom,c);
    d.converting=false;
    d.due=millis()+PERIOD_MS;
    if(err==0 && isfinite(c)&&c>=-55.0f&&c<=125.0f){
      const uint32_t sampled=millis();
      publish(i,sampled,c,false);
      success(i,"DS18B20 temperature CRC verified",sampled);
    }else{
      fail(i,err==1?"DS18B20 scratchpad CRC failure":
               err==3?"DS18B20 no response":
               err==4?"RMT driver not ready":"DS18B20 invalid conversion");
    }
    return;
  }
  uint64_t devices[2]={};
  const uint8_t count=d.bus->search(devices,2);
  if(count!=1){
    fail(i,count?"More than one sensor: assign individual GPIO":
                 "DS18B20 not found; check 4.7 kOhm to 3V3",count);
    d.due=millis()+RETRY_MS;return;
  }
  uint8_t rom[8];
  for(unsigned n=0;n<8;n++)rom[n]=uint8_t(devices[0]>>(n*8));
  if(rom[0]!=0x28||crc8(rom,7)!=rom[7]){
    fail(i,"Unknown 1-Wire device or ROM CRC failure",count);
    d.due=millis()+RETRY_MS;return;
  }
  d.rom=devices[0];
  setRom(i,d.rom);
  d.bus->request();
  d.converting=true;
  d.due=millis()+CONVERSION_MS;
}
void worker(void*){
  vTaskDelay(pdMS_TO_TICKS(1800));
  for(;;){
    panel_config::State config;
    if(panel_config::snapshot(config)){
      if(config.revision!=activeRevision)reconfigure(config);
      const uint32_t now=millis();
      for(unsigned i=0;i<panel_config::COUNT;i++){
        const auto type=driver[i].setting.type;
        if(type==panel_config::Type::DS18B20)thermometer(i,now);
        if(type==panel_config::Type::ReedNO||
           type==panel_config::Type::ReedNC)reed(i,now);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(45));
  }
}
} // namespace
bool begin(){
  if(task)return true;
  guard=xSemaphoreCreateMutex();
  samples=xQueueCreate(24,sizeof(Event));
  if(!guard||!samples)return false;
  if(xTaskCreate(worker,"boiler-sensors",12288,nullptr,1,&task)!=pdPASS){
    task=nullptr;return false;
  }
  state.started=true;
  Serial.println("[BP-SENSOR] real inputs enabled only after NVS GPIO assignment");
  return true;
}
bool poll(Event& out){
  return samples&&xQueueReceive(samples,&out,0)==pdTRUE;
}
bool snapshot(Snapshot& out){
  if(!guard||xSemaphoreTake(guard,0)!=pdTRUE)return false;
  out=state;
  const uint32_t now=millis();
  for(auto& c:out.channels)if(c.valid && uint32_t(now-c.lastGoodMs)>30000U)c.valid=false;
  xSemaphoreGive(guard);
  return true;
}
} // namespace panel_sensors
