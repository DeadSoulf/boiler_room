// JC4880P443 ESP32-P4 / Arduino 3.3.6. This is an on-device web server.
// A physical touch action is required before any HTTP command is accepted.
// HTTP is NOT encrypted: local trusted LAN only, never port forward.
#include <Arduino.h>
#include <sdkconfig.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <time.h>
#include <string.h>
#include "panel_web.h"
#include "panel_radio.h"
#include "panel_equipment.h"
#include "panel_screen.h"
#include "panel_storage.h"
#if defined(CONFIG_ESP_WIFI_REMOTE_ENABLED) && CONFIG_ESP_WIFI_REMOTE_ENABLED
#define BP_WEB_ENABLED 1
#include <WebServer.h>
#include <WiFi.h>
#include "panel_web_page.h"
#else
#define BP_WEB_ENABLED 0
#endif

namespace panel_web {
namespace {
using namespace panel;
struct TempSnap {
  bool valid=false, ever=false;
  float value=0,minimum=0,maximum=0,trend=0;
  uint32_t age=0,count=0;
  float history[HISTORY_POINTS]={};
  uint32_t historyAge[HISTORY_POINTS]={};
};
struct Shared {
  TempSnap temp[CHANNELS];
  uint8_t fuelStatus=0;
  bool lowerValid=false,lowerReached=false,upperValid=false,upperReached=false;
  uint32_t lowerAge=0,upperAge=0;
  uint32_t uptime=0,freeHeap=0,displayMs=0,lvglMs=0,firstFrameMs=0,touchMs=0;
  bool touchReady=false,published=false;
};
Shared latest;
SemaphoreHandle_t guard=nullptr;
TaskHandle_t httpTask=nullptr;
char physicalPin[7]="";
uint32_t pinExpires=0;
uint8_t failedCodes=0;
char sessionToken[33]="";
uint32_t sessionExpires=0;

bool future(uint32_t deadline) {
  return deadline && int32_t(deadline-millis())>0;
}
void clearPairingLocked() {
  memset(physicalPin,0,sizeof(physicalPin));
  pinExpires=0;failedCodes=0;
}
void clearSessionLocked() {
  memset(sessionToken,0,sizeof(sessionToken));sessionExpires=0;
}
void jsonQuote(String& j,const char* s) {
  j+='"';
  if(s)for(const unsigned char* p=reinterpret_cast<const unsigned char*>(s);*p;p++){
    if(*p=='"' || *p=='\\'){j+='\\';j+=char(*p);}
    else if(*p<32){char b[7];snprintf(b,sizeof(b),"\\u%04x",unsigned(*p));j+=b;}
    else j+=char(*p);
  }
  j+='"';
}
const char* fuelName(uint8_t value) {
  switch(static_cast<FuelLevelStatus>(value)){
    case FuelLevelStatus::BelowLower:return "Ниже нижнего датчика";
    case FuelLevelStatus::Between:return "Между датчиками";
    case FuelLevelStatus::UpperReached:return "Верхний датчик достигнут";
    case FuelLevelStatus::Inconsistent:return "Противоречивые показания";
    case FuelLevelStatus::Stale:return "Данные устарели";
    default:return "Нет данных";
  }
}
bool allDigits(const String& s,unsigned min,unsigned max) {
  if(s.length()<min||s.length()>max)return false;
  for(unsigned i=0;i<s.length();i++)if(s[i]<'0'||s[i]>'9')return false;
  return true;
}
uint32_t asUint(const String& s) {
  uint32_t val=0;for(unsigned i=0;i<s.length();i++)val=val*10+unsigned(s[i]-'0');return val;
}

#if BP_WEB_ENABLED
WebServer server(80);
void responseHeaders() {
  server.sendHeader("Cache-Control","no-store, max-age=0");
  server.sendHeader("X-Content-Type-Options","nosniff");
  server.sendHeader("X-Frame-Options","DENY");
  server.sendHeader("Referrer-Policy","no-referrer");
}
void jsonResponse(int status,const String& content) {
  responseHeaders();
  server.send(status,"application/json; charset=utf-8",content);
}
void errorResponse(int status,const char* code) {
  String j="{\"ok\":false,\"error\":";
  jsonQuote(j,code);j+='}';jsonResponse(status,j);
}
void homepage() {
  responseHeaders();server.send_P(200,"text/html; charset=utf-8",kBoilerWebPage);
}
bool authorized() {
  const String h=server.header("Authorization");
  if(!h.startsWith("Bearer ")||h.length()!=39||!guard)return false;
  if(xSemaphoreTake(guard,pdMS_TO_TICKS(30))!=pdTRUE)return false;
  const bool active=sessionToken[0] && future(sessionExpires);
  unsigned char diff=0;
  for(unsigned i=0;i<32;i++)diff|=uint8_t(h[7+i])^uint8_t(sessionToken[i]);
  const bool result=active && diff==0;
  xSemaphoreGive(guard);
  return result;
}
void pairRequest() {
  const String entered=server.arg("code");
  if(!allDigits(entered,6,6)){errorResponse(400,"invalid_code");return;}
  if(!guard || xSemaphoreTake(guard,pdMS_TO_TICKS(30))!=pdTRUE){errorResponse(503,"busy");return;}
  const bool active=physicalPin[0]&&future(pinExpires)&&failedCodes<5;
  unsigned char diff=0;
  for(unsigned i=0;i<6;i++)diff|=uint8_t(entered[i])^uint8_t(physicalPin[i]);
  if(!active || diff){
    if(active){++failedCodes;if(failedCodes>=5)clearPairingLocked();}
    xSemaphoreGive(guard);
    errorResponse(403,"code_invalid_or_expired");
    return;
  }
  static const char HEX_CHARS[]="0123456789abcdef";
  uint8_t secret[16];
  esp_fill_random(secret,sizeof(secret));
  for(unsigned i=0;i<sizeof(secret);i++){
    sessionToken[i*2]=HEX_CHARS[secret[i]>>4];
    sessionToken[i*2+1]=HEX_CHARS[secret[i]&15];
  }
  sessionToken[32]=0;
  memset(secret,0,sizeof(secret));
  sessionExpires=millis()+3600000U; // Single session, one hour after physical pairing.
  const String token(sessionToken);
  clearPairingLocked(); // One-use pairing code.
  xSemaphoreGive(guard);
  String j="{\"ok\":true,\"token\":";
  jsonQuote(j,token.c_str());
  j+=",\"expires_in_s\":3600}";
  jsonResponse(200,j);
}
void statusRequest() {
  Shared snap;
  if(!guard || xSemaphoreTake(guard,pdMS_TO_TICKS(30))!=pdTRUE){errorResponse(503,"busy");return;}
  snap=latest;
  const bool pairingActive=physicalPin[0]&&future(pinExpires);
  xSemaphoreGive(guard);
  if(!snap.published){errorResponse(503,"starting");return;}
  const bool admin=authorized();
  RadioState radio;
  if(!panel_radio::snapshot(radio))radio=RadioState{};
  char clock[40]="";
  const time_t current=time(nullptr);
  if(current>=time_t(1704067200)) {
    tm local{};localtime_r(&current,&local);
    strftime(clock,sizeof(clock),"%d.%m.%Y %H:%M:%S",&local);
  }
  String j;
  j.reserve(13000);
  j+="{\"version\":";jsonQuote(j,VERSION);
  j+=",\"uptime_s\":";j+=String(snap.uptime);
  j+=",\"clock\":";jsonQuote(j,clock);
  j+=",\"authorized\":";j+=admin?"true":"false";
  j+=",\"pair_available\":";j+=pairingActive?"true":"false";
  j+=",\"physical_control\":false,\"data_origin\":\"device\"";
  j+=",\"wifi\":{\"connected\":";j+=radio.connected?"true":"false";
  j+=",\"ready\":";j+=radio.ready?"true":"false";
  j+=",\"busy\":";j+=radio.busy?"true":"false";
  j+=",\"supported\":";j+=radio.wifiSupported?"true":"false";
  j+=",\"has_saved\":";j+=radio.hasSaved?"true":"false";
  j+=",\"auto_connect\":";j+=radio.autoConnect?"true":"false";
  j+=",\"job\":";j+=String(unsigned(radio.job));
  j+=",\"ip\":";jsonQuote(j,radio.ip);
  j+=",\"rssi\":";j+=String(radio.rssi);
  j+=",\"ssid\":";jsonQuote(j,admin?radio.ssid:"");
  j+=",\"message\":";jsonQuote(j,radio.wifiMessage);
  j+=",\"aps\":[";
  if(admin)for(unsigned i=0;i<radio.apCount;i++) {
    if(i)j+=',';
    j+="{\"ssid\":";jsonQuote(j,radio.aps[i].ssid);
    j+=",\"rssi\":";j+=String(radio.aps[i].rssi);
    j+=",\"auth\":";j+=String(radio.aps[i].auth);
    j+=",\"supported\":";j+=radio.aps[i].supported?"true":"false";
    j+='}';
  }
  j+="]}";
  j+=",\"ble\":{\"supported\":";j+=radio.bleSupported?"true":"false";
  j+=",\"message\":";jsonQuote(j,radio.bleMessage);
  j+=",\"devices\":[";
  if(admin)for(unsigned i=0;i<radio.bleCount;i++){
    if(i)j+=',';
    j+="{\"name\":";jsonQuote(j,radio.ble[i].name);
    j+=",\"address\":";jsonQuote(j,radio.ble[i].address);
    j+=",\"rssi\":";j+=String(radio.ble[i].rssi);j+='}';
  }
  j+="]}";
  j+=",\"time\":{\"zone\":";j+=String(unsigned(radio.zone));
  j+=",\"zone_name\":";jsonQuote(j,panel_radio::zoneName(radio.zone));
  j+=",\"message\":";jsonQuote(j,radio.timeMessage);j+='}';
  j+=",\"screen_timeout_s\":";j+=String(radio.displayTimeoutSec);
  j+=",\"screen\":{\"timeout_s\":";j+=String(radio.displayTimeoutSec);
  j+=",\"touch_ready\":";j+=snap.touchReady?"true":"false";
  j+=",\"message\":";jsonQuote(j,radio.displayMessage);j+='}';
  j+=",\"host_version\":";jsonQuote(j,radio.hostVersion);
  j+=",\"c6_version\":";jsonQuote(j,radio.c6Version);
  j+=",\"temperatures\":[";
  for(unsigned i=0;i<CHANNELS;i++){
    if(i)j+=',';
    const TempSnap& t=snap.temp[i];
    j+="{\"valid\":";j+=t.valid?"true":"false";
    j+=",\"ever\":";j+=t.ever?"true":"false";
    j+=",\"value\":";if(t.valid)j+=String(t.value,2);else j+="null";
    j+=",\"minimum\":";if(t.ever)j+=String(t.minimum,2);else j+="null";
    j+=",\"maximum\":";if(t.ever)j+=String(t.maximum,2);else j+="null";
    j+=",\"trend\":";if(t.count>=2)j+=String(t.trend,2);else j+="null";
    j+=",\"age_s\":";j+=String(t.age);
    j+=",\"history\":[";
    for(unsigned k=0;k<t.count;k++){
      if(k)j+=',';
      j+="{\"value\":";j+=String(t.history[k],2);
      j+=",\"age_s\":";j+=String(t.historyAge[k]);j+='}';
    }
    j+="]}";
  }
  j+="]";
  j+=",\"fuel_level_text\":";jsonQuote(j,fuelName(snap.fuelStatus));
  j+=",\"fuel\":{\"status\":";jsonQuote(j,fuelName(snap.fuelStatus));
  j+=",\"lower\":{\"valid\":";j+=snap.lowerValid?"true":"false";
  j+=",\"reached\":";j+=snap.lowerReached?"true":"false";
  j+=",\"age_s\":";j+=String(snap.lowerAge);
  j+="},\"upper\":{\"valid\":";j+=snap.upperValid?"true":"false";
  j+=",\"reached\":";j+=snap.upperReached?"true":"false";
  j+=",\"age_s\":";j+=String(snap.upperAge);j+="}}";
  j+=",\"equipment\":[";
  for(unsigned i=0;i<panel_equipment::COUNT;i++){
    if(i)j+=',';
    j+="{\"name\":";jsonQuote(j,panel_equipment::name(i));
    j+=",\"connected\":false,\"state\":\"not_connected\"}";
  }
  j+="]";
  panel_storage::Snapshot storage;
  const bool storageOK=panel_storage::snapshot(storage);
  j+=",\"storage\":{\"started\":";j+=(storageOK&&storage.started)?"true":"false";
  j+=",\"mounted\":";j+=storage.mounted?"true":"false";
  j+=",\"logging\":";j+=storage.logging?"true":"false";
  j+=",\"card_bytes\":";j+=String((unsigned long long)storage.cardBytes);
  j+=",\"total_bytes\":";j+=String((unsigned long long)storage.totalBytes);
  j+=",\"used_bytes\":";j+=String((unsigned long long)storage.usedBytes);
  j+=",\"free_bytes\":";j+=String((unsigned long long)storage.freeBytes);
  j+=",\"written_events\":";j+=String(storage.writtenEvents);
  j+=",\"written_samples\":";j+=String(storage.writtenSamples);
  j+=",\"dropped\":";j+=String(storage.dropped);
  j+=",\"message\":";jsonQuote(j,storage.message);j+='}';
  j+=",\"service\":{\"free_heap\":";j+=String(snap.freeHeap);
  j+=",\"display_ms\":";j+=String(snap.displayMs);
  j+=",\"lvgl_ms\":";j+=String(snap.lvglMs);
  j+=",\"first_frame_ms\":";j+=String(snap.firstFrameMs);
  j+=",\"touch_ms\":";j+=String(snap.touchMs);
  j+="}}";
  jsonResponse(200,j);
}

void commandRequest() {
  if(!authorized()){errorResponse(401,"pair_required");return;}
  const String kind=server.arg("action");
  panel::Command cmd;
  if(kind=="wifi_scan")cmd.job=Job::WifiScan;
  else if(kind=="wifi_disconnect")cmd.job=Job::Disconnect;
  else if(kind=="wifi_forget")cmd.job=Job::Forget;
  else if(kind=="ble_scan")cmd.job=Job::BleScan;
  else if(kind=="wifi_connect"){
    const String ssid=server.arg("ssid"),pass=server.arg("password");
    const bool open=server.arg("open")=="1";
    if(ssid.isEmpty()||ssid.length()>32||pass.length()>64||
       !passwordValid(pass.c_str(),open) || (open&&!pass.isEmpty())){
      errorResponse(400,"invalid_network_credentials");return;
    }
    cmd.job=Job::Connect;
    cmd.open=open;
    snprintf(cmd.ssid,sizeof(cmd.ssid),"%s",ssid.c_str());
    snprintf(cmd.password,sizeof(cmd.password),"%s",pass.c_str());
  }
  else if(kind=="zone"){
    const String val=server.arg("value");
    if(!allDigits(val,1,1)||asUint(val)>2){errorResponse(400,"invalid_zone");return;}
    cmd.job=Job::Zone;cmd.zone=uint8_t(asUint(val));
  }
  else if(kind=="clock"){
    const String val=server.arg("value");
    time_t timestamp;
    if(!parseLocalTime(val.c_str(),timestamp)){errorResponse(400,"invalid_time");return;}
    cmd.job=Job::SetClock;snprintf(cmd.input,sizeof(cmd.input),"%s",val.c_str());
  }
  else if(kind=="screen"){
    const String val=server.arg("value");
    if(!allDigits(val,1,4)){errorResponse(400,"invalid_timeout");return;}
    const uint32_t seconds=asUint(val);
    if(!(seconds==0||seconds==30||seconds==60||seconds==120||
         seconds==300||seconds==600||seconds==1800)){
      errorResponse(400,"invalid_timeout");return;
    }
    Shared snap;
    if(!guard||xSemaphoreTake(guard,pdMS_TO_TICKS(25))!=pdTRUE){
      errorResponse(503,"busy");return;
    }
    snap=latest;xSemaphoreGive(guard);
    if(seconds>0 && !snap.touchReady){errorResponse(409,"touch_unavailable");return;}
    cmd.job=Job::SetDisplayTimeout;cmd.displayTimeoutSec=uint16_t(seconds);
  }
  else {errorResponse(404,"unknown_action");return;}
  RadioState radio;
  if(!panel_radio::snapshot(radio)||!radio.ready){
    memset(cmd.password,0,sizeof(cmd.password));
    errorResponse(503,"radio_unavailable");return;
  }
  const bool accepted=panel_radio::submit(cmd);
  volatile char* secret=cmd.password;
  for(unsigned i=0;i<sizeof(cmd.password);i++)secret[i]=0;
  if(!accepted){errorResponse(409,"radio_busy");return;}
  jsonResponse(202,"{\"ok\":true,\"queued\":true}");
}
void logoutRequest() {
  if(!authorized()){errorResponse(401,"pair_required");return;}
  if(xSemaphoreTake(guard,pdMS_TO_TICKS(20))!=pdTRUE){errorResponse(503,"busy");return;}
  clearSessionLocked();
  xSemaphoreGive(guard);
  jsonResponse(200,"{\"ok\":true}");
}
void notFound() {
  responseHeaders();server.send(404,"text/plain; charset=utf-8","Не найдено");
}
void worker(void*) {
  const char* headers[]={"Authorization"};
  server.collectHeaders(headers,1);
  server.on("/",HTTP_GET,homepage);
  server.on("/health",HTTP_GET,[]{responseHeaders();server.send(200,"text/plain; charset=utf-8","ok");});
  server.on("/api/status",HTTP_GET,statusRequest);
  server.on("/api/pair",HTTP_POST,pairRequest);
  server.on("/api/command",HTTP_POST,commandRequest);
  server.on("/api/logout",HTTP_POST,logoutRequest);
  server.onNotFound(notFound);
  bool listening=false;
  for(;;){
    RadioState radio;
    const bool connected=panel_radio::snapshot(radio)&&radio.connected;
    if(connected){
      if(!listening){
        server.begin();listening=true;
        Serial.printf("[BP-WEB] listening http://%s/ \n",radio.ip);
      }
      server.handleClient();
    } else if(listening){
      server.stop();listening=false;
      Serial.println("[BP-WEB] paused (Wi-Fi disconnected)");
    }
    vTaskDelay(pdMS_TO_TICKS(30));
  }
}
#endif
} // namespace

bool begin() {
#if BP_WEB_ENABLED
  if(httpTask)return true;
  guard=xSemaphoreCreateMutex();
  if(!guard){Serial.println("[BP-WEB] mutex unavailable");return false;}
  if(xTaskCreate(worker,"boiler-http",16384,nullptr,1,&httpTask)!=pdPASS){
    vSemaphoreDelete(guard);guard=nullptr;
    Serial.println("[BP-WEB] worker start failed");return false;
  }
  Serial.println("[BP-WEB] worker ready; awaiting Wi-Fi IP");
  return true;
#else
  return false;
#endif
}
bool armPairing() {
  if(!guard || xSemaphoreTake(guard,pdMS_TO_TICKS(20))!=pdTRUE)return false;
  clearSessionLocked();
  clearPairingLocked();
  const uint32_t pin=100000+esp_random()%900000U;
  snprintf(physicalPin,sizeof(physicalPin),"%06lu",(unsigned long)pin);
  pinExpires=millis()+120000U;
  xSemaphoreGive(guard);
  return true;
}
bool pairingCode(char out[7],uint32_t& remaining) {
  if(!out)return false;
  out[0]=0;remaining=0;
  if(!guard||xSemaphoreTake(guard,0)!=pdTRUE)return false;
  const bool active=physicalPin[0]&&future(pinExpires);
  if(active){
    snprintf(out,7,"%s",physicalPin);
    remaining=(uint32_t(pinExpires-millis())+999U)/1000U;
  }
  xSemaphoreGive(guard);
  return active;
}
void revokePairing() {
  if(!guard||xSemaphoreTake(guard,pdMS_TO_TICKS(20))!=pdTRUE)return;
  clearPairingLocked();
  clearSessionLocked();
  xSemaphoreGive(guard);
}
void publish(const Measurement (&temps)[CHANNELS],const FuelLevel& fuel,uint32_t now) {
  if(!guard)return;
  Shared next;
  next.uptime=now/1000U;
  next.freeHeap=ESP.getFreeHeap();
  next.touchReady=panel_screen::touchReady();
  next.displayMs=panel_screen::boot.displayMs;
  next.lvglMs=panel_screen::boot.lvglMs;
  next.firstFrameMs=panel_screen::boot.firstFrameAt;
  next.touchMs=panel_screen::boot.touchMs;
  next.fuelStatus=uint8_t(fuel.status(now));
  next.lowerValid=fuel.lower.valid;
  next.lowerReached=fuel.lower.reached;
  next.lowerAge=fuel.lower.valid?uint32_t(now-fuel.lower.updated)/1000U:0;
  next.upperValid=fuel.upper.valid;
  next.upperReached=fuel.upper.reached;
  next.upperAge=fuel.upper.valid?uint32_t(now-fuel.upper.updated)/1000U:0;
  for(unsigned i=0;i<CHANNELS;i++){
    const Measurement& m=temps[i];
    TempSnap& t=next.temp[i];
    t.valid=m.fresh(now);
    t.ever=m.count>0;
    t.value=t.valid?m.value:0;
    t.minimum=t.ever?m.minimum:0;
    t.maximum=t.ever?m.maximum:0;
    t.trend=m.count>=2?float(m.delta()):0;
    t.age=m.count?uint32_t(now-m.updated)/1000U:0;
    t.count=m.count;
    for(unsigned k=0;k<m.count && k<HISTORY_POINTS;k++){
      t.history[k]=m.at(k);
      t.historyAge[k]=uint32_t(now-m.timeAt(k))/1000U;
    }
  }
  next.published=true;
  if(xSemaphoreTake(guard,0)==pdTRUE){latest=next;xSemaphoreGive(guard);}
}
} // namespace panel_web
