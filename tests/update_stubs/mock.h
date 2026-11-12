#pragma once
// HOST fault injection only. These fakes test the actual adapter's ordering and
// refusal paths; they make no claim about hardware, TLS or SHA implementation.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <strings.h>
#include <vector>
#define CONFIG_IDF_TARGET_ESP32P4 1
#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1
#define CONFIG_MBEDTLS_CERTIFICATE_BUNDLE 1
inline uint32_t fakeMillis=12000;
inline uint32_t millis(){return fakeMillis;}
using SemaphoreHandle_t=void*;using TaskHandle_t=void*;
constexpr int pdTRUE=1,pdFALSE=0,pdPASS=1;
#define pdMS_TO_TICKS(ms) (ms)
inline void* xSemaphoreCreateMutex(){return new int(0);}
inline int xSemaphoreTake(void*,unsigned){return pdTRUE;}
inline int xSemaphoreGive(void*){return pdTRUE;}
inline void vSemaphoreDelete(void* p){delete static_cast<int*>(p);}
struct Queue {size_t size;bool has=false;std::vector<uint8_t> data;};using QueueHandle_t=Queue*;
inline QueueHandle_t xQueueCreate(unsigned,size_t n){return new Queue{n,false,std::vector<uint8_t>(n)};}
inline void vQueueDelete(QueueHandle_t q){delete q;}
inline int xQueueSend(QueueHandle_t q,const void* p,unsigned){if(q->has)return pdFALSE;memcpy(q->data.data(),p,q->size);q->has=true;return pdTRUE;}
inline int xQueueReceive(QueueHandle_t q,void* p,unsigned){if(!q->has)return pdFALSE;memcpy(p,q->data.data(),q->size);q->has=false;return pdTRUE;}
inline void vTaskDelay(unsigned ms){fakeMillis+=ms;}
inline int xTaskCreate(void(*)(void*),const char*,unsigned,void*,unsigned,void** p){*p=reinterpret_cast<void*>(1);return pdPASS;}
inline std::map<std::string,std::vector<uint8_t>> nvs;
inline unsigned nvsWrites=0,failNvsWrite=0;
inline std::function<void(unsigned)> persistenceHook;
struct Preferences {
  std::string name;
  bool begin(const char* s,bool){name=s;return true;}void end(){}
  size_t getBytesLength(const char*){return nvs[name].size();}
  size_t getBytes(const char*,void* p,size_t n){if(nvs[name].size()!=n)return 0;memcpy(p,nvs[name].data(),n);return n;}
  size_t putBytes(const char*,const void* p,size_t n){
    assert(name=="bp-update");++nvsWrites;if(persistenceHook)persistenceHook(nvsWrites);if(nvsWrites==failNvsWrite)return 0;
    const auto* b=static_cast<const uint8_t*>(p);nvs[name]=std::vector<uint8_t>(b,b+n);return n;
  }
};
using esp_err_t=int;constexpr int ESP_OK=0,ESP_FAIL=-1;
enum esp_ota_img_states_t {ESP_OTA_IMG_NEW=0,ESP_OTA_IMG_PENDING_VERIFY=1,ESP_OTA_IMG_VALID=2,ESP_OTA_IMG_INVALID=3,ESP_OTA_IMG_ABORTED=4,ESP_OTA_IMG_UNDEFINED=-1};
constexpr int ESP_PARTITION_TYPE_APP=0,ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_APP_OTA_0=0x10,ESP_PARTITION_SUBTYPE_APP_OTA_1=0x11,ESP_PARTITION_SUBTYPE_DATA_OTA=0;
struct esp_partition_t {uint32_t address,size;char label[17];};
inline esp_partition_t fakePartitions[]={{0x10000,0x640000,"app0"},{0x650000,0x640000,"app1"},{0xe000,0x2000,"otadata"}};
inline unsigned runningIndex=0,bootIndex=0;
inline esp_ota_img_states_t partitionStates[]={ESP_OTA_IMG_VALID,ESP_OTA_IMG_UNDEFINED};
inline std::vector<uint8_t> images[2],otaData(8192,255),partitionTable(4096,255);
inline std::vector<std::string> trace;
inline std::function<void(const char*)> faultHook;
inline bool wifi=true,badBootloader=false,failVerifyTarget=false,failMarkValid=false,rollbackPossible=true;
inline bool failBegin=false,failEnd=false,failBoot=false,bootFailureAfterSelect=false,failTimer=false;
inline bool badBootReadbackOnce=false,bootReadbackFault=false,failTimerStart=false;
inline unsigned otaBegins=0,otaWrites=0,otaEnds=0,otaAborts=0,bootSelections=0,restarts=0,failWriteAt=0;
inline unsigned targetIndex=1;
inline uint32_t imageSize=100000;
inline void put32(uint8_t* p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=uint8_t(n>>(8*i));}
inline void makeTable(){
  std::fill(partitionTable.begin(),partitionTable.end(),255);
  const uint32_t fields[][4]={{1,2,0x9000,0x5000},{1,0,0xe000,0x2000},{0,0x10,0x10000,0x640000},{0,0x11,0x650000,0x640000},{1,0x82,0xc90000,0x360000},{1,3,0xff0000,0x10000}};
  const char* labels[]={"nvs","otadata","app0","app1","spiffs","coredump"};
  for(unsigned i=0;i<6;i++){
    auto* p=partitionTable.data()+32*i;memset(p,0,32);p[0]=0xaa;p[1]=0x50;p[2]=uint8_t(fields[i][0]);p[3]=uint8_t(fields[i][1]);
    put32(p+4,fields[i][2]);put32(p+8,fields[i][3]);memcpy(p+12,labels[i],strlen(labels[i]));
  }
  partitionTable[192]=partitionTable[193]=0xeb;
  const uint8_t md5[]={0xb3,0x2d,0xcd,0xb8,0x3b,0x16,0x74,0xaf,0x27,0x76,0x8c,0x92,0xb1,0xf8,0x8a,0x62};
  memcpy(partitionTable.data()+208,md5,sizeof(md5));
}
inline void fakeDigest(const uint8_t* data,size_t size,uint8_t* out);
inline std::vector<uint8_t> makeImage(size_t n=100000){
  assert(n>=policy::MIN_FIRMWARE_BYTES+32);
  std::vector<uint8_t> image(n,0x5a);memset(image.data(),0,36);image[0]=0xe9;image[1]=4;image[3]=0x40;image[12]=18;
  image[15]=103;image[17]=image[18]=255;image[23]=1;image[29]=1;
  image[32]=0x32;image[33]=0x54;image[34]=0xcd;image[35]=0xab;
  fakeDigest(image.data(),image.size()-32,image.data()+image.size()-32);
  return image;
}
// Deliberately non-cryptographic identity hash; ESP-IDF owns the real SHA-256.
inline void fakeDigest(const uint8_t* data,size_t size,uint8_t* out){
  uint32_t h=2166136261U;for(size_t i=0;i<size;i++){h^=data[i];h*=16777619U;}
  for(unsigned i=0;i<32;i++){h^=i;h*=16777619U;out[i]=uint8_t(h>>24);}
}
struct mbedtls_sha256_context {std::vector<uint8_t> data;};
inline void mbedtls_sha256_init(mbedtls_sha256_context*){}
inline int mbedtls_sha256_starts(mbedtls_sha256_context* p,int){p->data.clear();return 0;}
inline int mbedtls_sha256_update(mbedtls_sha256_context* p,const uint8_t* b,size_t n){p->data.insert(p->data.end(),b,b+n);return 0;}
inline int mbedtls_sha256_finish(mbedtls_sha256_context* p,uint8_t* out){
  if(p->data.size()==21616&&!badBootloader){
    const char* expected="6a88d429ece0ae20f8ae18493b9f3dee05c35f4abaa22f0320e44c34d30631c2";
    auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};
    for(unsigned i=0;i<32;i++)out[i]=uint8_t((digit(expected[i*2])<<4)|digit(expected[i*2+1]));
  }else fakeDigest(p->data.data(),p->data.size(),out);
  return 0;
}
inline void mbedtls_sha256_free(mbedtls_sha256_context*){}
struct esp_chip_info_t {int model=0;uint16_t revision=0;};constexpr int CHIP_ESP32P4=18;
inline void esp_chip_info(esp_chip_info_t* p){p->model=CHIP_ESP32P4;p->revision=103;}
inline void* esp_flash_default_chip=nullptr;
inline int esp_flash_get_size(void*,uint32_t* n){*n=0x1000000;return ESP_OK;}
inline int esp_flash_read(void*,void* out,uint32_t address,size_t n){
  if(address==0x8000&&n==4096){memcpy(out,partitionTable.data(),n);return ESP_OK;}
  if(address>=0x2000&&address+n<=0x2000+21616){memset(out,0,n);return ESP_OK;}return ESP_FAIL;
}
inline const esp_partition_t* esp_partition_find_first(int type,int subtype,const char*){
  if(type==ESP_PARTITION_TYPE_APP&&subtype==0x10)return &fakePartitions[0];
  if(type==ESP_PARTITION_TYPE_APP&&subtype==0x11)return &fakePartitions[1];
  if(type==ESP_PARTITION_TYPE_DATA&&subtype==0)return &fakePartitions[2];
  return nullptr;
}
inline const esp_partition_t* esp_ota_get_running_partition(){return &fakePartitions[runningIndex];}
inline const esp_partition_t* esp_ota_get_boot_partition(){if(bootReadbackFault){bootReadbackFault=false;return nullptr;}return &fakePartitions[bootIndex];}
inline const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*){return &fakePartitions[runningIndex^1U];}
inline int esp_ota_get_state_partition(const esp_partition_t* p,esp_ota_img_states_t* s){
  if(!p||p==&fakePartitions[2])return ESP_FAIL;
  *s=partitionStates[p==&fakePartitions[0]?0:1];return ESP_OK;
}
inline int esp_partition_read(const esp_partition_t* p,size_t at,void* out,size_t n){
  const auto& data=p==&fakePartitions[2]?otaData:images[p==&fakePartitions[0]?0:1];
  if(at+n>data.size())return ESP_FAIL;
  memcpy(out,data.data()+at,n);return ESP_OK;
}
inline int esp_partition_get_sha256(const esp_partition_t* p,uint8_t* out){
  const auto& data=images[p==&fakePartitions[0]?0:1];if(data.size()<64)return ESP_FAIL;
  uint8_t digest[32];fakeDigest(data.data(),data.size()-32,digest);
  if(memcmp(digest,data.data()+data.size()-32,32))return ESP_FAIL;
  memcpy(out,digest,32);return ESP_OK;
}
struct esp_partition_pos_t {uint32_t offset,size;};
struct esp_image_metadata_t {uint32_t image_len=0;struct {uint8_t hash_appended=0;} image;uint8_t image_digest[32]={};};
constexpr int ESP_IMAGE_VERIFY=0;
inline int esp_image_verify(int,const esp_partition_pos_t* p,esp_image_metadata_t* result){
  const unsigned index=p->offset==0x10000?0:1;
  if(images[index].size()<64||images[index][0]!=0xe9||(failVerifyTarget&&index!=runningIndex))return ESP_FAIL;
  uint8_t digest[32];fakeDigest(images[index].data(),images[index].size()-32,digest);
  if(memcmp(digest,images[index].data()+images[index].size()-32,32))return ESP_FAIL;
  result->image_len=uint32_t(images[index].size());
  result->image.hash_appended=images[index][23];
  memcpy(result->image_digest,digest,32);
  return ESP_OK;
}
inline uint32_t esp_rom_crc32_le(uint32_t crc,const uint8_t* data,uint32_t size){
  for(uint32_t i=0;i<size;i++){crc^=data[i];for(unsigned b=0;b<8;b++)crc=(crc>>1)^((crc&1)?0xedb88320U:0);}return crc;
}
using esp_ota_handle_t=unsigned;
inline int esp_ota_begin(const esp_partition_t* p,uint32_t,esp_ota_handle_t* h){
  ++otaBegins;trace.push_back("begin");if(failBegin)return ESP_FAIL;targetIndex=p==&fakePartitions[0]?0:1;
  assert(targetIndex!=runningIndex);images[targetIndex].clear();partitionStates[targetIndex]=ESP_OTA_IMG_UNDEFINED;*h=1;return ESP_OK;
}
inline int esp_ota_write(esp_ota_handle_t,const void* p,size_t n){
  ++otaWrites;trace.push_back("write");if(otaWrites==failWriteAt)return ESP_FAIL;
  const auto* b=static_cast<const uint8_t*>(p);images[targetIndex].insert(images[targetIndex].end(),b,b+n);
  if(faultHook)faultHook("write");
  return ESP_OK;
}
inline int esp_ota_abort(esp_ota_handle_t){++otaAborts;trace.push_back("abort");return ESP_OK;}
inline int esp_ota_end(esp_ota_handle_t){
  ++otaEnds;trace.push_back("end");if(faultHook)faultHook("end");
  if(failEnd||failVerifyTarget)return ESP_FAIL;
  esp_partition_pos_t pos{fakePartitions[targetIndex].address,fakePartitions[targetIndex].size};
  esp_image_metadata_t meta{};
  return esp_image_verify(ESP_IMAGE_VERIFY,&pos,&meta);
}
inline int esp_ota_set_boot_partition(const esp_partition_t* p){
  ++bootSelections;trace.push_back("select");if(failBoot&&!bootFailureAfterSelect)return ESP_FAIL;
  bootIndex=p==&fakePartitions[0]?0:1;partitionStates[bootIndex]=ESP_OTA_IMG_NEW;
  if(badBootReadbackOnce&&bootIndex!=runningIndex){bootReadbackFault=true;badBootReadbackOnce=false;}
  return failBoot?ESP_FAIL:ESP_OK;
}
inline int esp_ota_mark_app_valid_cancel_rollback(){
  trace.push_back("valid");if(failMarkValid)return ESP_FAIL;partitionStates[runningIndex]=ESP_OTA_IMG_VALID;return ESP_OK;
}
inline bool esp_ota_check_rollback_is_possible(){return rollbackPossible;}
struct Restart{};
inline void esp_restart(){++restarts;trace.push_back("restart");throw Restart{};}
struct Timer {void(*callback)(void*)=nullptr;void* arg=nullptr;bool armed=false;uint64_t delay=0;};using esp_timer_handle_t=Timer*;
struct esp_timer_create_args_t {void(*callback)(void*)=nullptr;void* arg=nullptr;const char* name=nullptr;};
inline int esp_timer_create(const esp_timer_create_args_t* a,esp_timer_handle_t* h){if(failTimer)return ESP_FAIL;*h=new Timer{a->callback,a->arg,false,0};return ESP_OK;}
inline int esp_timer_start_once(esp_timer_handle_t h,uint64_t n){if(failTimerStart)return ESP_FAIL;h->armed=true;h->delay=n;return ESP_OK;}
inline int esp_timer_stop(esp_timer_handle_t h){h->armed=false;return ESP_OK;}
inline int esp_crt_bundle_attach(void*){return ESP_OK;}
constexpr int HTTP_EVENT_ON_HEADER=1,HTTP_METHOD_GET=0,HTTP_TRANSPORT_OVER_SSL=1;
struct esp_http_client_event_t {int event_id=0;void* user_data=nullptr;char* header_key=nullptr;char* header_value=nullptr;};
struct esp_http_client_config_t {
  const char* url=nullptr;int method=0,transport_type=0,timeout_ms=0,buffer_size=0,buffer_size_tx=0;
  int(*crt_bundle_attach)(void*)=nullptr;bool disable_auto_redirect=false,keep_alive_enable=false;
  const char* user_agent=nullptr;int(*event_handler)(esp_http_client_event_t*)=nullptr;void* user_data=nullptr;
};
struct Response {std::vector<uint8_t> body;int status=200;bool contentLength=true,complete=true;int readFailAfter=-1;std::vector<std::pair<std::string,std::string>> headers;};
inline std::map<std::string,Response> responses;
inline std::vector<std::string> requestedUrls;
struct Client {esp_http_client_config_t config;std::string url;size_t at=0;Response response;};using esp_http_client_handle_t=Client*;
inline Client* esp_http_client_init(const esp_http_client_config_t* config){
  assert(config->transport_type==HTTP_TRANSPORT_OVER_SSL&&config->crt_bundle_attach==esp_crt_bundle_attach&&config->disable_auto_redirect);
  assert(config->buffer_size_tx>=2048);requestedUrls.push_back(config->url);
  auto found=responses.find(config->url);if(found==responses.end())return nullptr;
  return new Client{*config,config->url,0,found->second};
}
inline int esp_http_client_set_header(Client*,const char* key,const char*){assert(strcasecmp(key,"Authorization")&&strcasecmp(key,"Cookie"));return ESP_OK;}
inline int esp_http_client_open(Client*,int){return ESP_OK;}
inline int64_t esp_http_client_fetch_headers(Client* c){
  auto pairs=c->response.headers;
  if(c->response.contentLength)pairs.push_back({"Content-Length",std::to_string(c->response.body.size())});
  for(auto& pair:pairs){esp_http_client_event_t event{HTTP_EVENT_ON_HEADER,c->config.user_data,pair.first.data(),pair.second.data()};c->config.event_handler(&event);}
  return c->response.contentLength?int64_t(c->response.body.size()):0;
}
inline int esp_http_client_get_status_code(Client* c){return c->response.status;}
inline int esp_http_client_read(Client* c,char* out,int n){
  if(c->response.readFailAfter>=0&&c->at>=size_t(c->response.readFailAfter))return -1;
  const size_t count=std::min(size_t(n),c->response.body.size()-c->at);
  if(count)memcpy(out,c->response.body.data()+c->at,count);
  c->at+=count;return int(count);
}
inline bool esp_http_client_is_complete_data_received(Client* c){return c->response.complete&&c->at==c->response.body.size();}
inline int esp_http_client_close(Client*){return ESP_OK;}
inline int esp_http_client_cleanup(Client* c){delete c;return ESP_OK;}
