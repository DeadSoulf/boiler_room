#include "panel_ha_protocol.h"
#include <cassert>
#include <string>
#include <iostream>
#include <limits>
using namespace panel_ha;
int main(){
 Config c;assert(validConfig(c)&&!c.enabled&&c.port==1883);
 c.enabled=1;assert(!validConfig(c));strcpy(c.host,"192.0.2.1");assert(validConfig(c));
 for(const char* bad:{"mqtt://host","host:1883","host/path","host@x","host\n","[::1]"}){strcpy(c.host,bad);assert(!validConfig(c));}
 strcpy(c.host,"mqtt.local");for(const char* bad:{"home/#","home+","home assistant","","x/../y"}){strcpy(c.prefix,bad);assert(!validConfig(c));}
 strcpy(c.prefix,"homeassistant");memset(c.password,'x',sizeof(c.password));assert(!validConfig(c));c.password[128]=0;assert(validConfig(c));
 c.port=0;assert(!validConfig(c));c.port=65535;assert(validConfig(c));c.enabled=2;assert(!validConfig(c));c.enabled=1;
 char small[4];assert(!discovery(small,sizeof(small),"bp_0123456789ab","v0.7.0",0));
 char payload[2048],topic[192];assert(!discovery(payload,sizeof(payload),"bad/id","v0.7.0",0));
 assert(!discoveryTopic(topic,sizeof(topic),c,"bp_x",ENTITY_COUNT));
 for(unsigned i=0;i<ENTITY_COUNT;i++){
  assert(discoveryTopic(topic,sizeof(topic),c,"bp_0123456789ab",i));
  assert(discovery(payload,sizeof(payload),"bp_0123456789ab","v0.7.0",i));
  assert(!strstr(payload,"command_topic"));std::cout<<"CONFIG "<<topic<<" "<<payload<<"\n";
 }
 Json escape(payload,sizeof(payload));escape.quote("x\"\\\n");assert(std::string(payload)=="\"x\\\"\\\\\\u000a\"");
 Telemetry t;Diagnostics d;assert(!fresh(t,0,1000));
 assert(statePayload(payload,sizeof(payload),t,d,1000));std::cout<<"EMPTY "<<payload<<"\n";
 t.received=true;t.captured=1000;
 for(auto& in:t.input){in.valid=true;in.updated=1000;in.temperature=-12.25f;}
 t.input[4].reached=true;d.wifi=true;d.rssi=-60;d.sdKnown=true;d.sdMounted=true;d.sdFree=62602392064ULL;
 assert(fresh(t,0,1000)&&fresh(t,0,5999)&&!fresh(t,0,6000));
 assert(statePayload(payload,sizeof(payload),t,d,1000));std::cout<<"LIVE "<<payload<<"\n";
 t.input[5].reached=true;t.input[4].reached=false;assert(statePayload(payload,sizeof(payload),t,d,1000));std::cout<<"INCONSISTENT "<<payload<<"\n";
 t.input[0].temperature=std::numeric_limits<float>::quiet_NaN();assert(!fresh(t,0,1000));
 t.input[0].temperature=126;assert(!fresh(t,0,1000));t.input[0].temperature=-56;assert(!fresh(t,0,1000));
 t.input[0].temperature=0;assert(fresh(t,0,1000));
 t.captured=31000;assert(!fresh(t,0,31000));assert(statePayload(payload,sizeof(payload),t,d,31000));std::cout<<"STALE "<<payload<<"\n";
 t.captured=UINT32_MAX-500;for(auto& in:t.input)in.updated=t.captured;
 assert(fresh(t,0,499));assert(!fresh(t,0,5000));assert(!fresh(t,6,0));
 assert(!statePayload(small,sizeof(small),t,d,499));
 std::cout<<"PASS: bounded JSON, discovery, nulls, freshness, rollover, credential/host validation\n";
}
