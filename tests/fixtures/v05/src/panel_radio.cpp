// ESP32-P4 / Arduino-ESP32 3.3.6. All radio/NVS operations run outside LVGL.
// No automatic C6 firmware updates. No passwords in Serial or UI snapshots.
#include <Arduino.h>
#include <Preferences.h>
#include <sdkconfig.h>
#include <soc/soc_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <sys/time.h>
#include <stdlib.h>
#include <algorithm>
#include <atomic>
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
// One protected mailbox, explicitly erased after consumption (including passwords).
Command pending;
bool pendingCommand=false;
Job urgent=Job::None;
bool workerStarted=false;
SemaphoreHandle_t stateMutex=nullptr;
RadioState state;
Preferences prefs;
bool nvsOK=false;
// Preserve installed v0.3.1 screen settings without touching Wi-Fi namespace.
Preferences screenPrefs;
bool screenNvsOK=false;
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
void jobState(Job job, bool busy) {
  lock();state.job=job;state.busy=busy || pendingCommand || urgent!=Job::None;unlock();
}
bool stopRequested() { lock();const bool stop=urgent!=Job::None;xSemaphoreGive(stateMutex);return stop; }
bool takeCommand(Command& cmd) {
  lock();
  if(urgent!=Job::None) {
    if(pendingCommand && (pending.job==Job::Connect || pending.job==Job::WifiScan)) {
      wipe(&pending,sizeof(pending));pendingCommand=false;
    }
    cmd.job=urgent;urgent=Job::None;
  } else if(pendingCommand) {
    cmd=pending;wipe(&pending,sizeof(pending));pendingCommand=false;
  } else {xSemaphoreGive(stateMutex);return false;}
  state.job=cmd.job;state.busy=true;unlock();return true;
}
bool validDisplayTimeout(uint32_t seconds) {
  return seconds==0 || seconds==30 || seconds==60 || seconds==120 || seconds==300 || seconds==600 || seconds==1800;
}
uint16_t loadDisplayTimeout() {
  const uint32_t seconds=screenNvsOK?screenPrefs.getUInt("off_sec",0):0;
  // Validate before narrowing: corrupt uint32 values must not become valid timeouts.
  return validDisplayTimeout(seconds)?uint16_t(seconds):0;
}
uint16_t initializeDisplaySettings() {
  screenNvsOK=screenPrefs.begin("boiler_screen",false);
  return loadDisplayTimeout();
}
void setDisplayTimeout(uint16_t seconds) {
  if(!validDisplayTimeout(seconds))return;
  lock();const uint16_t previous=state.displayTimeoutSec;xSemaphoreGive(stateMutex);
  // Explicit user command only; no periodic writes, and no rewrite for same value.
  if(previous==seconds) return;
  const bool stored=screenNvsOK && screenPrefs.putUInt("off_sec",uint32_t(seconds))==sizeof(uint32_t);
  lock();
  // Keep persisted policy on failure: never claim that an unsaved value survived reboot.
  if(stored)state.displayTimeoutSec=seconds;
  snprintf(state.displayMessage,sizeof(state.displayMessage),"%s",stored?"Таймаут экрана сохранён":"Ошибка NVS: таймаут не изменён");
  unlock();
}
void formatVersion(char* out, size_t cap, uint32_t a, uint32_t b, uint32_t c) {
  char complete[34];snprintf(complete,sizeof(complete),"%lu.%lu.%lu",(unsigned long)a,(unsigned long)b,(unsigned long)c);
  snprintf(out,cap,"%s",strlen(complete)<cap?complete:"Invalid version");
}
void versions() {
#if PANEL_WIFI || PANEL_BLE
  uint32_t a=0,b=0,c=0;
  hostedGetHostVersion(&a,&b,&c);
  lock();formatVersion(state.hostVersion,sizeof(state.hostVersion),a,b,c);unlock();
  a=b=c=0;
  if(pinsReady)hostedGetSlaveVersion(&a,&b,&c);
  lock();
  if(a || b || c) formatVersion(state.c6Version,sizeof(state.c6Version),a,b,c);
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
bool recoveryPending=false;
bool eventsRegistered=false;
std::atomic<unsigned> lastDisconnectReason{0};
void wifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  // Arduino event task: no LVGL, NVS, blocking mutex, or credential logging.
  if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED && info.wifi_sta_disconnected.reason!=WIFI_REASON_ASSOC_LEAVE)
    lastDisconnectReason.store(info.wifi_sta_disconnected.reason);
}
const char* reasonName(unsigned reason) {
  switch(reason) {
    case WIFI_REASON_AUTH_EXPIRE:return "AUTH_EXPIRE";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:return "4WAY_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND:return "NO_AP_FOUND";
    case WIFI_REASON_AUTH_FAIL:return "AUTH_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:return "HANDSHAKE_TIMEOUT";
    default:return "DISCONNECT";
  }
}
bool startWifi() {
  if(wifiStarted) return true;
  if(!configurePins()) { message("Ошибка настройки связи с C6");return false; }
  if(!eventsRegistered){WiFi.onEvent(wifiEvent,ARDUINO_EVENT_WIFI_STA_DISCONNECTED);eventsRegistered=true;}
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
    if(lost && !state.busy) {
      recoveryPending=true;const unsigned reason=lastDisconnectReason.load();
      snprintf(state.wifiMessage,sizeof(state.wifiMessage),"Связь потеряна: %s (%u)",reasonName(reason),reason);
    } else if(connected && recoveryPending && !state.busy) {
      recoveryPending=false;snprintf(state.wifiMessage,sizeof(state.wifiMessage),"Связь восстановлена");
    }
    ++state.revision;
  }
  xSemaphoreGive(stateMutex);
}
void scanWifi() {
  message("Поиск сетей Wi-Fi 2.4 ГГц...");
  if(!startWifi()) return;
  WiFi.scanDelete();
  const uint32_t scanStarted=millis();
  const int found=WiFi.scanNetworks(false,true,false,120);
  const uint32_t scanElapsed=uint32_t(millis()-scanStarted);
  // Application-level diagnostics remain available with CORE_DEBUG_LEVEL=0.
  // Numeric outcome/timing only: never log SSIDs, addresses or credentials.
  Serial.printf("[BoilerPanel][WiFiScan] result=%d elapsed_ms=%lu\n",found,(unsigned long)scanElapsed);
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
  if(found<0) snprintf(state.wifiMessage,sizeof(state.wifiMessage),"Ошибка поиска: код %d, %lu мс. Нужен журнал C6",found,(unsigned long)scanElapsed);
  else snprintf(state.wifiMessage,sizeof(state.wifiMessage),"Найдено сетей: %u (до 16 в списке)",unsigned(count));
  unlock();
}
void connectWifi(const Command& cmd, bool save) {
  if(stopRequested()){message("Подключение отменено");return;}
  if(!cmd.ssid[0] || !passwordValid(cmd.password,cmd.open)) { message("Проверьте имя сети и пароль");return; }
  recoveryPending=false;
  message("Подключение к Wi-Fi...");
  if(!startWifi()) return;
  WiFi.setAutoReconnect(false);
  const bool hadConnection=WiFi.status()==WL_CONNECTED;
  const bool disconnected=WiFi.disconnect(false,false);
  // An idle STA may reject disconnect because there is no association to end.
  // An existing association must be gone before a new command can be accepted.
  if((hadConnection && !disconnected) || WiFi.status()==WL_CONNECTED) {
    updateConnection();recoveryPending=false;
    message("Не удалось отключить прежнюю сеть. Настройки не изменены");return;
  }
  updateConnection();recoveryPending=false;
  // Open networks require an explicit UI choice; no silent security downgrade.
  WiFi.setMinSecurity(cmd.open?WIFI_AUTH_OPEN:WIFI_AUTH_WPA2_PSK);
  if(stopRequested())return;
  lastDisconnectReason.store(0);
  if(WiFi.begin(cmd.ssid,cmd.open?nullptr:cmd.password)==WL_CONNECT_FAILED) {
    updateConnection();recoveryPending=false;
    message("Не удалось запустить подключение. Настройки не изменены");return;
  }
  const auto requestedNetwork=[&cmd]() {
    return WiFi.status()==WL_CONNECTED && uint32_t(WiFi.localIP())!=0 && !strcmp(WiFi.SSID().c_str(),cmd.ssid);
  };
  const uint32_t start=millis();
  while(!stopRequested() && uint32_t(millis()-start)<20000U && !requestedNetwork()) vTaskDelay(pdMS_TO_TICKS(100));
  if(stopRequested()){
    WiFi.disconnect(false,false);updateConnection();recoveryPending=false;
    message(WiFi.status()==WL_CONNECTED?"Запрос отменён; Wi-Fi остался подключён":"Подключение отменено");return;
  }
  if(!requestedNetwork()) {
    const unsigned reason=lastDisconnectReason.load();const bool associated=WiFi.status()==WL_CONNECTED;
    const bool wrongNetwork=associated && strcmp(WiFi.SSID().c_str(),cmd.ssid);
    WiFi.disconnect(false,false);updateConnection();recoveryPending=false;char detail[150];
    if(wrongNetwork)snprintf(detail,sizeof(detail),"Подтверждена другая сеть. Настройки не сохранены");
    else if(reason)snprintf(detail,sizeof(detail),"%s (%u). Нет IP за 20 с; повторите вручную",reasonName(reason),reason);
    else snprintf(detail,sizeof(detail),"%s",associated?"Таймаут DHCP: нет IP за 20 с":"Таймаут Wi-Fi: нет IP за 20 с. Проверьте журнал C6");
    message(detail);
    return;
  }
  updateConnection();recoveryPending=false;
  bool stored=true;
  lock();bool automatic=state.autoConnect;xSemaphoreGive(stateMutex);
  if(save) {
    Credentials candidate;wipe(&candidate,sizeof(candidate));candidate.magic=0x42503033;snprintf(candidate.ssid,sizeof(candidate.ssid),"%s",cmd.ssid);
    snprintf(candidate.password,sizeof(candidate.password),"%s",cmd.password);candidate.open=cmd.open?1:0;
    stored=nvsOK && prefs.putBytes("credentials",&candidate,sizeof(candidate))==sizeof(candidate);
    if(stored) {haveCredentials=true;stored=prefs.putBool("autoconnect",true)==1;automatic=stored || prefs.getBool("autoconnect",false);}
    wipe(&candidate,sizeof(candidate));
  }
  WiFi.setAutoReconnect(true);
  configTime(0,0,"pool.ntp.org","time.cloudflare.com");
  // configTime may change TZ; restore the user's selected timezone.
  lock();const uint8_t zone=state.zone;unlock();setenv("TZ",ZONES[zone],1);tzset();
  lock();state.hasSaved=haveCredentials;state.autoConnect=automatic;
  snprintf(state.wifiMessage,sizeof(state.wifiMessage),"%s",stored?"Подключено. Время синхронизируется":"Подключено, но настройки не сохранены");
  unlock();versions();
}
#endif
#if PANEL_BLE
std::atomic<bool> bleScanComplete{false};
bool bleScanPending=false; // Worker-owned; a missing callback must not overlap a new scan.
void bleScanFinished(BLEScanResults) {
  // NimBLE event task: no UI, NVS or application mutex; publish completion only.
  bleScanComplete.store(true);
}
#endif
void scanBle() {
#if PANEL_BLE
  lock();state.bleCount=0;unlock();
  message("Поиск BLE-устройств: 5 секунд...",true);
  if(!configurePins()) {message("Ошибка связи с C6",true);return;}
  if(!BLEDevice::init("BoilerPanel")) {message("BLE не запущен. Проверьте C6",true);return;}
  BLEScan* scanner=BLEDevice::getScan();
  if(!scanner) {message("BLE: сканер недоступен",true);return;}
  if(scanner->isScanning()) {message("Поиск BLE уже выполняется",true);return;}
  if(bleScanPending && !bleScanComplete.load()) {
    message("Нет подтверждения предыдущего поиска BLE",true);return;
  }
  scanner->setActiveScan(true);scanner->setInterval(160);scanner->setWindow(80);
  bleScanComplete.store(false);
  bleScanPending=true;
  // The blocking 3.3.6 wrapper returns a results pointer even on launch failure.
  if(!scanner->start(5,bleScanFinished,false)) {bleScanPending=false;message("Не удалось запустить поиск BLE",true);return;}
  const uint32_t started=millis();
  // The extra 2 s is a worker watchdog for the 5 s SDK scan, not a hardware setting.
  while(!stopRequested() && uint32_t(millis()-started)<7000U && (scanner->isScanning() || !bleScanComplete.load()))
    vTaskDelay(pdMS_TO_TICKS(50));
  const bool cancelled=stopRequested();
  if(cancelled || scanner->isScanning() || !bleScanComplete.load()) {
    const bool stopped=!scanner->isScanning() || scanner->stop();
    const bool inactive=stopped && !scanner->isScanning();
    // Never free the SDK result vector while its event task can still write to it.
    const bool confirmed=inactive && bleScanComplete.load();
    if(confirmed){bleScanPending=false;scanner->clearResults();}
    message(!inactive?"Поиск BLE не удалось остановить":cancelled?(confirmed?"Поиск BLE отменён":"Отмена BLE: нет подтверждения завершения"):"Таймаут BLE: нет подтверждения завершения",true);
    return;
  }
  bleScanPending=false;
  BLEScanResults* found=scanner->getResults();
  if(!found){message("BLE: результат недоступен",true);return;}
  BleDevice results[MAX_BLE];uint8_t count=0;
  for(int i=0;i<found->getCount();i++) {
    BLEAdvertisedDevice d=found->getDevice(i);BleDevice item;
    snprintf(item.name,sizeof(item.name),"%s",d.haveName()?d.getName().c_str():"Без имени");
    snprintf(item.address,sizeof(item.address),"%s",d.getAddress().toString().c_str());item.rssi=d.getRSSI();
    if(count<MAX_BLE)results[count++]=item;
    else if(item.rssi>results[count-1].rssi)results[count-1]=item;
    std::sort(results,results+count,[](const BleDevice&a,const BleDevice&b){return a.rssi>b.rssi;});
  }
  scanner->clearResults();
  lock();state.bleCount=count;for(int i=0;i<count;i++)state.ble[i]=results[i];
  snprintf(state.bleMessage,sizeof(state.bleMessage),"Поиск завершён. Найдено: %u. Только BLE",unsigned(count));unlock();versions();
#else
  message("BLE не включён в установленной сборке Arduino",true);
#endif
}
void disconnectWifi(bool forget) {
  bool disconnected=true;
#if PANEL_WIFI
  if(wifiStarted){
    recoveryPending=false;WiFi.setAutoReconnect(false);
    const bool hadConnection=WiFi.status()==WL_CONNECTED;
    const bool accepted=WiFi.disconnect(false,false);
    disconnected=(!hadConnection || accepted) && WiFi.status()!=WL_CONNECTED;
    updateConnection();recoveryPending=false;
  }
#endif
  const bool autoStored=nvsOK && prefs.putBool("autoconnect",false)==1;
  bool forgotten=true;
  if(forget){
    forgotten=nvsOK && (!prefs.isKey("credentials") || prefs.remove("credentials"));
    wipe(&saved,sizeof(saved));if(forgotten)haveCredentials=false;
  }
  lock();state.autoConnect=haveCredentials && !autoStored && state.autoConnect;state.hasSaved=haveCredentials;
  if(disconnected){state.connected=false;state.ssid[0]=0;state.ip[0]=0;state.rssi=0;}
  const char* result=!forgotten?"Не удалось удалить сеть из NVS":!autoStored?"NVS: автозапуск не сохранён":forget?"Сохранённая сеть удалена":"Автоподключение выключено";
  snprintf(state.wifiMessage,sizeof(state.wifiMessage),"%s. %s",disconnected?"Wi-Fi отключён":"Не удалось отключить Wi-Fi",result);unlock();
}
void worker(void*) {
  nvsOK=prefs.begin("boiler03",false);
  uint8_t zone=nvsOK?prefs.getUChar("timezone",0):0;if(zone>2)zone=0;
  setenv("TZ",ZONES[zone],1);tzset();
  if(nvsOK && prefs.getBytesLength("credentials")==sizeof(saved)) {
    const bool read=prefs.getBytes("credentials",&saved,sizeof(saved))==sizeof(saved);
    haveCredentials=read && saved.magic==0x42503033 && saved.open<=1 && saved.ssid[0] &&
      memchr(saved.ssid,0,sizeof(saved.ssid)) && memchr(saved.password,0,sizeof(saved.password)) && passwordValid(saved.password,saved.open!=0);
    if(!haveCredentials)wipe(&saved,sizeof(saved));
  }
  const bool automatic=nvsOK && haveCredentials && prefs.getBool("autoconnect",false);
  const uint16_t displayTimeout=initializeDisplaySettings();
  lock();state.displayTimeoutSec=displayTimeout;
  snprintf(state.displayMessage,sizeof(state.displayMessage),"%s",screenNvsOK?"":"NVS недоступна: таймаут не сохранится");
  state.busy=automatic && PANEL_WIFI;state.job=state.busy?Job::Connect:Job::None;state.zone=zone;state.ready=true;state.hasSaved=haveCredentials;state.autoConnect=automatic;
  state.wifiSupported=PANEL_WIFI;state.bleSupported=PANEL_BLE;
  snprintf(state.wifiMessage,sizeof(state.wifiMessage),"%s",PANEL_WIFI?"Wi-Fi не подключён":"Wi-Fi Remote отсутствует в этой сборке");
  snprintf(state.bleMessage,sizeof(state.bleMessage),"%s",PANEL_BLE?"Нажмите Поиск. Устройства не подключаются":"BLE не включён в установленной сборке Arduino");
  unlock();
  if(!nvsOK)message("NVS недоступна: настройки не будут сохранены");
  versions();
#if PANEL_WIFI
  if(automatic) {
    Command cmd;cmd.job=Job::Connect;cmd.open=saved.open;snprintf(cmd.ssid,sizeof(cmd.ssid),"%s",saved.ssid);snprintf(cmd.password,sizeof(cmd.password),"%s",saved.password);
    jobState(Job::Connect,true);connectWifi(cmd,false);wipe(&cmd,sizeof(cmd));jobState(Job::None,false);
  }
#endif
  wipe(&saved,sizeof(saved)); // Boot credential copy is no longer needed.
  for(;;) {
    Command cmd;
    if(takeCommand(cmd)) {
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
#else
          message("Wi-Fi Remote отсутствует в этой сборке");
#endif
          break;
        case Job::Disconnect:
        case Job::Forget:disconnectWifi(cmd.job==Job::Forget);break;
        case Job::SetDisplayTimeout:setDisplayTimeout(cmd.displayTimeoutSec);break;
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
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
} // namespace
const char* zoneName(uint8_t zone) {return ZONE_NAMES[zone<=2?zone:0];}
bool snapshot(RadioState& out) {
  if(!stateMutex || xSemaphoreTake(stateMutex,pdMS_TO_TICKS(3))!=pdTRUE)return false;
  out=state;xSemaphoreGive(stateMutex);return true;
}
bool submit(const Command& command) {
  if(!stateMutex)return false;
  if(command.job==Job::None || unsigned(command.job)>unsigned(Job::SetDisplayTimeout))return false;
  if(command.job==Job::Connect && (!memchr(command.ssid,0,sizeof(command.ssid)) || !command.ssid[0] ||
      !memchr(command.password,0,sizeof(command.password)) || !passwordValid(command.password,command.open)))return false;
  if(command.job==Job::SetClock && !memchr(command.input,0,sizeof(command.input)))return false;
  if(command.job==Job::Zone && command.zone>2)return false;
  if(command.job==Job::SetDisplayTimeout && !validDisplayTimeout(command.displayTimeoutSec))return false;
  if(xSemaphoreTake(stateMutex,pdMS_TO_TICKS(3))!=pdTRUE)return false;
  if(!state.ready){xSemaphoreGive(stateMutex);return false;}
  if(command.job==Job::Disconnect || command.job==Job::Forget){
    if(urgent!=Job::Forget)urgent=command.job;
    state.busy=true;unlock();return true;
  }
  if(state.busy || pendingCommand || urgent!=Job::None){xSemaphoreGive(stateMutex);return false;}
  pending=command;pendingCommand=true;
  if(command.job!=Job::Connect)wipe(pending.password,sizeof(pending.password));
  state.busy=true;state.job=command.job;unlock();return true;
}
bool cancel(){Command cmd;cmd.job=Job::Disconnect;return submit(cmd);}
bool begin() {
  if(workerStarted)return true;
  stateMutex=xSemaphoreCreateMutex();if(!stateMutex)return false;
  // All radio/NVS work stays out of the LVGL task.
  workerStarted=xTaskCreate(worker,"boiler-radio",12288,nullptr,1,nullptr)==pdPASS;
  if(!workerStarted){vSemaphoreDelete(stateMutex);stateMutex=nullptr;}
  return workerStarted;
}
} // namespace panel_radio
