#pragma once
// Deterministic HOST fakes only; no sockets, GPIO, RF or real NVS.
#include <stdint.h>
#include <stddef.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#define CONFIG_ESP_WIFI_REMOTE_ENABLED 1
inline uint32_t fakeMillis=1000;
inline uint32_t millis(){return fakeMillis;}
struct ESPType {uint64_t getEfuseMac(){return 0x0123456789abULL;}uint32_t getFreeHeap(){return 100000;}};inline ESPType ESP;
using SemaphoreHandle_t=void*;using TaskHandle_t=void*;
struct Queue {size_t size;bool available=false;std::vector<unsigned char> bytes;};using QueueHandle_t=Queue*;
constexpr int pdTRUE=1,pdFALSE=0,pdPASS=1;
#define pdMS_TO_TICKS(ms) (ms)
inline void* xSemaphoreCreateMutex(){return new int(0);}
inline int xSemaphoreTake(void*,unsigned){return pdTRUE;}
inline int xSemaphoreGive(void*){return pdTRUE;}
inline void vSemaphoreDelete(void* h){delete static_cast<int*>(h);}
inline QueueHandle_t xQueueCreate(unsigned,size_t size){return new Queue{size,false,std::vector<unsigned char>(size)};}
inline void vQueueDelete(QueueHandle_t q){delete q;}
inline int xQueueOverwrite(QueueHandle_t q,const void* p){memcpy(q->bytes.data(),p,q->size);q->available=true;return pdTRUE;}
inline int xQueueReceive(QueueHandle_t q,void* p,unsigned){if(!q->available)return pdFALSE;memcpy(p,q->bytes.data(),q->size);q->available=false;return pdTRUE;}
inline void vTaskDelay(unsigned ms){fakeMillis+=ms;}
inline int xTaskCreate(void(*)(void*),const char*,unsigned,void*,unsigned,void** out){*out=reinterpret_cast<void*>(1);return pdPASS;}
inline std::map<std::string,std::vector<unsigned char>> nvs;
inline unsigned writes=0;inline bool writeOK=true;
struct Preferences {
 std::string space;bool begin(const char* n,bool){space=n;return true;}void end(){}
 size_t getBytesLength(const char*){return nvs[space].size();}
 size_t getBytes(const char*,void* p,size_t n){if(nvs[space].size()!=n)return 0;memcpy(p,nvs[space].data(),n);return n;}
 size_t putBytes(const char*,const void* p,size_t n){++writes;if(!writeOK)return 0;const auto* b=static_cast<const unsigned char*>(p);nvs[space]=std::vector<unsigned char>(b,b+n);return n;}
};
using esp_err_t=int;using esp_event_base_t=const char*;constexpr int ESP_OK=0,MQTT_TRANSPORT_OVER_TCP=1;
constexpr int MQTT_EVENT_ANY=-1,MQTT_EVENT_CONNECTED=1,MQTT_EVENT_DISCONNECTED=2,MQTT_EVENT_ERROR=3,MQTT_EVENT_DATA=4;
struct Error {int connect_return_code=0;};
struct esp_mqtt_event_t {int current_data_offset=0,total_data_len=0,data_len=0,topic_len=0;char* topic=nullptr;char* data=nullptr;Error* error_handle=nullptr;};
struct esp_mqtt_client_config_t {
 struct {struct {const char* hostname=nullptr;int port=0,transport=0;}address;}broker;
 struct {const char* client_id=nullptr;const char* username=nullptr;struct{const char* password=nullptr;}authentication;}credentials;
 struct{struct{const char* topic=nullptr;const char* msg=nullptr;int qos=0,retain=0;}last_will;int keepalive=0;}session;
 struct{int timeout_ms=0;bool disable_auto_reconnect=false;}network;
 struct{int stack_size=0;}task;struct{int size=0,out_size=0;}buffer;struct{uint64_t limit=0;}outbox;
};
using esp_mqtt_client_handle_t=void*;
struct Sent {std::string topic,payload;int qos,retain;bool queued;};
inline std::vector<Sent> sent;inline esp_mqtt_client_config_t mqttOptions;
inline unsigned creates=0,destroys=0,stops=0;inline bool mqttOK=true;inline std::string subscription;
inline void* esp_mqtt_client_init(const esp_mqtt_client_config_t* c){mqttOptions=*c;++creates;return reinterpret_cast<void*>(1);}
inline int esp_mqtt_client_register_event(void*,int,void(*)(void*,esp_event_base_t,int32_t,void*),void*){return ESP_OK;}
inline int esp_mqtt_client_start(void*){return ESP_OK;}
inline int esp_mqtt_client_stop(void*){++stops;return ESP_OK;}
inline int esp_mqtt_client_destroy(void*){++destroys;return ESP_OK;}
inline int esp_mqtt_client_get_outbox_size(void*){return 0;}
inline int esp_mqtt_client_enqueue(void*,const char* t,const char* p,int,int q,int r,bool){if(!mqttOK)return -1;sent.push_back({t,p,q,r,true});return int(sent.size());}
inline int esp_mqtt_client_publish(void*,const char* t,const char* p,int,int q,int r){if(!mqttOK)return -1;sent.push_back({t,p,q,r,false});return 0;}
inline int esp_mqtt_client_subscribe_single(void*,const char* t,int){subscription=t;return 1;}
