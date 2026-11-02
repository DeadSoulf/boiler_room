// JC4880P443 ESP32-P4: small read-only web UI on the existing Wi-Fi connection.
// Network work runs in its own task, NEVER on LVGL or radio worker tasks.
// No AP mode, no Wi-Fi credentials, no remote actuator or demo commands.
#include <Arduino.h>
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <time.h>
#include "panel_web.h"
#include "panel_radio.h"

#if defined(CONFIG_ESP_WIFI_REMOTE_ENABLED) && CONFIG_ESP_WIFI_REMOTE_ENABLED
#define BP_WEB_ENABLED 1
#include <WiFi.h>
#include <WebServer.h>
#include "panel_web_page.h"
#else
#define BP_WEB_ENABLED 0
#endif

namespace panel_web {
namespace {
struct Temp {
  bool valid=false;
  float value=0;
  uint32_t ageSeconds=0;
};
struct WebState {
  Temp temps[panel::CHANNELS];
  uint8_t fuel=0;
  uint8_t equipmentModes[panel::equipment_demo::COUNT]={};
  uint8_t equipmentRequests[panel::equipment_demo::COUNT]={};
  uint32_t uptimeSeconds=0;
  bool published=false;
};
SemaphoreHandle_t dataMutex=nullptr;
WebState latest;
TaskHandle_t httpTask=nullptr;

const char* fuelText(uint8_t status) {
  switch(static_cast<panel::FuelLevelStatus>(status)) {
    case panel::FuelLevelStatus::BelowLower:return "Ниже нижнего датчика";
    case panel::FuelLevelStatus::Between:return "Между датчиками";
    case panel::FuelLevelStatus::UpperReached:return "Верхний уровень достигнут";
    case panel::FuelLevelStatus::Stale:return "Устаревшие данные";
    case panel::FuelLevelStatus::Inconsistent:return "Ошибка датчиков";
    default:return "Нет данных";
  }
}
const char* modeText(uint8_t mode) {
  switch(static_cast<panel::equipment_demo::Mode>(mode)){
    case panel::equipment_demo::Mode::Enabled:return "enabled";
    case panel::equipment_demo::Mode::Off:return "off";
    default:return "unknown";
  }
}
const char* requestText(uint8_t status) {
  switch(static_cast<panel::equipment_demo::Status>(status)){
    case panel::equipment_demo::Status::Pending:return "pending";
    case panel::equipment_demo::Status::Accepted:return "accepted";
    case panel::equipment_demo::Status::Rejected:return "rejected";
    case panel::equipment_demo::Status::Timeout:return "timeout";
    default:return "none";
  }
}

#if BP_WEB_ENABLED
WebServer server(80);

void headers() {
  server.sendHeader("Cache-Control","no-store, max-age=0");
  server.sendHeader("X-Content-Type-Options","nosniff");
  server.sendHeader("X-Frame-Options","DENY");
  server.sendHeader("Referrer-Policy","no-referrer");
}
void index() {
  headers();
  server.send_P(200, "text/html; charset=utf-8", kBoilerWebPage);
}
void status() {
  WebState snap;
  if (!dataMutex || xSemaphoreTake(dataMutex,pdMS_TO_TICKS(20))!=pdTRUE) {
    headers();server.send(503,"application/json; charset=utf-8","{\"error\":\"busy\"}");return;
  }
  snap=latest;
  xSemaphoreGive(dataMutex);
  if(!snap.published) {
    headers();server.send(503,"application/json; charset=utf-8","{\"error\":\"starting\"}");return;
  }

  panel::RadioState radio;
  const bool haveRadio=panel_radio::snapshot(radio);
  if(!haveRadio) radio=panel::RadioState{};
  char clock[40]="";
  const time_t current=time(nullptr);
  if(current>=time_t(1704067200)) {
    struct tm local{};
    localtime_r(&current,&local);
    strftime(clock,sizeof(clock),"%d.%m.%Y %H:%M:%S",&local);
  }

  String json;
  json.reserve(1900);
  json += "{\"version\":\"";json += panel::VERSION;
  json += "\",\"uptime_s\":";json += String(snap.uptimeSeconds);
  json += ",\"clock\":\"";json += clock;json += '"';
  json += ",\"wifi\":{\"connected\":";
  json += radio.connected ? "true":"false";
  json += ",\"ip\":\"";json += radio.ip;
  json += "\",\"rssi\":";json += String(radio.rssi);
  json += "}";
  json += ",\"screen_timeout_s\":";json += String(radio.displayTimeoutSec);
  json += ",\"host_version\":\"";json += radio.hostVersion;
  json += "\",\"c6_version\":\"";json += radio.c6Version;
  json += "\",\"temperatures\":[";
  for(unsigned i=0;i<panel::CHANNELS;i++){
    if(i)json += ',';
    json += "{\"valid\":";
    json += snap.temps[i].valid?"true":"false";
    json += ",\"value\":";
    if(snap.temps[i].valid) json += String(snap.temps[i].value,1);
    else json += "null";
    json += ",\"age_s\":";
    json += String(snap.temps[i].ageSeconds);
    json += '}';
  }
  json += "],\"fuel_level_text\":\"";
  json += fuelText(snap.fuel);
  json += "\",\"equipment\":[";
  for(unsigned i=0;i<panel::equipment_demo::COUNT;i++){
    if(i)json += ',';
    json += "{\"mode\":\"";json += modeText(snap.equipmentModes[i]);
    json += "\",\"request\":\"";json += requestText(snap.equipmentRequests[i]);
    json += "\"}";
  }
  json += "],\"physical_control\":false,\"data_origin\":\"device\"}";

  headers();
  server.send(200,"application/json; charset=utf-8",json);
}
void notFound() {
  headers();
  server.send(404,"text/plain; charset=utf-8","Не найдено");
}
void worker(void*) {
  // Only GET endpoints; no CORS, cookies, credential edits or HTTP POST handlers.
  server.on("/",HTTP_GET,index);
  server.on("/api/status",HTTP_GET,status);
  server.on("/health",HTTP_GET,[]{headers();server.send(200,"text/plain; charset=utf-8","ok");});
  server.onNotFound(notFound);
  bool listening=false;
  for(;;) {
    panel::RadioState radio;
    const bool connected=panel_radio::snapshot(radio) && radio.connected;
    if(connected) {
      if(!listening) {
        server.begin();
        listening=true;
        Serial.printf("[BP-WEB] listening http://%s/\n", radio.ip);
      }
      server.handleClient();
    } else if(listening) {
      server.stop();
      listening=false;
      Serial.println("[BP-WEB] stopped; Wi-Fi disconnected");
    }
    vTaskDelay(pdMS_TO_TICKS(30));
  }
}
#endif
} // namespace

void publish(const panel::Measurement (&temps)[panel::CHANNELS],
             const panel::FuelLevel &fuel,
             const panel::equipment_demo::State &equipment,
             uint32_t atMs) {
  if(!dataMutex) return;
  WebState next;
  next.uptimeSeconds=atMs/1000U;
  next.fuel=static_cast<uint8_t>(fuel.status(atMs));
  for(unsigned i=0;i<panel::CHANNELS;i++) {
    const bool fresh=temps[i].fresh(atMs);
    next.temps[i].valid=fresh;
    next.temps[i].value=fresh?temps[i].value:0;
    next.temps[i].ageSeconds=fresh?uint32_t(atMs-temps[i].updated)/1000U:0;
  }
  for(unsigned i=0;i<panel::equipment_demo::COUNT;i++) {
    next.equipmentModes[i]=static_cast<uint8_t>(equipment.devices[i].mode);
    next.equipmentRequests[i]=static_cast<uint8_t>(equipment.devices[i].command.status);
  }
  next.published=true;
  // The LVGL thread must never block on an HTTP request.
  if(xSemaphoreTake(dataMutex,0)==pdTRUE) {
    latest=next;
    xSemaphoreGive(dataMutex);
  }
}
bool begin() {
#if BP_WEB_ENABLED
  if(httpTask) return true;
  dataMutex=xSemaphoreCreateMutex();
  if(!dataMutex) {Serial.println("[BP-WEB] cannot allocate mutex");return false;}
  if(xTaskCreate(worker,"boiler-http",12288,nullptr,1,&httpTask)!=pdPASS) {
    vSemaphoreDelete(dataMutex);
    dataMutex=nullptr;
    Serial.println("[BP-WEB] cannot start web task");
    return false;
  }
  Serial.println("[BP-WEB] server task started; waiting for Wi-Fi and IP");
  return true;
#else
  Serial.println("[BP-WEB] Wi-Fi Remote not enabled; server disabled");
  return false;
#endif
}
} // namespace panel_web
