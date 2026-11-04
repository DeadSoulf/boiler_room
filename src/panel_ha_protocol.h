#pragma once
// Pure, bounded protocol code: no network, GPIO, credentials in payloads or commands.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

namespace panel_ha {
constexpr uint32_t SAMPLE_TTL_MS=30000, SOURCE_TTL_MS=5000, PUBLISH_MS=5000;
constexpr unsigned EXPIRE_SECONDS=15;
struct Config {
  char host[129]={},username[65]={},password[129]={},prefix[65]="homeassistant";
  uint16_t port=1883;
  uint8_t enabled=0;
};
inline void wipe(void* p,size_t n){volatile uint8_t* b=static_cast<volatile uint8_t*>(p);while(n--)*b++=0;}
template<size_t N> inline bool terminated(const char (&v)[N]){return memchr(v,0,N)!=nullptr;}
inline bool slug(const char* s){
  if(!s||!*s)return false;
  for(;*s;s++)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='_'||*s=='-'))return false;
  return true;
}
inline bool hostName(const char* s){
  if(!s||!*s)return false;
  for(;*s;s++)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='.'||*s=='-'||*s=='_'))return false;
  return true; // Hostname/IPv4 only, not a URI or IPv6 literal.
}
inline bool cleanText(const char* s){for(;*s;s++)if(static_cast<unsigned char>(*s)<32||*s==127)return false;return true;}
inline bool validConfig(const Config& c){
  return terminated(c.host)&&terminated(c.username)&&terminated(c.password)&&terminated(c.prefix)&&
    c.enabled<=1&&c.port>0&&slug(c.prefix)&&cleanText(c.username)&&cleanText(c.password)&&
    (!c.enabled||hostName(c.host))&&(!c.host[0]||hostName(c.host));
}
inline bool sameConfig(const Config& a,const Config& b){
  return a.enabled==b.enabled&&a.port==b.port&&!strcmp(a.host,b.host)&&!strcmp(a.username,b.username)&&
    !strcmp(a.password,b.password)&&!strcmp(a.prefix,b.prefix);
}
struct Input {
  bool valid=false,reached=false;
  float temperature=0;
  uint32_t updated=0;
};
struct Telemetry {
  Input input[6];
  uint32_t captured=0;
  bool received=false;
};
struct Diagnostics {
  uint32_t uptime=0,freeHeap=0;
  int32_t rssi=0;
  uint64_t sdFree=0;
  bool wifi=false,sdKnown=false,sdMounted=false;
};
inline bool fresh(const Telemetry& t,unsigned i,uint32_t now){
  return i<6&&t.received&&uint32_t(now-t.captured)<SOURCE_TTL_MS&&t.input[i].valid&&
    uint32_t(now-t.input[i].updated)<SAMPLE_TTL_MS&&
    (i>=4||(isfinite(t.input[i].temperature)&&t.input[i].temperature>=-55&&t.input[i].temperature<=125));
}
struct Entity {const char* key;const char* name;const char* kind;const char* deviceClass;const char* unit;bool diagnostic;};
constexpr Entity ENTITIES[]={
  {"room","Температура в котельной","sensor","temperature","°C",false},
  {"outside","Температура на улице","sensor","temperature","°C",false},
  {"coolant","Температура теплоносителя","sensor","temperature","°C",false},
  {"fuel_temp","Температура топлива","sensor","temperature","°C",false},
  {"fuel_lower","Нижний уровень топлива достигнут","binary_sensor","","",false},
  {"fuel_upper","Верхний уровень топлива достигнут","binary_sensor","","",false},
  {"fuel_level","Уровень топлива","sensor","","",false},
  {"wifi_rssi","Сигнал Wi-Fi","sensor","signal_strength","dBm",true},
  {"uptime","Время работы","sensor","duration","s",true},
  {"free_heap","Свободная память","sensor","data_size","B",true},
  {"sd_mounted","microSD подключена","binary_sensor","","",true},
  {"sd_free","Свободно на microSD","sensor","data_size","B",true}
};
constexpr unsigned ENTITY_COUNT=sizeof(ENTITIES)/sizeof(ENTITIES[0]);
class Json {
  char* out;size_t cap,pos=0;bool good;
public:
  Json(char* p,size_t n):out(p),cap(n),good(p&&n){if(good)out[0]=0;}
  void add(const char* fmt,...){
    if(!good)return;
    va_list args;va_start(args,fmt);int n=vsnprintf(out+pos,cap-pos,fmt,args);va_end(args);
    if(n<0||size_t(n)>=cap-pos){good=false;out[0]=0;return;}pos+=size_t(n);
  }
  void quote(const char* s){
    add("\"");
    if(s)for(const unsigned char* p=reinterpret_cast<const unsigned char*>(s);*p;p++){
      if(*p=='"'||*p=='\\')add("\\%c",*p);
      else if(*p<32)add("\\u%04x",unsigned(*p));else add("%c",*p);
    }
    add("\"");
  }
  bool ok()const{return good;}
};
inline bool discoveryTopic(char* out,size_t cap,const Config& c,const char* id,unsigned i){
  if(!out||!cap||i>=ENTITY_COUNT||!validConfig(c)||!slug(id))return false;
  int n=snprintf(out,cap,"%s/%s/%s_%s/config",c.prefix,ENTITIES[i].kind,id,ENTITIES[i].key);
  return n>=0&&size_t(n)<cap;
}
inline bool discovery(char* out,size_t cap,const char* id,const char* version,unsigned i){
  if(i>=ENTITY_COUNT||!slug(id))return false;
  const auto& e=ENTITIES[i];Json j(out,cap);
  j.add("{\"name\":");j.quote(e.name);
  j.add(",\"unique_id\":\"%s_%s\",\"state_topic\":\"boiler_panel/%s/state\",",id,e.key,id);
  j.add("\"value_template\":\"{{ value_json.%s }}\",\"expire_after\":%u,",e.key,EXPIRE_SECONDS);
  j.add("\"availability_mode\":\"all\",\"availability\":[{\"topic\":\"boiler_panel/%s/availability\"},",id);
  j.add("{\"topic\":\"boiler_panel/%s/state\",\"value_template\":\"{{ 'online' if value_json.%s_valid else 'offline' }}\"}],",id,e.key);
  j.add("\"device\":{\"identifiers\":[\"%s\"],\"name\":\"Котельная %s\",\"manufacturer\":\"GUITION\",\"model\":\"JC4880P443\",\"sw_version\":",id,id);j.quote(version);j.add("}");
  if(e.deviceClass[0]){j.add(",\"device_class\":");j.quote(e.deviceClass);}
  if(e.unit[0]){j.add(",\"unit_of_measurement\":");j.quote(e.unit);j.add(",\"state_class\":\"measurement\"");}
  if(!strcmp(e.kind,"binary_sensor"))j.add(",\"payload_on\":\"ON\",\"payload_off\":\"OFF\"");
  if(e.diagnostic)j.add(",\"entity_category\":\"diagnostic\"");
  j.add("}");return j.ok();
}
inline bool statePayload(char* out,size_t cap,const Telemetry& t,const Diagnostics& d,uint32_t now){
  Json j(out,cap);j.add("{");
  for(unsigned i=0;i<6;i++){
    const bool valid=fresh(t,i,now);
    if(i){j.add(",");}
    j.add("\"%s\":",ENTITIES[i].key);
    if(!valid)j.add("null");
    else if(i<4)j.add("%.2f",double(t.input[i].temperature));
    else j.quote(t.input[i].reached?"ON":"OFF");
    j.add(",\"%s_valid\":%s",ENTITIES[i].key,valid?"true":"false");
  }
  const bool fuelOK=fresh(t,4,now)&&fresh(t,5,now);
  j.add(",\"fuel_level\":");
  if(!fuelOK)j.add("null");
  else j.quote(t.input[5].reached?(t.input[4].reached?"upper_reached":"inconsistent"):
      (t.input[4].reached?"between":"below_lower"));
  j.add(",\"fuel_level_valid\":%s",fuelOK?"true":"false");
  j.add(",\"wifi_rssi\":%ld,\"wifi_rssi_valid\":%s",(long)d.rssi,d.wifi?"true":"false");
  j.add(",\"uptime\":%lu,\"uptime_valid\":true,\"free_heap\":%lu,\"free_heap_valid\":true",(unsigned long)d.uptime,(unsigned long)d.freeHeap);
  j.add(",\"sd_mounted\":");if(d.sdKnown)j.quote(d.sdMounted?"ON":"OFF");else j.add("null");
  j.add(",\"sd_mounted_valid\":%s,\"sd_free\":",d.sdKnown?"true":"false");
  const bool sdOK=d.sdKnown&&d.sdMounted;
  if(sdOK)j.add("%llu",(unsigned long long)d.sdFree);else j.add("null");
  j.add(",\"sd_free_valid\":%s}",sdOK?"true":"false");return j.ok();
}
} // namespace panel_ha
