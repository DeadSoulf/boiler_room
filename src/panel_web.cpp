// JC4880P443 ESP32-P4 / Arduino 3.3.6. This is an on-device web server.
// A code displayed only on the physical panel is required before commands.
// HTTP is NOT encrypted: local trusted LAN only, never port forward.
#include <Arduino.h>
#include <sdkconfig.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <time.h>
#include <string.h>
#include "panel_web.h"
#include "panel_pairing.h"
#include "panel_ha.h"
#include "panel_radio.h"
#include "panel_equipment.h"
#include "panel_boiler.h"
#include "panel_screen.h"
#include "panel_storage.h"
#include "panel_update.h"
#include "panel_config.h"
#include "panel_sensors.h"
#include "panel_pin_profile.h"
#if defined(CONFIG_ESP_WIFI_REMOTE_ENABLED) && CONFIG_ESP_WIFI_REMOTE_ENABLED
#define BP_WEB_ENABLED 1
#include <WebServer.h>
#include <WiFi.h>
#include "panel_web_page.h"
#include "panel_web_pinout.h"
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
panel_pairing::State pairing;
char sessionToken[33]="";
// Use monotonic 64-bit uptime: a bearer must not revive after millis() wraps.
int64_t sessionExpiresUs=0;
void clearPairingLocked() { pairing.revoke(); }

void clearSessionLocked() {
  memset(sessionToken,0,sizeof(sessionToken));sessionExpiresUs=0;
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
void pinoutImage() {
  responseHeaders();server.send_P(200,"image/jpeg",reinterpret_cast<const char*>(kJp1PinoutJpeg),sizeof(kJp1PinoutJpeg));
}
bool authorized() {
  const String h=server.header("Authorization");
  if(!h.startsWith("Bearer ")||h.length()!=39||!guard)return false;
  if(xSemaphoreTake(guard,pdMS_TO_TICKS(30))!=pdTRUE)return false;
  const bool active=sessionToken[0] && esp_timer_get_time()<sessionExpiresUs;
  unsigned char diff=0;
  for(unsigned i=0;i<32;i++)diff|=uint8_t(h[7+i])^uint8_t(sessionToken[i]);
  const bool result=active && diff==0;
  xSemaphoreGive(guard);
  return result;
}
bool pairingOriginAllowed() {
  const String base=String("http://")+WiFi.localIP().toString();
  const String withPort=base+":80";
  return panel_pairing::localRequest(server.header("X-Boiler-Pairing").c_str(),
    server.header("Origin").c_str(),base.c_str(),withPort.c_str(),server.header("Sec-Fetch-Site").c_str());
}
void pairStartRequest() {
  if(!pairingOriginAllowed()){errorResponse(403,"same_origin_required");return;}
  const String ticket=server.arg("request_id");
  if(ticket.length()!=32||!panel_pairing::ticket32(ticket.c_str())){errorResponse(400,"invalid_request_id");return;}
  if(!guard||xSemaphoreTake(guard,pdMS_TO_TICKS(30))!=pdTRUE){errorResponse(503,"busy");return;}
  if(!latest.published||!latest.firstFrameMs){xSemaphoreGive(guard);errorResponse(503,"display_not_ready");return;}
  char candidate[7];snprintf(candidate,sizeof(candidate),"%06lu",(unsigned long)(100000U+esp_random()%900000U));
  const auto result=pairing.request(ticket.c_str(),candidate,millis());
  panel_pairing::wipe(candidate,sizeof(candidate));
  const uint32_t seconds=pairing.remaining(millis()),retry=pairing.retryAfter(millis());
  xSemaphoreGive(guard);
  if(result==panel_pairing::Request::Busy){errorResponse(409,"pairing_in_progress");return;}
  if(result==panel_pairing::Request::Cooldown){server.sendHeader("Retry-After",String(retry));errorResponse(429,"pairing_rate_limited");return;}
  if(result==panel_pairing::Request::Invalid){errorResponse(400,"invalid_request");return;}
  // No code in response, logs, status JSON or MQTT. Starting does not revoke a session.
  String j="{\"ok\":true,\"display_only\":true,\"expires_in_s\":";j+=String(seconds);j+='}';jsonResponse(202,j);
}
void pairCancelRequest() {
  if(!pairingOriginAllowed()){errorResponse(403,"same_origin_required");return;}
  const String ticket=server.arg("request_id");
  if(!guard||xSemaphoreTake(guard,pdMS_TO_TICKS(30))!=pdTRUE){errorResponse(503,"busy");return;}
  const bool cancelled=pairing.cancel(ticket.c_str(),millis());xSemaphoreGive(guard);
  jsonResponse(200,cancelled?"{\"ok\":true,\"cancelled\":true}":"{\"ok\":true,\"cancelled\":false}");
}
void pairRequest() {
  if(!pairingOriginAllowed()){errorResponse(403,"same_origin_required");return;}
  const String entered=server.arg("code"),ticket=server.arg("request_id");
  if(!allDigits(entered,6,6)||!panel_pairing::ticket32(ticket.c_str())){errorResponse(400,"invalid_code");return;}
  if(!guard||xSemaphoreTake(guard,pdMS_TO_TICKS(30))!=pdTRUE){errorResponse(503,"busy");return;}
  const auto result=pairing.verify(ticket.c_str(),entered.c_str(),millis());
  if(result!=panel_pairing::Verify::Accepted){
    xSemaphoreGive(guard);errorResponse(result==panel_pairing::Verify::Locked?429:403,
      result==panel_pairing::Verify::Locked?"pairing_rate_limited":"code_invalid_or_expired");return;
  }
  static const char HEX_CHARS[]="0123456789abcdef";
  uint8_t secret[16];esp_fill_random(secret,sizeof(secret));
  for(unsigned i=0;i<sizeof(secret);i++){sessionToken[i*2]=HEX_CHARS[secret[i]>>4];sessionToken[i*2+1]=HEX_CHARS[secret[i]&15];}
  sessionToken[32]=0;panel_pairing::wipe(secret,sizeof(secret));
  sessionExpiresUs=esp_timer_get_time()+3600000000LL;
  const String token(sessionToken);
  xSemaphoreGive(guard);
  String j="{\"ok\":true,\"token\":";jsonQuote(j,token.c_str());j+=",\"expires_in_s\":3600}";
  jsonResponse(200,j); // The accepted challenge was already erased; LVGL hides it next tick.
}

void statusRequest() {
  Shared snap;
  if(!guard || xSemaphoreTake(guard,pdMS_TO_TICKS(30))!=pdTRUE){errorResponse(503,"busy");return;}
  snap=latest;
  const bool pairingActive=pairing.active(millis());
  const uint32_t pairingRemaining=pairing.remaining(millis());
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
  j+=",\"pair_remaining_s\":";j+=String(pairingRemaining);
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
    j+=",\"saved\":";j+=radio.aps[i].saved?"true":"false";
    j+='}';
  }
  j+="],\"savedNetworks\":[";
  if(admin)for(unsigned i=0;i<radio.savedCount;i++){
    if(i)j+=',';
    j+="{\"ssid\":";jsonQuote(j,radio.savedNetworks[i].ssid);
    j+=",\"open\":";j+=radio.savedNetworks[i].open?"true":"false";j+='}';
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
  for(const auto target : {panel_boiler::Target::Dhw,panel_boiler::Target::Electric}){
    j+=target==panel_boiler::Target::Dhw?",\"boiler_settings\":":",\"electric_settings\":";
    panel_boiler::Settings limits;
    if(!panel_boiler::snapshot(limits,target)){j+="null";continue;}
    j+="{\"revision\":";j+=String(limits.revision);
    j+=",\"configured\":";j+=limits.configured?"true":"false";
    if(limits.configured){
      j+=",\"on_c\":";j+=String(unsigned(limits.onC));j+=",\"off_c\":";j+=String(unsigned(limits.offC));
    }else j+=",\"on_c\":null,\"off_c\":null";
    j+='}';
  }
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
  j+=",\"format_state\":";j+=String(unsigned(storage.formatPhase));
  j+=",\"format_attempted\":";j+=storage.formatAttempted?"true":"false";
  j+=",\"format_remaining_s\":";j+=String(storage.formatRemainingSeconds);
  j+=",\"format_message\":";jsonQuote(j,storage.formatMessage);
  j+=",\"message\":";jsonQuote(j,storage.message);j+='}';
  panel_config::State cfg;
  const bool hasConfig=panel_config::snapshot(cfg);
  panel_sensors::Snapshot sensors;
  const bool hasSensors=panel_sensors::snapshot(sensors);
  bool anyInputRunning=false;
  if(hasSensors)for(const auto& c:sensors.channels)if(c.active)anyInputRunning=true;
  j+=",\"connections\":{\"revision\":";
  j+=String(hasConfig?cfg.revision:0);
  j+=",\"available\":";j+=hasConfig?"true":"false";
  j+=",\"drivers_active\":";j+=anyInputRunning?"true":"false";
  j+=",\"message\":";
  jsonQuote(j,"JP1 sensor drivers activate only on validated NVS assignments; other controllers remain offline");
  j+=",\"items\":[";
  const panel_sensors::ChannelState inactive{};
  for(unsigned i=0;i<panel_config::COUNT;i++){
    if(i)j+=',';
    const auto& a=cfg.item[i];
    j+="{\"channel\":";j+=String(i);
    j+=",\"type\":";jsonQuote(j,panel_config::typeName(a.type));
    j+=",\"gpio\":";j+=String(a.gpio);
    j+=",\"address\":";j+=String(a.busAddress);
    j+=",\"debounce_ms\":";j+=String(a.debounceMs);
    const bool input=i<panel_config::INPUT_COUNT;
    const auto& st=input?sensors.channels[i]:inactive;
    const bool fresh=input && hasConfig && hasSensors && sensors.configRevision==cfg.revision;
    j+=",\"active\":";j+=(fresh&&st.active)?"true":"false";
    j+=",\"online\":";j+=(fresh&&st.valid)?"true":"false";
    j+=",\"jp1_pin\":";j+=String(panel_pins::connectorPin(a.gpio));
    j+=",\"last_seen_ms\":";j+=String(fresh?st.lastGoodMs:0);
    j+=",\"errors\":";j+=String(fresh?st.errors:0);
    j+=",\"found\":";j+=String(fresh?st.found:0);
    j+=",\"rom\":";jsonQuote(j,fresh?st.rom:"");
    j+=",\"status\":";if(input)jsonQuote(j,fresh?st.status:"Awaiting sensor worker");
    else jsonQuote(j,a.type==panel_config::Type::Relay?"Relay GPIO reserved; output inactive":"Not assigned");
    j+='}';
  }
  j+="]}";
  panel_ha::Snapshot ha;
  panel_ha::snapshot(ha);
  j+=",\"home_assistant\":{\"ready\":";j+=ha.ready?"true":"false";
  j+=",\"supported\":";j+=ha.supported?"true":"false";
  j+=",\"enabled\":";j+=ha.enabled?"true":"false";
  j+=",\"connected\":";j+=ha.connected?"true":"false";
  j+=",\"revision\":";j+=String(ha.revision);
  j+=",\"id\":";jsonQuote(j,ha.id);
  j+=",\"message\":";jsonQuote(j,ha.message);
  j+=",\"error_code\":";j+=String(ha.errorCode);
  j+=",\"connections\":";j+=String(ha.connections);
  j+=",\"discovery_messages\":";j+=String(ha.discoveryMessages);
  j+=",\"last_publish_ms\":";j+=String(ha.lastPublishMs);
  j+=",\"host\":";jsonQuote(j,admin?ha.host:"");
  j+=",\"port\":";j+=String(admin?ha.port:0);
  j+=",\"username\":";jsonQuote(j,admin?ha.username:"");
  j+=",\"discovery_prefix\":";jsonQuote(j,admin?ha.prefix:"");
  j+=",\"password_set\":";j+=(admin&&ha.passwordSet)?"true":"false";
  j+=",\"transport\":\"mqtt_tcp_lan_only\",\"read_only\":true}";
  panel_update::Snapshot update;
  panel_update::snapshot(update);
  j+=",\"update\":{\"started\":";j+=update.started?"true":"false";
  j+=",\"supported\":";j+=update.supported?"true":"false";
  j+=",\"busy\":";j+=update.busy?"true":"false";
  j+=",\"available\":";j+=update.available?"true":"false";
  j+=",\"cancel_allowed\":";j+=update.cancelAllowed?"true":"false";
  j+=",\"rollback_available\":";j+=update.rollbackAvailable?"true":"false";
  j+=",\"boot_pending\":";j+=update.bootPending?"true":"false";
  j+=",\"automatic_rollback\":";j+=update.automaticRollback?"true":"false";
  j+=",\"phase\":";jsonQuote(j,panel_update::phaseName(update.phase));
  j+=",\"done\":";j+=String(update.done);
  j+=",\"total\":";j+=String(update.total);
  j+=",\"percent\":";j+=String(unsigned(update.percent));
  j+=",\"last_check\":";j+=String(update.lastCheck);
  j+=",\"latest_version\":";jsonQuote(j,update.latestVersion);
  j+=",\"previous_version\":";jsonQuote(j,update.previousVersion);
  j+=",\"message\":";jsonQuote(j,update.message);
  j+=",\"error_code\":";j+=String(update.errorCode);j+='}';
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
  if(!pairingOriginAllowed()){errorResponse(403,"same_origin_required");return;}
  const String kind=server.arg("action");
  panel_update::Snapshot update;
  if(!panel_update::snapshot(update)){errorResponse(503,"update_state_unavailable");return;}
  if(kind=="update_check"||kind=="update_install"||kind=="update_cancel"||kind=="update_rollback"){
    if(!update.started||!update.supported){errorResponse(503,"update_unavailable");return;}
    if(kind=="update_cancel"){
      if(!update.busy||!update.cancelAllowed||!panel_update::requestCancel()){
        errorResponse(409,"update_cancel_unavailable");return;
      }
    }else{
      if(update.busy||update.bootPending){errorResponse(409,"update_busy");return;}
      if(kind=="update_check"){
        if(!panel_update::requestCheck()){errorResponse(409,"update_not_ready");return;}
      }else{
        const String version=server.arg("version"),confirmation=server.arg("confirm");
        if(version.isEmpty()||version.length()>=sizeof(update.latestVersion)||version.length()!=strlen(version.c_str())||
           confirmation!=version){errorResponse(400,"update_confirmation_required");return;}
        const bool rollback=kind=="update_rollback";
        const char* expected=rollback?update.previousVersion:update.latestVersion;
        if((rollback?!update.rollbackAvailable:!update.available)||version!=expected){
          errorResponse(409,"update_version_changed");return;
        }
        if(!(rollback?panel_update::requestRollback(version.c_str()):panel_update::requestInstall(version.c_str()))){
          errorResponse(409,"update_not_ready");return;
        }
      }
    }
    jsonResponse(202,"{\"ok\":true,\"queued\":true}");return;
  }
  // Read/status and pairing stay available. All other writes wait for OTA to finish.
  if(update.busy||update.bootPending){errorResponse(409,"update_busy");return;}
  if(kind=="ha_discover"){
    if(!panel_ha::requestDiscovery()){errorResponse(409,"ha_disabled_or_unavailable");return;}
    jsonResponse(202,"{\"ok\":true,\"queued\":true}");return;
  }
  if(kind=="ha_save"){
    const String host=server.arg("host"),user=server.arg("username"),prefix=server.arg("prefix"),
      port=server.arg("port"),enabled=server.arg("enabled"),mode=server.arg("password_mode"),rev=server.arg("revision");
    String secret=server.arg("password");
    if(host.length()>128||user.length()>64||prefix.length()>64||secret.length()>128||
       host.length()!=strlen(host.c_str())||user.length()!=strlen(user.c_str())||
       prefix.length()!=strlen(prefix.c_str())||secret.length()!=strlen(secret.c_str())||
       !allDigits(port,1,5)||asUint(port)==0||asUint(port)>65535||
       !allDigits(rev,1,10)||strtoull(rev.c_str(),nullptr,10)>0xffffffffULL||
       (enabled!="0"&&enabled!="1")||(mode!="keep"&&mode!="set"&&mode!="clear")||
       (enabled=="1"&&server.arg("lan_confirm")!="1")){
      errorResponse(400,"ha_invalid_fields_or_lan_confirmation");return;
    }
    panel_ha::Config request;
    snprintf(request.host,sizeof(request.host),"%s",host.c_str());
    snprintf(request.username,sizeof(request.username),"%s",user.c_str());
    snprintf(request.prefix,sizeof(request.prefix),"%s",prefix.c_str());
    snprintf(request.password,sizeof(request.password),"%s",secret.c_str());
    request.enabled=enabled=="1";request.port=uint16_t(asUint(port));
    const auto passwordMode=mode=="set"?panel_ha::PasswordMode::Set:
        mode=="clear"?panel_ha::PasswordMode::Clear:panel_ha::PasswordMode::Keep;
    const auto result=panel_ha::configure(request,passwordMode,uint32_t(strtoull(rev.c_str(),nullptr,10)));
    panel_ha::wipe(&request,sizeof(request));
    for(unsigned i=0;i<secret.length();i++)secret.setCharAt(i,0);
    if(result==panel_ha::SaveResult::Invalid){errorResponse(400,"ha_invalid_or_password_required_for_new_broker");return;}
    if(result==panel_ha::SaveResult::Conflict){errorResponse(409,"ha_configuration_changed_reload");return;}
    if(result==panel_ha::SaveResult::StorageError){errorResponse(500,"ha_nvs_write_failed");return;}
    if(result!=panel_ha::SaveResult::Saved){errorResponse(503,"ha_unavailable");return;}
    jsonResponse(200,"{\"ok\":true,\"saved\":true}");return;
  }

  if(kind=="boiler_thresholds" || kind=="electric_thresholds"){
    const bool electric=kind=="electric_thresholds";
    const auto target=electric?panel_boiler::Target::Electric:panel_boiler::Target::Dhw;
    const char* invalid=electric?"invalid_electric_thresholds":"invalid_boiler_thresholds";
    if(!update.started){errorResponse(503,"update_state_unavailable");return;}
    const String on=server.arg("on_c"),off=server.arg("off_c"),revision=server.arg("revision");
    if(!allDigits(on,1,2)||!allDigits(off,1,2)||asUint(on)>=asUint(off)||
       !allDigits(revision,1,10)||strtoull(revision.c_str(),nullptr,10)>0xffffffffULL){
      errorResponse(400,invalid);return;
    }
    const auto result=panel_boiler::save(asUint(revision),int(asUint(on)),int(asUint(off)),target);
    if(result==panel_boiler::Result::Invalid){errorResponse(400,invalid);return;}
    if(result==panel_boiler::Result::Conflict){errorResponse(409,"stale_revision");return;}
    if(result!=panel_boiler::Result::Saved){errorResponse(503,electric?"electric_settings_unavailable":"boiler_settings_unavailable");return;}
    panel_boiler::Settings confirmed;
    if(!panel_boiler::snapshot(confirmed,target)||!confirmed.configured||
       unsigned(confirmed.onC)!=asUint(on)||unsigned(confirmed.offC)!=asUint(off)){
      errorResponse(409,electric?"electric_save_unconfirmed":"boiler_save_unconfirmed");return;
    }
    jsonResponse(200,String("{\"ok\":true,\"saved\":true,\"revision\":")+String(confirmed.revision)+"}");return;
  }
  if(kind=="sd_format_prepare"){
    // Strict exact phrase from browser + existing physical pairing.
    // This ONLY arms a temporary request; no SD API may be called here.
    if(server.arg("confirm")!="СТЕРЕТЬ MICROSD"){
      errorResponse(400,"type_ERASE_MICROSD");return;
    }
    if(!panel_storage::prepareFormat()){
      errorResponse(409,"sd_format_busy_or_already_attempted");return;
    }
    jsonResponse(202,"{\"ok\":true,\"status\":\"awaiting_physical_confirmation\",\"expires_in_s\":90}");
    return;
  }
  if(kind=="connection_pin_set"||kind=="connection_pin_clear"){
    const String pinValue=server.arg("gpio"),revision=server.arg("revision");
    if(!allDigits(pinValue,1,2)||!allDigits(revision,1,10)||
       strtoull(revision.c_str(),nullptr,10)>0xffffffffULL){
      errorResponse(400,"invalid_pin_fields");return;
    }
    const auto expectedRevision=uint32_t(strtoull(revision.c_str(),nullptr,10));
    const int16_t gpio=int16_t(asUint(pinValue));
    uint32_t savedRevision=0;
    panel_config::PinResult result;
    if(kind=="connection_pin_set"){
      const String channel=server.arg("channel"),type=server.arg("type"),debounce=server.arg("debounce");
      if(!allDigits(channel,1,1)||asUint(channel)>=panel_config::COUNT||
         !allDigits(type,1,1)||!panel_config::isLocal(static_cast<panel_config::Type>(asUint(type)))||
         !allDigits(debounce,1,4)||asUint(debounce)>5000){
        errorResponse(400,"invalid_pin_fields");return;
      }
      result=panel_config::setPin(gpio,static_cast<panel_config::Channel>(asUint(channel)),
        static_cast<panel_config::Type>(asUint(type)),uint16_t(asUint(debounce)),expectedRevision,savedRevision);
    }else result=panel_config::clearPin(gpio,expectedRevision,savedRevision);
    if(result==panel_config::PinResult::Invalid){errorResponse(400,"invalid_pin_assignment");return;}
    if(result==panel_config::PinResult::Conflict){errorResponse(409,"stale_revision");return;}
    if(result==panel_config::PinResult::StorageError){errorResponse(503,"configuration_write_failed");return;}
    if(result!=panel_config::PinResult::Saved){errorResponse(503,"configuration_unavailable");return;}
    jsonResponse(200,String("{\"ok\":true,\"saved\":true,\"revision\":")+String(savedRevision)+"}");return;
  }
  if(kind=="connection_rollback"){
    if(!panel_config::rollback()){
      errorResponse(409,"no_valid_previous_configuration");return;
    }
    jsonResponse(200,"{\"ok\":true,\"restored_previous\":true}");
    return;
  }
  if(kind=="connection_set"){
    const String channel=server.arg("channel"),kindValue=server.arg("type"),
      pinValue=server.arg("gpio"),addrValue=server.arg("address"),debValue=server.arg("debounce");
    if(!allDigits(channel,1,1)||channel.toInt()>=int(panel_config::COUNT)||
       !allDigits(kindValue,1,1)||kindValue.toInt()>int(panel_config::Type::Relay) ||
       pinValue.length()>3||(!pinValue.startsWith("-")&&!allDigits(pinValue,1,2))||
       !allDigits(addrValue,1,3)||!allDigits(debValue,1,4)){
      errorResponse(400,"invalid_connection_fields");return;
    }
    const int pin=pinValue.toInt();
    if(pin<-1||pin>54||addrValue.toInt()>247||debValue.toInt()>5000){
      errorResponse(400,"invalid_connection_range");return;
    }
    if(!panel_config::set(static_cast<panel_config::Channel>(channel.toInt()),
         static_cast<panel_config::Type>(kindValue.toInt()),pin,
         uint8_t(addrValue.toInt()),uint16_t(debValue.toInt()))){
      errorResponse(409,"connection_conflict_or_nvs_error");return;
    }
    jsonResponse(202,"{\"ok\":true,\"saved\":true,\"drivers_active\":false}");
    return;
  }
  panel::Command cmd;
  if(kind=="wifi_scan")cmd.job=Job::WifiScan;
  else if(kind=="wifi_disconnect")cmd.job=Job::Disconnect;
  else if(kind=="ble_scan")cmd.job=Job::BleScan;
  else if(kind=="wifi_connect"||kind=="wifi_forget"){
    const String ssid=server.arg("ssid"),open=server.arg("open"),saved=server.arg("saved");
    const bool forget=kind=="wifi_forget";
    // Older clients submitted an unqualified Forget. Bind it to the only profile
    // in the snapshot below; an empty or ambiguous target never clears the book.
    const bool legacyForget=forget&&!server.hasArg("ssid")&&!server.hasArg("open");
    if((!legacyForget&&(ssid.isEmpty()||ssid.length()>32||ssid.length()!=strlen(ssid.c_str())||
        (open!="0"&&open!="1")||open.length()!=1))||
       (server.hasArg("saved")&&(forget||saved!="1"||saved.length()!=1))){
      errorResponse(400,"invalid_network_credentials");return;
    }
    cmd.job=forget?Job::Forget:Job::Connect;cmd.useSaved=!forget&&saved=="1";cmd.open=open=="1";
    snprintf(cmd.ssid,sizeof(cmd.ssid),"%s",ssid.c_str());
    if(forget||cmd.useSaved){
      if(server.hasArg("password")){errorResponse(400,"invalid_network_credentials");return;}
    }else{
      const String pass=server.arg("password");
      if(pass.length()>64||pass.length()!=strlen(pass.c_str())||!passwordValid(pass.c_str(),cmd.open)){
        errorResponse(400,"invalid_network_credentials");return;
      }
      snprintf(cmd.password,sizeof(cmd.password),"%s",pass.c_str());
    }
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
  if(cmd.useSaved||cmd.job==Job::Forget){
    if(!cmd.ssid[0]&&radio.savedCount==1){
      const auto& only=radio.savedNetworks[0];
      snprintf(cmd.ssid,sizeof(cmd.ssid),"%s",only.ssid);cmd.open=only.open;
    }
    bool found=false;
    for(unsigned i=0;i<radio.savedCount;i++){
      const auto& net=radio.savedNetworks[i];
      if(net.open==cmd.open&&!strcmp(net.ssid,cmd.ssid)){found=true;break;}
    }
    if(!found){errorResponse(409,"wifi_profile_changed");return;}
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
  const char* headers[]={"Authorization","X-Boiler-Pairing","Origin","Sec-Fetch-Site"};
  server.collectHeaders(headers,4);
  server.on("/",HTTP_GET,homepage);
  server.on("/assets/jp1-pinout.jpg",HTTP_GET,pinoutImage);
  server.on("/health",HTTP_GET,[]{responseHeaders();server.send(200,"text/plain; charset=utf-8","ok");});
  server.on("/api/status",HTTP_GET,statusRequest);
  server.on("/api/pair/start",HTTP_POST,pairStartRequest);
  server.on("/api/pair/cancel",HTTP_POST,pairCancelRequest);
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
bool pairingCode(char out[7],uint32_t& remaining,uint32_t* generation) {
  if(!out)return false;
  out[0]=0;remaining=0;if(generation)*generation=0;
  if(!guard||xSemaphoreTake(guard,pdMS_TO_TICKS(10))!=pdTRUE)return false;
  uint32_t current=0;const bool active=pairing.display(out,remaining,current,millis());
  if(generation)*generation=current;
  xSemaphoreGive(guard);return active;
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
  next.lowerValid=fuel.lower.fresh(now);
  next.lowerReached=fuel.lower.reached;
  next.lowerAge=fuel.lower.valid?uint32_t(now-fuel.lower.updated)/1000U:0;
  next.upperValid=fuel.upper.fresh(now);
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
