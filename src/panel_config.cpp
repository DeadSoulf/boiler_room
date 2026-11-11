// Versioned dual NVS slots; all changes validated and read back before activation.
// GPIO assignments always belong to physically mapped JP1 pins.
#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
#include "panel_config.h"
#include "panel_pin_profile.h"

namespace panel_config {
namespace {
template<unsigned N> struct StoredConfig {
  uint32_t magic=0,schema=0,revision=0;
  Assignment item[N];
  uint32_t hash=0;
};
using Persist=StoredConfig<COUNT>;
constexpr uint32_t MAGIC=0x42504336U;
SemaphoreHandle_t lock=nullptr, sensorIo=nullptr;
State current;
uint8_t activeSlot=0;
bool ready=false;
bool sawInvalidStored=false;
uint32_t fnv(const uint8_t* p,size_t n){
  uint32_t h=2166136261U;
  for(size_t i=0;i<n;i++){h^=p[i];h*=16777619U;}
  return h;
}
bool allowedType(Channel ch,Type t){
  if(t==Type::None)return true;
  if(unsigned(ch)>=INPUT_COUNT)return t==Type::Relay;
  const bool temperature=temperatureIndex(unsigned(ch))>=0;
  return temperature?
    (t==Type::DS18B20||t==Type::RemoteModbus||t==Type::RemoteMQTT):
    (t==Type::ReedNO||t==Type::ReedNC||t==Type::RemoteModbus||t==Type::RemoteMQTT);
}
template<unsigned N> bool checked(const Assignment (&list)[N]){
  for(unsigned i=0;i<N;i++){
    if(!isValid(static_cast<Channel>(i),list[i]))return false;
    if(list[i].gpio<0)continue;
    for(unsigned j=i+1;j<N;j++){
      if(list[i].gpio==list[j].gpio)return false;
    }
  }
  return true;
}
template<unsigned N> bool readRecord(Preferences& prefs,uint32_t schema,StoredConfig<N>& entry){
  if(prefs.getBytes("config",&entry,sizeof(entry))!=sizeof(entry))return false;
  if(entry.magic!=MAGIC||entry.schema!=schema)return false;
  const uint32_t h=fnv(reinterpret_cast<const uint8_t*>(&entry),sizeof(entry)-sizeof(entry.hash));
  if(h!=entry.hash || !checked(entry.item))return false;
  return true;
}
template<unsigned N> bool readPrevious(Preferences& prefs,uint32_t schema,Persist& entry){
  StoredConfig<N> previous{};
  if(!readRecord(prefs,schema,previous))return false;
  entry=Persist{};
  entry.revision=previous.revision;
  for(unsigned i=0;i<N;i++)entry.item[i]=previous.item[i];
  return true;
}
bool readSlot(unsigned slot,Persist& entry){
  Preferences prefs;
  if(!prefs.begin(slot?"bp_cfg_b":"bp_cfg_a",true))return false;
  const size_t size=prefs.getBytesLength("config");
  bool ok=false;
  if(size==sizeof(Persist)){
    ok=readRecord(prefs,5,entry);
  }else if(size==sizeof(StoredConfig<10>)){
    ok=readPrevious<10>(prefs,4,entry);
  }else if(size==sizeof(StoredConfig<8>)){
    ok=readPrevious<8>(prefs,3,entry);
  }else if(size==sizeof(StoredConfig<7>)){
    ok=readPrevious<7>(prefs,2,entry);
  }else if(size==sizeof(StoredConfig<6>)){
    ok=readPrevious<6>(prefs,1,entry);
  }
  prefs.end();return ok;
}
bool writeSlot(unsigned slot,const State& next){
  Persist entry{};
  entry.magic=MAGIC;entry.schema=5;entry.revision=next.revision;
  for(unsigned i=0;i<COUNT;i++)entry.item[i]=next.item[i];
  entry.hash=fnv(reinterpret_cast<const uint8_t*>(&entry),sizeof(entry)-sizeof(entry.hash));
  Preferences prefs;
  if(!prefs.begin(slot?"bp_cfg_b":"bp_cfg_a",false))return false;
  // Keep the exact previous bytes, including records from older schemas.
  uint8_t previous[sizeof(Persist)];
  const size_t previousSize=prefs.getBytesLength("config");
  if(previousSize>sizeof(previous) ||
     (previousSize && prefs.getBytes("config",previous,previousSize)!=previousSize)){
    prefs.end();return false;
  }
  const bool stored=prefs.putBytes("config",&entry,sizeof(entry))==sizeof(entry);
  prefs.end();
  Persist verification{};
  const bool verified=stored&&readSlot(slot,verification)&&
         verification.revision==entry.revision&&
         memcmp(verification.item,entry.item,sizeof(entry.item))==0;
  if(verified)return true;
  // A failed read-back must not leave a newer candidate for rollback/reboot.
  // Recovery is best effort: a persistent NVS write fault can also prevent it.
  if(prefs.begin(slot?"bp_cfg_b":"bp_cfg_a",false)){
    if(previousSize)prefs.putBytes("config",previous,previousSize);
    else prefs.remove("config");
    prefs.end();
  }
  return false;
}
PinResult commit(const State& next){
  // Wait for an in-flight sensor transaction before handing its GPIO to a new
  // owner. Status snapshots do not need this second mutex during normal polling.
  if(xSemaphoreTake(sensorIo,pdMS_TO_TICKS(200))!=pdTRUE)return PinResult::Unavailable;
  const unsigned target=activeSlot^1U;
  const bool verified=writeSlot(target,next);
  if(verified){current=next;activeSlot=target;}
  xSemaphoreGive(sensorIo);
  return verified?PinResult::Saved:PinResult::StorageError;
}
PinResult changePin(int16_t gpio,unsigned channel,const Assignment* assignment,
                    uint32_t expectedRevision,uint32_t& savedRevision){
  if(!ready||xSemaphoreTake(lock,pdMS_TO_TICKS(45))!=pdTRUE)return PinResult::Unavailable;
  if(current.revision!=expectedRevision){
    xSemaphoreGive(lock);return PinResult::Conflict;
  }
  State next=current;
  for(unsigned i=0;i<COUNT;i++)if(next.item[i].gpio==gpio)next.item[i]=Assignment{};
  if(assignment)next.item[channel]=*assignment;
  if(!checked(next.item)){
    xSemaphoreGive(lock);return PinResult::Invalid;
  }
  bool unchanged=true;
  for(unsigned i=0;i<COUNT;i++){
    const Assignment& a=current.item[i];const Assignment& b=next.item[i];
    if(a.type!=b.type||a.gpio!=b.gpio||a.busAddress!=b.busAddress||a.debounceMs!=b.debounceMs){
      unchanged=false;break;
    }
  }
  if(!unchanged){
    ++next.revision;
    snprintf(next.message,sizeof(next.message),"NVS saved and verified (rev %lu)",
      (unsigned long)next.revision);
    const auto result=commit(next);
    if(result!=PinResult::Saved){xSemaphoreGive(lock);return result;}
  }
  savedRevision=current.revision;
  xSemaphoreGive(lock);return PinResult::Saved;
}
} // namespace

bool isLocal(Type type){
  return type==Type::DS18B20||type==Type::ReedNO||type==Type::ReedNC||type==Type::Relay;
}
const char* pinRestriction(int gpio){
  const char* restricted=panel_pins::blockReason(gpio);
  return restricted?restricted:"Available: verified JP1 GPIO";
}
bool isValid(Channel ch,const Assignment& a){
  if(unsigned(ch)>=COUNT||!allowedType(ch,a.type))return false;
  if(a.debounceMs>5000 || a.busAddress>247)return false;
  if(isLocal(a.type)){
    if(!panel_pins::sensorPin(a.gpio) || a.busAddress!=0)return false;
    if((a.type==Type::DS18B20||a.type==Type::Relay) && a.debounceMs!=50)return false;
    return true;
  }
  if(a.gpio!=-1)return false;
  if(a.type==Type::RemoteModbus)return a.busAddress>=1;
  return a.busAddress==0;
}
const char* typeName(Type type){
  switch(type){
    case Type::None:return "none";
    case Type::DS18B20:return "ds18b20";
    case Type::ReedNO:return "reed_no";
    case Type::ReedNC:return "reed_nc";
    case Type::RemoteModbus:return "modbus";
    case Type::RemoteMQTT:return "mqtt";
    case Type::Relay:return "relay";
    default:return "invalid";
  }
}
bool begin(){
  if(ready)return true;
  if(!lock)lock=xSemaphoreCreateMutex();
  if(!sensorIo)sensorIo=xSemaphoreCreateMutex();
  if(!lock||!sensorIo)return false;
  Persist a{},b{};
  const bool okA=readSlot(0,a),okB=readSlot(1,b);
  if(okA||okB){
    activeSlot=(okB&&(!okA||int32_t(b.revision-a.revision)>0))?1:0;
    const Persist& source=activeSlot?b:a;
    current.revision=source.revision;
    for(unsigned i=0;i<COUNT;i++)current.item[i]=source.item[i];
    snprintf(current.message,sizeof(current.message),"NVS revision %lu verified",
      (unsigned long)current.revision);
  }else{
    current=State{};
    snprintf(current.message,sizeof(current.message),"No validated JP1 assignments in NVS");
  }
  ready=true;
  return true;
}
bool snapshot(State& out){
  if(!ready||xSemaphoreTake(lock,pdMS_TO_TICKS(10))!=pdTRUE)return false;
  out=current;xSemaphoreGive(lock);return true;
}
void withSensorAssignments(void (*sample)(const State&)){
  if(!ready||xSemaphoreTake(sensorIo,pdMS_TO_TICKS(10))!=pdTRUE)return;
  sample(current);
  xSemaphoreGive(sensorIo);
}
bool set(Channel ch,Type type,int16_t gpio,uint8_t address,uint16_t debounceMs){
  if(!ready)return false;
  Assignment assignment{};
  assignment.type=type;
  assignment.gpio=gpio;
  assignment.busAddress=address;
  assignment.debounceMs=debounceMs;
  if(!isValid(ch,assignment))return false;
  if(xSemaphoreTake(lock,pdMS_TO_TICKS(45))!=pdTRUE)return false;
  State next=current;
  next.item[unsigned(ch)]=assignment;
  if(!checked(next.item)){
    xSemaphoreGive(lock);return false;
  }
  const bool unchanged=memcmp(current.item,next.item,sizeof(next.item))==0;
  if(unchanged){xSemaphoreGive(lock);return true;}
  ++next.revision;
  snprintf(next.message,sizeof(next.message),"NVS saved and verified (rev %lu)",
       (unsigned long)next.revision);
  const bool ok=commit(next)==PinResult::Saved;
  xSemaphoreGive(lock);
  return ok;
}
bool reset(Channel ch){
  return set(ch,Type::None,-1,0,50);
}
PinResult setPin(int16_t gpio,Channel channel,Type type,uint16_t debounceMs,
                 uint32_t expectedRevision,uint32_t& savedRevision){
  Assignment assignment{};
  assignment.type=type;assignment.gpio=gpio;assignment.debounceMs=debounceMs;
  if(!isLocal(type)||!isValid(channel,assignment))return PinResult::Invalid;
  return changePin(gpio,unsigned(channel),&assignment,expectedRevision,savedRevision);
}
PinResult clearPin(int16_t gpio,uint32_t expectedRevision,uint32_t& savedRevision){
  if(!panel_pins::sensorPin(gpio))return PinResult::Invalid;
  return changePin(gpio,0,nullptr,expectedRevision,savedRevision);
}
bool rollback(){
  if(!ready||xSemaphoreTake(lock,pdMS_TO_TICKS(45))!=pdTRUE)return false;
  Persist previous{};
  const bool oldValid=readSlot(activeSlot^1U,previous);
  if(!oldValid || int32_t(current.revision-previous.revision)<=0){
    xSemaphoreGive(lock);return false;
  }
  State restored=current;
  for(unsigned i=0;i<COUNT;i++)restored.item[i]=previous.item[i];
  ++restored.revision;
  snprintf(restored.message,sizeof(restored.message),"Restored previous NVS configuration");
  const bool ok=commit(restored)==PinResult::Saved;
  xSemaphoreGive(lock);return ok;
}
} // namespace panel_config
