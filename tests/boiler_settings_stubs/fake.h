#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace fixture {
inline std::mutex lock;
inline bool denyLock=false;
struct Slot {
  bool space=false,key=false,badRead=false;
  bool badCommitOnce=false,failVerifyOnce=false;
  uint64_t word=0;
  unsigned writes=0;
};
inline Slot dhw,electric,targets[6];
inline const char* targetKeys[]={"room","outside","supply","fuel_tank","return","fuel_outlet"};
}
using SemaphoreHandle_t=std::mutex*;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return &fixture::lock;}
inline int pdMS_TO_TICKS(int ms){return ms;}
constexpr int pdTRUE=1;
inline int xSemaphoreTake(SemaphoreHandle_t m,int){if(fixture::denyLock)return 0;m->lock();return pdTRUE;}
inline void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
struct nvs_handle_t {fixture::Slot* slot;int mode;bool targets=false;};
using esp_err_t=int;
constexpr int ESP_OK=0, ESP_FAIL=-1, ESP_ERR_NVS_NOT_FOUND=0x1102, NVS_READONLY=1, NVS_READWRITE=2;
inline int nvs_open(const char* space,int mode,nvs_handle_t* h){
  if(!strcmp(space,"bp_temp_targets")){*h={nullptr,mode,true};return ESP_OK;}
  assert(!strcmp(space,"bp_dhw") || !strcmp(space,"bp_electric"));
  auto& slot=!strcmp(space,"bp_dhw")?fixture::dhw:fixture::electric;
  if(mode==NVS_READONLY && !slot.space)return ESP_ERR_NVS_NOT_FOUND;
  if(mode==NVS_READWRITE)slot.space=true;
  *h={&slot,mode};return ESP_OK;
}
inline fixture::Slot& getSlot(nvs_handle_t h,const char* key){
  if(h.targets){
    for(unsigned i=0;i<6;++i)if(!strcmp(key,fixture::targetKeys[i]))return fixture::targets[i];
    assert(false);
  }
  assert(!strcmp(key,"thresholds"));return *h.slot;
}
inline int nvs_get_u64(nvs_handle_t h,const char* key,uint64_t* value){
  assert(h.mode==NVS_READONLY);
  auto& slot=getSlot(h,key);
  if(slot.badRead)return ESP_FAIL;
  if(!slot.key)return ESP_ERR_NVS_NOT_FOUND;
  *value=slot.word;
  if(slot.failVerifyOnce && slot.writes){slot.failVerifyOnce=false;*value^=1ULL<<63;}
  return ESP_OK;
}
inline int nvs_set_u64(nvs_handle_t& h,const char* key,uint64_t value){
  assert(h.mode==NVS_READWRITE);
  auto& slot=getSlot(h,key);h.slot=&slot;slot.key=true;slot.word=value;++slot.writes;return ESP_OK;
}
inline int nvs_erase_key(nvs_handle_t& h,const char* key){
  assert(h.mode==NVS_READWRITE);
  auto& slot=getSlot(h,key);h.slot=&slot;slot.key=false;return ESP_OK;
}
inline int nvs_commit(nvs_handle_t h){
  assert(h.mode==NVS_READWRITE);
  if(h.slot && h.slot->badCommitOnce){h.slot->badCommitOnce=false;return ESP_FAIL;}
  return ESP_OK;
}
inline void nvs_close(nvs_handle_t){}
