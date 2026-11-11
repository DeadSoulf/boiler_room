// Opt-in Home Assistant telemetry. No GPIO, actuator commands or wildcard subscriptions.
// Uses the ESP-MQTT already bundled with the pinned Arduino/IDF platform.
#include <Arduino.h>
#include <Preferences.h>
#include <sdkconfig.h>
#include <atomic>
#include <stddef.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "panel_ha.h"
#include "panel_radio.h"
#include "panel_storage.h"
#if defined(CONFIG_ESP_WIFI_REMOTE_ENABLED) && CONFIG_ESP_WIFI_REMOTE_ENABLED
#define BP_HA_SUPPORTED 1
#include <mqtt_client.h>
#include <esp_event.h>
#else
#define BP_HA_SUPPORTED 0
#endif

namespace panel_ha {
namespace {
SemaphoreHandle_t guard=nullptr;
QueueHandle_t readings=nullptr;
TaskHandle_t task=nullptr;
Config saved;
uint32_t revision=0;
unsigned activeSlot=0;
char deviceId[32]={};
std::atomic<bool> connected{false},announce{false},lost{false},subscribeBirth{false};
std::atomic<uint32_t> connections{0},discoveryMessages{0},lastPublish{0};
std::atomic<int> lastError{0},phase{0};
enum Phase { Disabled,AwaitingWifi,Connecting,Online,Failed };
struct Persist {uint32_t magic=0,schema=1,revision=0;Config config;uint32_t hash=0;};
constexpr uint32_t MAGIC=0x42504841U;
uint32_t checksum(const Persist& p){
  const uint8_t* b=reinterpret_cast<const uint8_t*>(&p);uint32_t h=2166136261U;
  for(size_t i=0;i<offsetof(Persist,hash);i++){h^=b[i];h*=16777619U;}return h;
}
bool readSlot(unsigned slot,Persist& p){
  Preferences prefs;if(!prefs.begin(slot?"bp_ha_b":"bp_ha_a",true))return false;
  const bool ok=prefs.getBytesLength("config")==sizeof(p)&&prefs.getBytes("config",&p,sizeof(p))==sizeof(p);
  prefs.end();
  return ok&&p.magic==MAGIC&&p.schema==1&&p.hash==checksum(p)&&validConfig(p.config);
}
bool writeSlot(unsigned slot,const Config& c,uint32_t nextRevision){
  Persist p{};p.magic=MAGIC;p.revision=nextRevision;p.config=c;p.hash=checksum(p);
  Preferences prefs;
  if(!prefs.begin(slot?"bp_ha_b":"bp_ha_a",false)){wipe(&p,sizeof(p));return false;}
  const bool written=prefs.putBytes("config",&p,sizeof(p))==sizeof(p);prefs.end();
  Persist check{};const bool ok=written&&readSlot(slot,check)&&check.revision==nextRevision&&sameConfig(check.config,c);
  wipe(&p,sizeof(p));wipe(&check,sizeof(check));return ok;
}
bool copyConfig(Config& out,uint32_t& rev){
  if(!guard||xSemaphoreTake(guard,pdMS_TO_TICKS(10))!=pdTRUE)return false;
  out=saved;rev=revision;xSemaphoreGive(guard);return true;
}
const char* phaseName(int p){
  switch(p){
    case AwaitingWifi:return "Ожидание Wi-Fi";
    case Connecting:return "Подключение к MQTT-брокеру";
    case Online:return "MQTT подключён; передача только показаний";
    case Failed:return "Ошибка MQTT; проверьте брокер, сеть и учётные данные";
    default:return "Интеграция выключена";
  }
}
#if BP_HA_SUPPORTED
esp_mqtt_client_handle_t client=nullptr; // Only the worker owns the handle.
char availabilityTopic[96]={},birthTopic[96]={},stateTopic[96]={};
void mqttEvent(void*,esp_event_base_t,int32_t id,void* data){
  auto* e=static_cast<esp_mqtt_event_t*>(data);
  switch(id){
    case MQTT_EVENT_CONNECTED:
      connected.store(true);connections.fetch_add(1);lastError.store(0);
      subscribeBirth.store(true);announce.store(true);break;
    case MQTT_EVENT_DISCONNECTED:
      connected.store(false);lost.store(true);break;
    case MQTT_EVENT_ERROR:
      // Numeric status only. Never copy SDK error strings, URI, username or password.
      lastError.store(e&&e->error_handle?int(e->error_handle->connect_return_code):-1);break;
    case MQTT_EVENT_DATA:
      // A single, exact HA birth topic is the ONLY accepted inbound message.
      // Fragmented or unexpected messages are ignored; they cannot become commands.
      if(e&&e->current_data_offset==0&&e->total_data_len==6&&e->data_len==6&&
         e->topic_len==int(strlen(birthTopic))&&e->topic&&e->data&&
         !memcmp(e->topic,birthTopic,e->topic_len)&&!memcmp(e->data,"online",6))announce.store(true);
      break;
    default:break;
  }
}
void stopClient(const Config& old,bool removeDiscovery){
  if(!client)return;
  if(connected.load()){
    // Best effort on the OLD broker/prefix. If it is unreachable, HA state expiry
    // still makes entities unavailable; old retained discovery may need manual removal.
    esp_mqtt_client_enqueue(client,availabilityTopic,"offline",0,1,1,true);
    if(removeDiscovery){
      char topic[192];
      for(unsigned i=0;i<ENTITY_COUNT;i++)if(discoveryTopic(topic,sizeof(topic),old,deviceId,i))
        esp_mqtt_client_enqueue(client,topic,"",0,1,1,true);
    }
    const uint32_t start=millis();
    while(connected.load()&&esp_mqtt_client_get_outbox_size(client)>0&&uint32_t(millis()-start)<1500U)
      vTaskDelay(pdMS_TO_TICKS(25));
  }
  // Stop/destroy are outside the SDK event handler and outside the application mutex.
  esp_mqtt_client_stop(client);esp_mqtt_client_destroy(client);client=nullptr;
  connected.store(false);lost.store(false);subscribeBirth.store(false);
}
bool startClient(const Config& c){
  snprintf(availabilityTopic,sizeof(availabilityTopic),"boiler_panel/%s/availability",deviceId);
  snprintf(stateTopic,sizeof(stateTopic),"boiler_panel/%s/state",deviceId);
  snprintf(birthTopic,sizeof(birthTopic),"%s/status",c.prefix);
  esp_mqtt_client_config_t options{};
  // MQTT/TCP without TLS: deliberate LAN-only first version. No URI with credentials.
  options.broker.address.hostname=c.host;
  options.broker.address.port=c.port;
  options.broker.address.transport=MQTT_TRANSPORT_OVER_TCP;
  options.credentials.client_id=deviceId;
  options.credentials.username=c.username[0]?c.username:nullptr;
  options.credentials.authentication.password=c.password[0]?c.password:nullptr;
  options.session.last_will.topic=availabilityTopic;
  options.session.last_will.msg="offline";
  options.session.last_will.qos=1;options.session.last_will.retain=1;
  options.session.keepalive=20;
  options.network.timeout_ms=2500;
  options.network.disable_auto_reconnect=true; // New client on reconnect: no stale outbox replay.
  options.task.stack_size=6144;
  options.buffer.size=2048;options.buffer.out_size=2048;
  options.outbox.limit=24576;
  lost.store(false);connected.store(false);announce.store(false);
  client=esp_mqtt_client_init(&options);
  if(!client)return false;
  if(esp_mqtt_client_register_event(client,MQTT_EVENT_ANY,mqttEvent,nullptr)!=ESP_OK||
     esp_mqtt_client_start(client)!=ESP_OK){
    esp_mqtt_client_destroy(client);client=nullptr;return false;
  }
  return true;
}
bool sendState(const Telemetry& telemetry,const panel::RadioState& radio){
  if(!client||!connected.load())return false;
  Diagnostics d;d.uptime=millis()/1000U;d.freeHeap=ESP.getFreeHeap();d.wifi=radio.connected;d.rssi=radio.rssi;
  panel_storage::Snapshot sd;
  d.sdKnown=panel_storage::snapshot(sd);d.sdMounted=d.sdKnown&&sd.mounted;d.sdFree=sd.freeBytes;
  char payload[2048];
  if(!statePayload(payload,sizeof(payload),telemetry,d,millis()))return false;
  // QoS 0 / NOT retained / NOT enqueued: stale measurements must never replay later.
  if(esp_mqtt_client_publish(client,stateTopic,payload,0,0,0)<0)return false;
  lastPublish.store(millis());return true;
}
void worker(void*){
  Config active;uint32_t applied=0xffffffffU,createdAt=0,retryAt=0,nextState=0,lastAnnounce=0;
  bool announcedOnce=false,discoveryPending=false;unsigned index=0;
  Telemetry telemetry;
  for(;;){
    if(readings)xQueueReceive(readings,&telemetry,0);
    Config next;uint32_t nextRevision;
    if(!copyConfig(next,nextRevision)){vTaskDelay(pdMS_TO_TICKS(100));continue;}
    if(nextRevision!=applied){
      const bool changedBroker=strcmp(active.host,next.host)||active.port!=next.port||strcmp(active.prefix,next.prefix);
      stopClient(active,!next.enabled||changedBroker);
      wipe(&active,sizeof(active));active=next;applied=nextRevision;
      discoveryPending=false;announcedOnce=false;retryAt=millis();lastError.store(0);
    }
    wipe(&next,sizeof(next));
    panel::RadioState radio;
    const bool wifi=panel_radio::snapshot(radio)&&radio.connected;
    uint32_t now=millis();
    if(!active.enabled||!wifi){
      stopClient(active,false);phase.store(active.enabled?AwaitingWifi:Disabled);
      discoveryPending=false;announcedOnce=false;retryAt=now;
      vTaskDelay(pdMS_TO_TICKS(200));continue;
    }
    if(client&&(lost.exchange(false)||(!connected.load()&&uint32_t(now-createdAt)>15000U))){
      stopClient(active,false);phase.store(Failed);retryAt=millis()+10000U;
      discoveryPending=false;announcedOnce=false;
    }
    if(!client&&int32_t(now-retryAt)>=0){
      phase.store(Connecting);createdAt=now;
      if(!startClient(active)){phase.store(Failed);retryAt=millis()+10000U;}
    }
    if(client&&connected.load()){
      phase.store(Online);
      if(subscribeBirth.exchange(false)&&esp_mqtt_client_subscribe_single(client,birthTopic,1)<0)
        subscribeBirth.store(true);
      if(announce.load()&&(!announcedOnce||uint32_t(now-lastAnnounce)>=10000U)){
        announce.store(false);discoveryPending=true;index=0;lastAnnounce=now;announcedOnce=true;
      }
      if(discoveryPending){
        if(index<ENTITY_COUNT){
          char topic[192],payload[2048];
          if(discoveryTopic(topic,sizeof(topic),active,deviceId,index)&&
             discovery(payload,sizeof(payload),deviceId,panel::VERSION,index)&&
             esp_mqtt_client_enqueue(client,topic,payload,0,1,1,true)>=0){
            ++index;discoveryMessages.fetch_add(1);
          }
        }else if(esp_mqtt_client_get_outbox_size(client)==0){
          if(esp_mqtt_client_enqueue(client,availabilityTopic,"online",0,1,1,true)>=0){
            discoveryPending=false;nextState=now;
          }
        }
      }
      if(!discoveryPending&&int32_t(now-nextState)>=0){
        if(!sendState(telemetry,radio))lastError.store(-1);
        nextState=millis()+PUBLISH_MS;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
#endif
} // namespace

bool begin(){
#if BP_HA_SUPPORTED
  if(task)return true;
  guard=xSemaphoreCreateMutex();readings=xQueueCreate(1,sizeof(Telemetry));
  if(!guard||!readings){if(guard)vSemaphoreDelete(guard);if(readings)vQueueDelete(readings);guard=nullptr;readings=nullptr;return false;}
  const uint64_t mac=ESP.getEfuseMac()&0xffffffffffffULL;
  if(mac==0||mac==0xffffffffffffULL){vSemaphoreDelete(guard);vQueueDelete(readings);guard=nullptr;readings=nullptr;return false;}
  snprintf(deviceId,sizeof(deviceId),"bp_%012llx",(unsigned long long)mac);
  Persist a{},b{};const bool hasA=readSlot(0,a),hasB=readSlot(1,b);
  if(hasA||hasB){
    activeSlot=hasB&&(!hasA||int32_t(b.revision-a.revision)>0)?1:0;
    const Persist& p=activeSlot?b:a;saved=p.config;revision=p.revision;
  }
  wipe(&a,sizeof(a));wipe(&b,sizeof(b));
  if(xTaskCreate(worker,"boiler-ha",12288,nullptr,1,&task)!=pdPASS){
    task=nullptr;vSemaphoreDelete(guard);vQueueDelete(readings);guard=nullptr;readings=nullptr;return false;
  }
  return true;
#else
  return false;
#endif
}
bool snapshot(Snapshot& out){
  out=Snapshot{};out.supported=BP_HA_SUPPORTED;
  if(!guard||xSemaphoreTake(guard,pdMS_TO_TICKS(10))!=pdTRUE)return false;
  out.ready=task!=nullptr;out.enabled=saved.enabled;out.port=saved.port;out.revision=revision;out.passwordSet=saved.password[0]!=0;
  snprintf(out.host,sizeof(out.host),"%s",saved.host);snprintf(out.username,sizeof(out.username),"%s",saved.username);
  snprintf(out.prefix,sizeof(out.prefix),"%s",saved.prefix);snprintf(out.id,sizeof(out.id),"%s",deviceId);
  xSemaphoreGive(guard);
  out.connected=connected.load();out.connections=connections.load();out.discoveryMessages=discoveryMessages.load();
  out.lastPublishMs=lastPublish.load();out.errorCode=lastError.load();
  snprintf(out.message,sizeof(out.message),"%s",phaseName(phase.load()));return true;
}
SaveResult configure(const Config& request,PasswordMode mode,uint32_t expectedRevision){
  if(!task||!guard)return SaveResult::Unavailable;
  if(!validConfig(request)||unsigned(mode)>unsigned(PasswordMode::Clear))return SaveResult::Invalid;
  if(mode==PasswordMode::Set&&!request.password[0])return SaveResult::Invalid;
  if(xSemaphoreTake(guard,pdMS_TO_TICKS(50))!=pdTRUE)return SaveResult::Unavailable;
  if(expectedRevision!=revision){xSemaphoreGive(guard);return SaveResult::Conflict;}
  // Never forward an existing secret to a newly entered host/account by accident.
  if(mode==PasswordMode::Keep&&saved.password[0]&&
     (strcmp(saved.host,request.host)||saved.port!=request.port||strcmp(saved.username,request.username))){
    xSemaphoreGive(guard);return SaveResult::Invalid;
  }
  Config candidate{};candidate.enabled=request.enabled;candidate.port=request.port;
  snprintf(candidate.host,sizeof(candidate.host),"%s",request.host);
  snprintf(candidate.username,sizeof(candidate.username),"%s",request.username);
  snprintf(candidate.prefix,sizeof(candidate.prefix),"%s",request.prefix);
  if(mode!=PasswordMode::Clear)snprintf(candidate.password,sizeof(candidate.password),"%s",mode==PasswordMode::Keep?saved.password:request.password);
  if(sameConfig(saved,candidate)){wipe(&candidate,sizeof(candidate));xSemaphoreGive(guard);return SaveResult::Saved;}
  const unsigned target=activeSlot^1U;
  const bool ok=writeSlot(target,candidate,revision+1);
  if(ok){wipe(&saved,sizeof(saved));saved=candidate;++revision;activeSlot=target;}
  wipe(&candidate,sizeof(candidate));xSemaphoreGive(guard);
  return ok?SaveResult::Saved:SaveResult::StorageError;
}
bool requestDiscovery(){
  if(!guard||xSemaphoreTake(guard,pdMS_TO_TICKS(10))!=pdTRUE)return false;
  const bool enabled=task&&saved.enabled;xSemaphoreGive(guard);
  if(enabled){announce.store(true);}
  return enabled;
}
void publish(const panel::Measurement (&temps)[panel::CHANNELS],const panel::FuelLevel& fuel,uint32_t now){
  if(!readings)return;
  Telemetry t;t.received=true;t.captured=now;
  // Propagate sticky model expiration, not historical sample validity.
  for(unsigned channel=0;channel<panel_config::COUNT;channel++){
    const int index=panel_config::temperatureIndex(channel);
    if(index<0)continue;
    const auto& measurement=temps[index];
    t.input[channel].valid=measurement.fresh(now);
    t.input[channel].temperature=measurement.value;
    t.input[channel].updated=measurement.updated;
  }
  t.input[4].valid=fuel.lower.fresh(now);t.input[4].reached=fuel.lower.reached;t.input[4].updated=fuel.lower.updated;
  t.input[5].valid=fuel.upper.fresh(now);t.input[5].reached=fuel.upper.reached;t.input[5].updated=fuel.upper.updated;
  xQueueOverwrite(readings,&t);
}
} // namespace panel_ha
