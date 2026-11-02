#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
#include "panel_config.h"

namespace panel_config {
namespace {
struct Persist {
 uint32_t magic=0, schema=1, revision=0;
 Assignment item[COUNT];
 uint32_t hash=0;
};
constexpr uint32_t MAGIC=0x42504336; // BPC6
SemaphoreHandle_t lock=nullptr;
State current;
uint8_t activeSlot=0;
bool ready=false;
uint32_t fnv(const uint8_t* p,size_t n){
 uint32_t h=2166136261U;
 for(size_t i=0;i<n;i++){h^=p[i];h*=16777619U;}
 return h;
}
bool decode(unsigned slot,Persist& entry){
 Preferences prefs;
 if(!prefs.begin(slot?"bp_cfg_b":"bp_cfg_a",true))return false;
 const bool valid=prefs.getBytesLength("config")==sizeof(entry) &&
   prefs.getBytes("config",&entry,sizeof(entry))==sizeof(entry);
 prefs.end();
 if(!valid||entry.magic!=MAGIC||entry.schema!=1)return false;
 const uint32_t checksum=fnv(reinterpret_cast<const uint8_t*>(&entry),sizeof(entry)-sizeof(entry.hash));
 return checksum==entry.hash;
}
bool save(unsigned slot,const State& next){
 Persist entry{};
 entry.magic=MAGIC;entry.schema=1;entry.revision=next.revision;
 for(unsigned i=0;i<COUNT;i++)entry.item[i]=next.item[i];
 entry.hash=fnv(reinterpret_cast<const uint8_t*>(&entry),sizeof(entry)-sizeof(entry.hash));
 Preferences prefs;
 if(!prefs.begin(slot?"bp_cfg_b":"bp_cfg_a",false))return false;
 const bool written=prefs.putBytes("config",&entry,sizeof(entry))==sizeof(entry);
 prefs.end();
 Persist check{};
 return written&&decode(slot,check)&&check.hash==entry.hash&&check.revision==entry.revision;
}
bool allowedType(Channel ch,Type t) {
 if(uint8_t(t)>uint8_t(Type::RemoteMQTT))return false;
 const bool temp=uint8_t(ch)<4;
 if(t==Type::None)return true;
 return temp?(t==Type::DS18B20||t==Type::RemoteModbus||t==Type::RemoteMQTT):
    (t==Type::ReedNO||t==Type::ReedNC||t==Type::RemoteModbus||t==Type::RemoteMQTT);
}
} // namespace

const char* pinRestriction(int gpio){
 if(gpio<0||gpio>54)return "GPIO вне диапазона ESP32-P4";
 switch(gpio){
 case 3:case 5:case 7:case 8:case 23:return "Занят дисплеем или сенсором GT911";
 case 14:case 15:case 16:case 17:case 18:case 19:case 54:return "Зарезервирован для ESP32-C6 / SDIO";
 case 39:case 40:case 41:case 42:case 43:case 44:return "Занят microSD / SDMMC";
 case 9:case 10:case 11:case 12:case 13:case 48:return "Зарезервирован для аудио";
 case 26:case 27:return "Зарезервирован для RS-485";
 case 35:return "Кнопка BOOT";
 case 37:case 38:return "USB/UART";
 default:return "Нет подтверждения доступности на физическом разъёме";
 }
}
const char* typeName(Type t){
 switch(t){
 case Type::None:return "none";
 case Type::DS18B20:return "ds18b20";
 case Type::ReedNO:return "reed_no";
 case Type::ReedNC:return "reed_nc";
 case Type::RemoteModbus:return "modbus";
 case Type::RemoteMQTT:return "mqtt";
 default:return "invalid";
 }
}
bool begin(){
 if(ready)return true;
 lock=xSemaphoreCreateMutex();
 if(!lock)return false;
 Persist a{},b{};
 const bool validA=decode(0,a),validB=decode(1,b);
 if(validA||validB){
   activeSlot=(validB&&(!validA||int32_t(b.revision-a.revision)>0))?1:0;
   const Persist& v=activeSlot?b:a;
   current.revision=v.revision;
   for(unsigned i=0;i<COUNT;i++)current.item[i]=v.item[i];
   snprintf(current.message,sizeof(current.message),"NVS: profile %lu loaded",(unsigned long)v.revision);
 }else{
   current=State{};
   snprintf(current.message,sizeof(current.message),"No GPIO assignments; configure in web");
 }
 ready=true;
 return true;
}
bool snapshot(State& out){
 if(!ready||xSemaphoreTake(lock,pdMS_TO_TICKS(5))!=pdTRUE)return false;
 out=current;xSemaphoreGive(lock);return true;
}
bool set(Channel ch,Type type,int16_t gpio,uint8_t addr,uint16_t debounce){
 if(!ready||unsigned(ch)>=COUNT||!allowedType(ch,type))return false;
 if(type==Type::DS18B20||type==Type::ReedNO||type==Type::ReedNC){
   // Design-time assignments until board connector is physically verified.
   // This firmware deliberately refuses to drive unconfirmed GPIOs.
   if(gpio<0||gpio>54)return false;
   if(strcmp(pinRestriction(gpio),"Нет подтверждения доступности на физическом разъёме")!=0)return false;
 }else if(gpio!=-1)return false;
 if(addr>247||debounce>5000)return false;
 if(xSemaphoreTake(lock,pdMS_TO_TICKS(40))!=pdTRUE)return false;
 State next=current;
 for(unsigned i=0;i<COUNT;i++)if(i!=unsigned(ch) && gpio>=0 && next.item[i].gpio==gpio){
   xSemaphoreGive(lock);return false;
 }
 next.item[unsigned(ch)].type=type;
 next.item[unsigned(ch)].gpio=gpio;
 next.item[unsigned(ch)].busAddress=addr;
 next.item[unsigned(ch)].debounceMs=debounce;
 next.revision++;
 snprintf(next.message,sizeof(next.message),"Saved. GPIO drivers not activated pending connector verification.");
 const unsigned target=activeSlot^1;
 const bool good=save(target,next);
 if(good){current=next;activeSlot=target;}
 xSemaphoreGive(lock);
 return good;
}
bool reset(Channel ch){return set(ch,Type::None,-1,0,50);}
} // namespace panel_config
