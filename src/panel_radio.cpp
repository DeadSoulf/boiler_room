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
#include <esp_wifi.h>
#include <esp_err.h>
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
bool eventInstalled=false;
// Set only while the initialized Wi-Fi driver is stopped for bounded recovery.
// Do not use WiFi.mode(WIFI_OFF): that deinitializes ESP-Hosted / affects BLE.
bool driverStoppedForRecovery=false;
std::atomic<uint32_t> staStartEvents{0};
std::atomic<uint32_t> staStopEvents{0};
std::atomic<unsigned> disconnectReason{0};
std::atomic<bool> observeConnect{false};
std::atomic<bool> associated{false};
void wifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  // Runs in the Arduino event task. No LVGL, NVS, Serial or radio calls here.
  if(event==ARDUINO_EVENT_WIFI_STA_START) staStartEvents.fetch_add(1);
  if(event==ARDUINO_EVENT_WIFI_STA_STOP) {
    associated.store(false);
    staStopEvents.fetch_add(1);
  }
  if(event==ARDUINO_EVENT_WIFI_STA_CONNECTED) associated.store(true);
  if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    associated.store(false);
    if(observeConnect.load()) disconnectReason.store(info.wifi_sta_disconnected.reason);
  }
}
void wifiTrace(const char* stage, esp_err_t err, uint32_t elapsed=0) {
  if(Serial) Serial.printf("[BP-WIFI] %s: %s (0x%X), %lu ms\n",stage,
     esp_err_to_name(err),unsigned(err),(unsigned long)elapsed);
}
void wifiFailure(const char* stage, esp_err_t err) {
  char text[150];
  snprintf(text,sizeof(text),"%s: %s (0x%X)",stage,esp_err_to_name(err),unsigned(err));
  message(text);wifiTrace(stage,err);
}
// The delay in the old scan path was not an acknowledgement of STA stopping.
// Observe the actual Arduino event forwarded from the remote driver instead.
bool waitWifiEvent(const std::atomic<uint32_t>& counter, uint32_t previous) {
  const uint32_t since=millis();
  while(counter.load()==previous && uint32_t(millis()-since)<2000U)
    vTaskDelay(pdMS_TO_TICKS(20));
  return counter.load()!=previous;
}
bool restartStoppedWifi() {
  if(!driverStoppedForRecovery) return true;
  const uint32_t before=staStartEvents.load();
  const esp_err_t err=esp_wifi_start();
  wifiTrace("recovery_start",err);
  if(err!=ESP_OK) {wifiFailure("Запуск Wi-Fi",err);return false;}
  driverStoppedForRecovery=false;
  if(!waitWifiEvent(staStartEvents,before)) {
    wifiFailure("Ожидание STA_START",ESP_ERR_TIMEOUT);return false;
  }
  wifiTrace("recovery_started",ESP_OK);
  return true;
}
bool recoverIdleWifiForScan() {
  // A driver-confirmed association (including DHCP-in-progress) is preserved.
  // A stale WL_* flag alone is not enough to decide it is safe to stop Wi-Fi.
  wifi_ap_record_t current={};
  const esp_err_t link=esp_wifi_sta_get_ap_info(&current);
  wifiTrace("recovery_link",link);
  if(link==ESP_OK) {
    message("Wi-Fi занят подключением. Повторите поиск позже");return false;
  }
  if(link!=ESP_ERR_WIFI_NOT_CONNECT) {
    wifiFailure("Состояние Wi-Fi",link);return false;
  }
  message("Восстановление поиска Wi-Fi...");
  WiFi.setAutoReconnect(false);
  observeConnect.store(false);
  const uint32_t before=staStopEvents.load();
  esp_err_t err=esp_wifi_stop();
  wifiTrace("recovery_stop",err);
  if(err!=ESP_OK) {wifiFailure("Остановка Wi-Fi",err);return false;}
  driverStoppedForRecovery=true;
  if(!waitWifiEvent(staStopEvents,before)) {
    // Bring the initialized driver back, but do not pretend a missing event
    // confirms recovery. No second scan is issued in this case.
    if(restartStoppedWifi()) wifiFailure("Ожидание STA_STOP",ESP_ERR_TIMEOUT);
    return false;
  }
  // Clear only transient STA configuration. Never erase Wi-Fi NVS, credentials
  // saved by the panel, other settings, or either chip's firmware.
  err=esp_wifi_set_storage(WIFI_STORAGE_RAM);
  wifiTrace("recovery_storage_ram",err);
  if(err!=ESP_OK) {
    if(restartStoppedWifi()) wifiFailure("Режим памяти Wi-Fi",err);
    return false;
  }
  wifi_config_t empty={};
  err=esp_wifi_set_config(WIFI_IF_STA,&empty);
  wifiTrace("recovery_clear_runtime_config",err);
  if(err!=ESP_OK) {
    if(restartStoppedWifi()) wifiFailure("Сброс текущей попытки",err);
    return false;
  }
  associated.store(false);
  if(!restartStoppedWifi()) return false;
  err=esp_wifi_set_ps(WIFI_PS_NONE);
  wifiTrace("recovery_power_save_off",err);
  return true;
}
bool startWifi() {
  if(!wifiStarted) {
    if(!configurePins()) { message("SDIO: ошибка настройки выводов");return false; }
    if(!eventInstalled) {WiFi.onEvent(wifiEvent);eventInstalled=true;}
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.setHostname("boiler-panel");
    const uint32_t t=millis();
    if(!WiFi.mode(WIFI_STA)) {
      message("WiFi.mode(STA): отказ запуска");
      wifiTrace("WiFi.mode(STA)",ESP_FAIL,uint32_t(millis()-t));return false;
    }
    wifiTrace("WiFi.mode(STA)",ESP_OK,uint32_t(millis()-t));
    wifiStarted=true;versions();
  }
  if(!restartStoppedWifi()) return false;
  // Check the actual remote driver, not just the application's cached flag.
  wifi_mode_t mode=WIFI_MODE_NULL;
  esp_err_t err=esp_wifi_get_mode(&mode);
  if(err!=ESP_OK) {wifiFailure("Режим Wi-Fi",err);return false;}
  if(mode!=WIFI_MODE_STA && mode!=WIFI_MODE_APSTA) {
    wifiFailure("Wi-Fi не в STA",ESP_ERR_WIFI_MODE);return false;
  }
  // Failure to disable power saving must not silently replace the main error.
  err=esp_wifi_set_ps(WIFI_PS_NONE);wifiTrace("power_save_off",err);
  return true;
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
      recoveryPending=true;const unsigned reason=disconnectReason.load();
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
  lock();state.apCount=0;unlock();
  if(!startWifi()) return;
  wifi_ap_record_t current={};
  const esp_err_t link=esp_wifi_sta_get_ap_info(&current);
  if(link!=ESP_OK && link!=ESP_ERR_WIFI_NOT_CONNECT) {
    wifiFailure("Состояние перед поиском",link);return;
  }
  const bool wasConnected=link==ESP_OK || WiFi.status()==WL_CONNECTED;
  const bool reconnect=WiFi.getAutoReconnect();
  WiFi.setAutoReconnect(false);
  if(!wasConnected) {
    // A connection attempt has priority over a user scan; cancel it first.
    observeConnect.store(false);
    const esp_err_t cancel=esp_wifi_disconnect();
    wifiTrace("cancel_pending_connect",cancel);
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  WiFi.scanDelete(); // Clear Arduino's previous result/event flags only.
  wifi_scan_config_t config={};
  config.show_hidden=true;config.scan_type=WIFI_SCAN_TYPE_ACTIVE;
  config.scan_time.active.min=100;config.scan_time.active.max=120;
  config.home_chan_dwell_time=30;
  const uint32_t started=millis();
  // Blocking only this worker task. Unlike Arduino scanNetworks(), this path
  // preserves the real ESP-IDF result and does not wait for WIFI_SCAN_DONE_BIT.
  esp_err_t err=esp_wifi_scan_start(&config,true);
  wifiTrace("scan_start",err,uint32_t(millis()-started));
  if(err==ESP_ERR_WIFI_STATE && !wasConnected) {
    // One recovery per user request; no deinit/reboot/reflash and no retry loop.
    if(!recoverIdleWifiForScan()) {esp_wifi_clear_ap_list();return;}
    message("Повторный поиск Wi-Fi...");
    const uint32_t retryAt=millis();
    err=esp_wifi_scan_start(&config,true);
    wifiTrace("scan_retry",err,uint32_t(millis()-retryAt));
  }
  if(err!=ESP_OK) {
    wifiFailure("Поиск",err);
    if(err==ESP_ERR_WIFI_TIMEOUT) esp_wifi_scan_stop();
    esp_wifi_clear_ap_list();
    if(wasConnected)WiFi.setAutoReconnect(reconnect);
    return;
  }
  uint16_t total=0;
  err=esp_wifi_scan_get_ap_num(&total);
  if(err!=ESP_OK) {
    wifiFailure("Число сетей",err);esp_wifi_clear_ap_list();
    if(wasConnected) { WiFi.setAutoReconnect(reconnect); }
    return;
  }
  AccessPoint results[MAX_AP];uint8_t count=0;
  if(total>0) {
    // The driver sorts by RSSI. Bound allocation even in a crowded RF area.
    uint16_t returned=std::min<uint16_t>(total,64);
    auto* records=static_cast<wifi_ap_record_t*>(calloc(returned,sizeof(wifi_ap_record_t)));
    if(!records) {
      wifiFailure("Память поиска",ESP_ERR_NO_MEM);esp_wifi_clear_ap_list();
      if(wasConnected) { WiFi.setAutoReconnect(reconnect); }
      return;
    }
    err=esp_wifi_scan_get_ap_records(&returned,records);
    if(err!=ESP_OK) {
      free(records);esp_wifi_clear_ap_list();wifiFailure("Список сетей",err);
      if(wasConnected) { WiFi.setAutoReconnect(reconnect); }
      return;
    }
    for(uint16_t i=0;i<returned;i++) {
      const wifi_ap_record_t& record=records[i];
      if(!record.ssid[0])continue; // Hidden SSIDs can still be entered manually.
      AccessPoint ap;memcpy(ap.ssid,record.ssid,32);ap.ssid[32]=0;
      ap.rssi=record.rssi;const auto auth=record.authmode;ap.auth=uint8_t(auth);
      ap.supported=auth==WIFI_AUTH_OPEN || auth==WIFI_AUTH_WPA2_PSK || auth==WIFI_AUTH_WPA_WPA2_PSK || auth==WIFI_AUTH_WPA3_PSK || auth==WIFI_AUTH_WPA2_WPA3_PSK;
      int duplicate=-1;
      for(int j=0;j<count;j++)if(!strcmp(results[j].ssid,ap.ssid) && results[j].auth==ap.auth){duplicate=j;break;}
      if(duplicate>=0){if(ap.rssi>results[duplicate].rssi)results[duplicate]=ap;}
      else if(count<MAX_AP)results[count++]=ap;
      else if(ap.rssi>results[count-1].rssi)results[count-1]=ap;
      std::sort(results,results+count,[](const AccessPoint&a,const AccessPoint&b){return a.rssi>b.rssi;});
    }
    free(records);
  } else esp_wifi_clear_ap_list();
  if(wasConnected)WiFi.setAutoReconnect(reconnect);
  lock();state.apCount=count;
  for(int i=0;i<count;i++)state.aps[i]=results[i];
  snprintf(state.wifiMessage,sizeof(state.wifiMessage),"Найдено: %u. В списке: %u",unsigned(total),unsigned(count));unlock();
  if(Serial)Serial.printf("[BP-WIFI] scan_result: total=%u listed=%u\n",unsigned(total),unsigned(count));
}
void connectWifi(const Command& cmd, bool save) {
  if(stopRequested()){message("Подключение отменено");return;}
  if(!cmd.ssid[0] || !passwordValid(cmd.password,cmd.open)) { message("Проверьте имя сети и пароль");return; }
  recoveryPending=false;
  message("Подключение к Wi-Fi...");
  if(!startWifi()) return;
  WiFi.setAutoReconnect(false);
  observeConnect.store(false); // Intentional disconnect must not overwrite failure reason.
  const bool hadConnection=WiFi.status()==WL_CONNECTED;
  const bool disconnected=WiFi.disconnect(false,false);
  // An idle STA may reject disconnect because there is no association to end.
  // An existing association must be gone before a new command can be accepted.
  if((hadConnection && !disconnected) || WiFi.status()==WL_CONNECTED) {
    updateConnection();recoveryPending=false;
    message("Не удалось отключить прежнюю сеть. Настройки не изменены");return;
  }
  updateConnection();recoveryPending=false;
  vTaskDelay(pdMS_TO_TICKS(200)); // Settle previous remote association state.
  // Open networks require an explicit UI choice; no silent security downgrade.
  WiFi.setMinSecurity(cmd.open?WIFI_AUTH_OPEN:WIFI_AUTH_WPA2_PSK);
  if(stopRequested())return;
  disconnectReason.store(0);
  observeConnect.store(true);
  const uint32_t attemptAt=millis();
  const wl_status_t beginResult=WiFi.begin(cmd.ssid,cmd.open?nullptr:cmd.password);
  if(Serial) Serial.printf("[BP-WIFI] connect_begin status=%u open=%u pass_len=%u\n",
                           unsigned(beginResult),unsigned(cmd.open),unsigned(strlen(cmd.password)));
  if(beginResult==WL_CONNECT_FAILED) {
    observeConnect.store(false);
    updateConnection();recoveryPending=false;
    message("Не удалось запустить подключение. Настройки не изменены");return;
  }
  const auto requestedNetwork=[&cmd]() {
    return WiFi.status()==WL_CONNECTED && uint32_t(WiFi.localIP())!=0 && !strcmp(WiFi.SSID().c_str(),cmd.ssid);
  };
  const uint32_t start=millis();
  while(!stopRequested() && uint32_t(millis()-start)<20000U && !requestedNetwork()) vTaskDelay(pdMS_TO_TICKS(100));
  if(stopRequested()){
    observeConnect.store(false);
    WiFi.disconnect(false,false);updateConnection();recoveryPending=false;
    message(WiFi.status()==WL_CONNECTED?"Запрос отменён; Wi-Fi остался подключён":"Подключение отменено");return;
  }
  if(!requestedNetwork()) {
    observeConnect.store(false);
    const unsigned reason=disconnectReason.load();const bool associated=WiFi.status()==WL_CONNECTED;
    const bool wrongNetwork=associated && strcmp(WiFi.SSID().c_str(),cmd.ssid);
    const auto statusBefore=WiFi.status();
    wifi_ap_record_t current={};
    const esp_err_t link=esp_wifi_sta_get_ap_info(&current);
    if(Serial) Serial.printf("[BP-WIFI] connect_failed reason=%u status=%u link=0x%X elapsed_ms=%lu\n",
       reason,unsigned(statusBefore),unsigned(link),(unsigned long)uint32_t(millis()-attemptAt));
    WiFi.disconnect(false,false);updateConnection();recoveryPending=false;char detail[150];
    if(wrongNetwork)snprintf(detail,sizeof(detail),"Подтверждена другая сеть. Настройки не сохранены");
    else if(reason)snprintf(detail,sizeof(detail),"%s (%u). Нет IP за 20 с; повторите вручную",reasonName(reason),reason);
    else snprintf(detail,sizeof(detail),"%s",associated?"Таймаут DHCP: нет IP за 20 с":"Таймаут Wi-Fi: нет IP за 20 с. Проверьте журнал C6");
    message(detail);
    return;
  }
  observeConnect.store(false);
  if(Serial) Serial.printf("[BP-WIFI] connected_with_ip elapsed_ms=%lu\n",(unsigned long)uint32_t(millis()-attemptAt));
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
