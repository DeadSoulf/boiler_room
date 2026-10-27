// ESP32-P4 / Arduino-ESP32 3.3.6. All radio/NVS operations run outside LVGL.
// No automatic C6 firmware updates. No passwords in Serial or UI snapshots.
#include <Arduino.h>
#include <Preferences.h>
#include <sdkconfig.h>
#include <soc/soc_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <sys/time.h>
#include <stdlib.h>
#include <algorithm>
#include "panel_radio.h"

#if defined(CONFIG_ESP_WIFI_REMOTE_ENABLED) && CONFIG_ESP_WIFI_REMOTE_ENABLED
#define PANEL_WIFI 1
#include <WiFi.h>
#include <esp32-hal-hosted.h>
#else
#define PANEL_WIFI 0
#endif
#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE) && CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE && defined(CONFIG_NIMBLE_ENABLED) && CONFIG_NIMBLE_ENABLED && defined(CONFIG_BT_NIMBLE_ROLE_OBSERVER) && CONFIG_BT_NIMBLE_ROLE_OBSERVER
#define PANEL_BLE 1
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <esp32-hal-hosted.h>
#else
#define PANEL_BLE 0
#endif

namespace panel_radio {
using namespace panel;
namespace {
QueueHandle_t commands=nullptr;
SemaphoreHandle_t stateMutex=nullptr;
RadioState state;
Preferences prefs;
bool nvsOK=false;
bool pinsReady=false;
struct Credentials { uint32_t magic=0x42503033; char ssid[33]={}; char password[65]={}; uint8_t open=0; };
Credentials saved;
bool haveCredentials=false;
const char* const ZONES[] = {"UTC0", "MSK-3", "CET-1CEST,M3.5.0/2,M10.5.0/3"};
const char* const ZONE_NAMES[] = {"UTC", "Москва UTC+3", "Берлин CET/CEST"};
void lock() { xSemaphoreTake(stateMutex, portMAX_DELAY); }
void unlock() { ++state.revision; xSemaphoreGive(stateMutex); }
void wipe(void* ptr, size_t size) { volatile unsigned char* p=static_cast<volatile unsigned char*>(ptr); while(size--) *p++=0; }
void message(const char* text, bool ble=false) {
  lock(); snprintf(ble?state.bleMessage:state.wifiMessage,150,"%s",text); unlock();
}
void jobState(Job job, bool busy) { lock();state.job=job;state.busy=busy;unlock(); }
void versions() {
#if PANEL_WIFI || PANEL_BLE
  uint32_t a=0,b=0,c=0;
  hostedGetHostVersion(&a,&b,&c);
  lock();snprintf(state.hostVersion,sizeof(state.hostVersion),"%lu.%lu.%lu",(unsigned long)a,(unsigned long)b,(unsigned long)c);unlock();
  hostedGetSlaveVersion(&a,&b,&c);
  lock();
  if(a || b || c) snprintf(state.c6Version,sizeof(state.c6Version),"%lu.%lu.%lu",(unsigned long)a,(unsigned long)b,(unsigned long)c);
  else snprintf(state.c6Version,sizeof(state.c6Version),"Не прочитана");
  unlock();
#endif
}
bool configurePins() {
#if PANEL_WIFI || PANEL_BLE
  if(pinsReady) return true;
  // Guition JC4880P443: internal C6 SDIO, not external sensor GPIOs.
  pinsReady=hostedSetPins(18,19,14,15,16,17,54);
  return pinsReady;
#else
  return false;
#endif
}
#if PANEL_WIFI
bool wifiStarted=false;
bool startWifi() {
  if(wifiStarted) return true;
  if(!configurePins()) { message("Ошибка настройки связи с C6");return false; }
  WiFi.persistent(false);
  WiFi.setHostname("boiler-panel");
  if(!WiFi.mode(WIFI_STA)) { message("Wi-Fi не запущен. Проверьте C6 в Сервисе");return false; }
  WiFi.setSleep(false);
  WiFi.setScanTimeout(15000);
  wifiStarted=true; versions();return true;
}
void updateConnection() {
  if(!wifiStarted) return;
  const bool connected=WiFi.status()==WL_CONNECTED && uint32_t(WiFi.localIP())!=0;
  const String ssid=connected?WiFi.SSID():String("");
  const String ip=connected?WiFi.localIP().toString():String("");
  const int32_t rssi=connected?WiFi.RSSI():0;
  lock();
  if(state.connected!=connected || strcmp(state.ssid,ssid.c_str()) || strcmp(state.ip,ip.c_str()) || state.rssi!=rssi) {
    const bool lost=state.connected && !connected;
    state.connected=connected; state.rssi=rssi;
    snprintf(state.ssid,sizeof(state.ssid),"%s",ssid.c_str());
    snprintf(state.ip,sizeof(state.ip),"%s",ip.c_str());
    if(lost && !state.busy) snprintf(state.wifiMessage,sizeof(state.wifiMessage),"Связь потеряна. Ожидается восстановление");
    else if(connected && !state.busy) snprintf(state.wifiMessage,sizeof(state.wifiMessage),"Подключено");
    ++state.revision;
  }
  xSemaphoreGive(stateMutex);
}
void scanWifi() {
  message("Поиск сетей Wi-Fi 2.4 ГГц...");
  if(!startWifi()) return;
  WiFi.scanDelete();
  const int found=WiFi.scanNetworks(false,true,false,120);
  AccessPoint results[MAX_AP];uint8_t count=0;
  if(found>=0) {
    for(int i=0;i<found && i<256;i++) {
      const String ssid=WiFi.SSID(i);
      if(ssid.length()==0) continue; // Hidden networks can be entered manually.
      const auto auth=WiFi.encryptionType(i);
      AccessPoint ap;snprintf(ap.ssid,sizeof(ap.ssid),"%s",ssid.c_str());ap.rssi=WiFi.RSSI(i);ap.auth=uint8_t(auth);
      ap.supported=auth==WIFI_AUTH_OPEN || auth==WIFI_AUTH_WPA2_PSK || auth==WIFI_AUTH_WPA_WPA2_PSK || auth==WIFI_AUTH_WPA3_PSK || auth==WIFI_AUTH_WPA2_WPA3_PSK;
      int duplicate=-1;
      for(int j=0;j<count;j++) if(!strcmp(results[j].ssid,ap.ssid) && results[j].auth==ap.auth) {duplicate=j;break;}
      if(duplicate>=0) {if(ap.rssi>results[duplicate].rssi)results[duplicate]=ap;}
      else if(count<MAX_AP) results[count++]=ap;
      else if(ap.rssi>results[count-1].rssi) results[count-1]=ap;
      std::sort(results,results+count,[](const AccessPoint&a,const AccessPoint&b){return a.rssi>b.rssi;});
    }
  }
  WiFi.scanDelete();lock();state.apCount=count;
  for(int i=0;i<count;i++) state.aps[i]=results[i];
  if(found<0) snprintf(state.wifiMessage,sizeof(state.wifiMessage),"Ошибка поиска. Проверьте журнал C6");
  else snprintf(state.wifiMessage,sizeof(state.wifiMessage),"Найдено сетей: %u (до 16 в списке)",unsigned(count));
  unlock();
}
void connectWifi(const Command& cmd, bool save) {
  if(!cmd.ssid[0] || !passwordValid(cmd.password,cmd.open)) { message("Проверьте имя сети и пароль");return; }
  message("Подключение к Wi-Fi...");
  if(!startWifi()) return;
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false,false);
  // Open networks require an explicit UI choice; no silent security downgrade.
  WiFi.setMinSecurity(cmd.open?WIFI_AUTH_OPEN:WIFI_AUTH_WPA2_PSK);
  WiFi.begin(cmd.ssid,cmd.open?nullptr:cmd.password);
  const uint32_t start=millis();
  while(uint32_t(millis()-start)<20000U && !(WiFi.status()==WL_CONNECTED && uint32_t(WiFi.localIP())!=0)) vTaskDelay(pdMS_TO_TICKS(100));
  if(WiFi.status()!=WL_CONNECTED || uint32_t(WiFi.localIP())==0) {
    WiFi.disconnect(false,false);
    message("Не подключено: пароль, сигнал, DHCP или C6. Повторите вручную");
    return;
  }
  bool stored=true;
  if(save) {
    Credentials candidate;snprintf(candidate.ssid,sizeof(candidate.ssid),"%s",cmd.ssid);
    snprintf(candidate.password,sizeof(candidate.password),"%s",cmd.password);candidate.open=cmd.open?1:0;
    stored=nvsOK && prefs.putBytes("credentials",&candidate,sizeof(candidate))==sizeof(candidate);
    if(stored) {saved=candidate;haveCredentials=true;stored=prefs.putBool("autoconnect",true)==1;}
    wipe(&candidate,sizeof(candidate));
  }
  WiFi.setAutoReconnect(true);
  configTime(0,0,"pool.ntp.org","time.cloudflare.com");
  // configTime may change TZ; restore the user's selected timezone.
  lock();const uint8_t zone=state.zone;unlock();setenv("TZ",ZONES[zone],1);tzset();
  lock();state.hasSaved=haveCredentials;state.autoConnect=stored;
  snprintf(state.wifiMessage,sizeof(state.wifiMessage),"%s",stored?"Подключено. Время синхронизируется":"Подключено, но настройки не сохранены");
  unlock();versions();
}
#endif
void scanBle() {
#if PANEL_BLE
  message("Поиск BLE-устройств: 5 секунд...",true);
  if(!configurePins()) {message("Ошибка связи с C6",true);return;}
  if(!BLEDevice::init("BoilerPanel")) {message("BLE не запущен. Проверьте C6",true);return;}
  BLEScan* scanner=BLEDevice::getScan();
  if(!scanner) {message("BLE: сканер недоступен",true);return;}
  scanner->setActiveScan(true);scanner->setInterval(160);scanner->setWindow(80);
  BLEScanResults* found=scanner->start(5,false);
  BleDevice results[MAX_BLE];uint8_t count=0;
  if(found) for(int i=0;i<found->getCount();i++) {
    BLEAdvertisedDevice d=found->getDevice(i);BleDevice item;
    snprintf(item.name,sizeof(item.name),"%s",d.haveName()?d.getName().c_str():"Без имени");
    snprintf(item.address,sizeof(item.address),"%s",d.getAddress().toString().c_str());item.rssi=d.getRSSI();
    if(count<MAX_BLE)results[count++]=item;
    else if(item.rssi>results[count-1].rssi)results[count-1]=item;
    std::sort(results,results+count,[](const BleDevice&a,const BleDevice&b){return a.rssi>b.rssi;});
  }
  scanner->clearResults();
  lock();state.bleCount=count;for(int i=0;i<count;i++)state.ble[i]=results[i];
  snprintf(state.bleMessage,sizeof(state.bleMessage),"%s",found?"Поиск завершён. Только BLE, без подключения":"Ошибка BLE-сканирования");unlock();versions();
#else
  message("BLE не включён в установленной сборке Arduino",true);
#endif
}
void worker(void*) {
  nvsOK=prefs.begin("boiler03",false);
  uint8_t zone=nvsOK?prefs.getUChar("timezone",0):0;if(zone>2)zone=0;
  setenv("TZ",ZONES[zone],1);tzset();
  if(nvsOK && prefs.getBytesLength("credentials")==sizeof(saved)) {
    prefs.getBytes("credentials",&saved,sizeof(saved));
    saved.ssid[32]=0;saved.password[64]=0;
    haveCredentials=saved.magic==0x42503033 && saved.ssid[0] && passwordValid(saved.password,saved.open!=0);
  }
  const bool automatic=nvsOK && haveCredentials && prefs.getBool("autoconnect",false);
  lock();state.zone=zone;state.ready=true;state.hasSaved=haveCredentials;state.autoConnect=automatic;
  state.wifiSupported=PANEL_WIFI;state.bleSupported=PANEL_BLE;
  snprintf(state.wifiMessage,sizeof(state.wifiMessage),"%s",PANEL_WIFI?"Wi-Fi не подключён":"Wi-Fi Remote отсутствует в этой сборке");
  snprintf(state.bleMessage,sizeof(state.bleMessage),"%s",PANEL_BLE?"Нажмите Поиск. Устройства не подключаются":"BLE не включён в установленной сборке Arduino");
  unlock();versions();
#if PANEL_WIFI
  if(automatic) {
    Command cmd;cmd.job=Job::Connect;cmd.open=saved.open;snprintf(cmd.ssid,sizeof(cmd.ssid),"%s",saved.ssid);snprintf(cmd.password,sizeof(cmd.password),"%s",saved.password);
    jobState(Job::Connect,true);connectWifi(cmd,false);wipe(&cmd,sizeof(cmd));jobState(Job::None,false);
  }
#endif
  for(;;) {
    Command cmd;
    if(xQueueReceive(commands,&cmd,pdMS_TO_TICKS(200))==pdTRUE) {
      jobState(cmd.job,true);
      switch(cmd.job) {
        case Job::WifiScan:
#if PANEL_WIFI
          scanWifi();
#else
          message("Wi-Fi Remote отсутствует в этой сборке");
#endif
          break;
        case Job::Connect:
#if PANEL_WIFI
          connectWifi(cmd,true);
#endif
          break;
        case Job::Disconnect:
        case Job::Forget:
#if PANEL_WIFI
          if(wifiStarted) {WiFi.setAutoReconnect(false);WiFi.disconnect(false,false);}
#endif
          if(nvsOK)prefs.putBool("autoconnect",false);
          if(cmd.job==Job::Forget) {if(nvsOK)prefs.remove("credentials");wipe(&saved,sizeof(saved));haveCredentials=false;}
          lock();state.connected=false;state.autoConnect=false;state.hasSaved=haveCredentials;state.ssid[0]=0;state.ip[0]=0;
          snprintf(state.wifiMessage,sizeof(state.wifiMessage),"%s",cmd.job==Job::Forget?"Сохранённая сеть удалена":"Wi-Fi отключён. Автоподключение выключено");unlock();break;
        case Job::BleScan:scanBle();break;
        case Job::Zone:
          if(cmd.zone<=2) {
            setenv("TZ",ZONES[cmd.zone],1);tzset();const bool stored=nvsOK && prefs.putUChar("timezone",cmd.zone)==1;
            lock();state.zone=cmd.zone;snprintf(state.timeMessage,sizeof(state.timeMessage),"%s",stored?"Часовой пояс сохранён":"Часовой пояс изменён, но не сохранён");unlock();
          }break;
        case Job::SetClock: {
          time_t t;bool valid=parseLocalTime(cmd.input,t);
          if(valid) {timeval tv={};tv.tv_sec=t;valid=settimeofday(&tv,nullptr)==0;}
          lock();snprintf(state.timeMessage,sizeof(state.timeMessage),"%s",valid?"Время установлено. При Wi-Fi уточняется по NTP":"Неверная дата или время");unlock();break;
        }
        default:break;
      }
      wipe(&cmd,sizeof(cmd));jobState(Job::None,false);
    }
#if PANEL_WIFI
    updateConnection();
#endif
  }
}
} // namespace
const char* zoneName(uint8_t zone) {return ZONE_NAMES[zone<=2?zone:0];}
bool snapshot(RadioState& out) {
  if(!stateMutex || xSemaphoreTake(stateMutex,pdMS_TO_TICKS(3))!=pdTRUE)return false;
  out=state;xSemaphoreGive(stateMutex);return true;
}
bool submit(const Command& command) {
  if(!commands || !stateMutex)return false;
  if(xSemaphoreTake(stateMutex,pdMS_TO_TICKS(3))!=pdTRUE)return false;
  if(!state.ready || state.busy) {xSemaphoreGive(stateMutex);return false;}
  state.busy=true;state.job=command.job;++state.revision;xSemaphoreGive(stateMutex);
  if(xQueueSend(commands,&command,0)==pdTRUE)return true;
  jobState(Job::None,false);return false;
}
bool begin() {
  stateMutex=xSemaphoreCreateMutex();commands=xQueueCreate(1,sizeof(Command));
  if(!stateMutex || !commands)return false;
  // No radio startup in the LVGL task; the display remains independent.
  return xTaskCreate(worker,"boiler-radio",12288,nullptr,1,nullptr)==pdPASS;
}
} // namespace panel_radio
