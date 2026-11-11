#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace fixture {
inline std::mutex lock;
inline bool denyLock=false, space=false, key=false, badRead=false;
inline bool badCommitOnce=false, failVerifyOnce=false;
inline uint64_t word=0;
inline unsigned writes=0;
}
using SemaphoreHandle_t=std::mutex*;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return &fixture::lock;}
inline int pdMS_TO_TICKS(int ms){return ms;}
constexpr int pdTRUE=1;
inline int xSemaphoreTake(SemaphoreHandle_t m,int){if(fixture::denyLock)return 0;m->lock();return pdTRUE;}
inline void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
using nvs_handle_t=int;
using esp_err_t=int;
constexpr int ESP_OK=0, ESP_FAIL=-1, ESP_ERR_NVS_NOT_FOUND=0x1102, NVS_READONLY=1, NVS_READWRITE=2;
inline int nvs_open(const char* space,int mode,nvs_handle_t* h){
  assert(!strcmp(space,"bp_dhw"));
  if(mode==NVS_READONLY && !fixture::space)return ESP_ERR_NVS_NOT_FOUND;
  if(mode==NVS_READWRITE)fixture::space=true;
  *h=mode;return ESP_OK;
}
inline int nvs_get_u64(nvs_handle_t h,const char* key,uint64_t* value){
  assert(h==NVS_READONLY && !strcmp(key,"thresholds"));
  if(fixture::badRead)return ESP_FAIL;
  if(!fixture::key)return ESP_ERR_NVS_NOT_FOUND;
  *value=fixture::word;
  if(fixture::failVerifyOnce && fixture::writes){fixture::failVerifyOnce=false;*value^=1ULL<<63;}
  return ESP_OK;
}
inline int nvs_set_u64(nvs_handle_t h,const char* key,uint64_t value){
  assert(h==NVS_READWRITE && !strcmp(key,"thresholds"));
  fixture::key=true;fixture::word=value;++fixture::writes;return ESP_OK;
}
inline int nvs_erase_key(nvs_handle_t h,const char* key){
  assert(h==NVS_READWRITE && !strcmp(key,"thresholds"));
  fixture::key=false;return ESP_OK;
}
inline int nvs_commit(nvs_handle_t h){
  assert(h==NVS_READWRITE);
  if(fixture::badCommitOnce){fixture::badCommitOnce=false;return ESP_FAIL;}
  return ESP_OK;
}
inline void nvs_close(nvs_handle_t){}
