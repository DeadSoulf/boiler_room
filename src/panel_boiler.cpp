#include "panel_boiler.h"
#include "panel_targets.h"
#include "panel_types.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs.h>

namespace {
using Result=panel_boiler::Result;
enum class Format:uint16_t { Thresholds=0x4201, Temperature=0x5401 };
struct Store {
  const char* space;
  const char* key;
  Format format;
  bool known=false;
  uint64_t word=0;
};
Store dhw{"bp_dhw","thresholds",Format::Thresholds},electric{"bp_electric","thresholds",Format::Thresholds};
Store temperatures[]={
  {"bp_temp_targets","room",Format::Temperature},
  {"bp_temp_targets","outside",Format::Temperature},
  {"bp_temp_targets","supply",Format::Temperature},
  {"bp_temp_targets","fuel_tank",Format::Temperature},
  {"bp_temp_targets","return",Format::Temperature},
  {"bp_temp_targets","fuel_outlet",Format::Temperature}
};
static_assert(sizeof(temperatures)/sizeof(temperatures[0])==panel::CHANNELS,"One target per measured temperature");
SemaphoreHandle_t boilerMutex=nullptr,targetMutex=nullptr;

struct Guard {
  SemaphoreHandle_t mutex;
  bool held;
  explicit Guard(SemaphoreHandle_t m):mutex(m),held(m && xSemaphoreTake(m,pdMS_TO_TICKS(50))==pdTRUE){}
  ~Guard(){if(held)xSemaphoreGive(mutex);}
};
bool validPair(int on,int off){return on>=0 && on<off && off<=99;}
uint32_t revisionOf(uint64_t word){return uint32_t(word>>16);}
Store* forAppliance(panel_boiler::Target target){
  switch(target){
    case panel_boiler::Target::Dhw:return &dhw;
    case panel_boiler::Target::Electric:return &electric;
  }
  return nullptr;
}
// One native NVS integer keeps the value and its revision in one CRC-protected entry.
// Word zero in memory means no stored key. Zero on flash is invalid.
bool read(const Store& store,uint64_t& word){
  word=0;
  nvs_handle_t handle;
  esp_err_t err=nvs_open(store.space,NVS_READONLY,&handle);
  if(err==ESP_ERR_NVS_NOT_FOUND)return true;
  if(err!=ESP_OK)return false;
  uint64_t raw=0;
  err=nvs_get_u64(handle,store.key,&raw);
  nvs_close(handle);
  if(err==ESP_ERR_NVS_NOT_FOUND)return true;
  if(err!=ESP_OK || uint16_t(raw>>48)!=uint16_t(store.format) || !revisionOf(raw))return false;
  const uint16_t value=uint16_t(raw);
  if(store.format==Format::Thresholds){
    if(!validPair(value>>8,value&0xff))return false;
  }else if(value>panel_targets::MAX_C-panel_targets::MIN_C)return false;
  word=raw;return true;
}
bool write(const Store& store,uint64_t word){
  nvs_handle_t handle;
  if(nvs_open(store.space,NVS_READWRITE,&handle)!=ESP_OK)return false;
  esp_err_t err=word?nvs_set_u64(handle,store.key,word):nvs_erase_key(handle,store.key);
  if(!word && err==ESP_ERR_NVS_NOT_FOUND)err=ESP_OK;
  if(err==ESP_OK)err=nvs_commit(handle);
  nvs_close(handle);return err==ESP_OK;
}
bool reload(Store& store){
  store.known=read(store,store.word);
  return store.known;
}
Result saveValue(Store& store,uint32_t expected,uint16_t value){
  if(!reload(store))return Result::Unavailable;
  const uint64_t before=store.word;
  const uint32_t revision=revisionOf(before);
  if(revision!=expected)return Result::Conflict;
  if(before && uint16_t(before)==value)return Result::Saved;
  if(revision==UINT32_MAX)return Result::Unavailable;
  const uint64_t next=(uint64_t(store.format)<<48)|(uint64_t(revision+1)<<16)|value;
  if(write(store,next) && reload(store) && store.word==next)return Result::Saved;
  // The write may have committed before reporting failure. Restore and verify the old word.
  store.known=write(store,before) && reload(store) && store.word==before;
  return Result::Unavailable;
}
} // namespace

namespace panel_boiler {
bool begin(){
  if(!boilerMutex)boilerMutex=xSemaphoreCreateMutex();
  Guard lock(boilerMutex);if(!lock.held)return false;
  const bool a=reload(dhw),b=reload(electric);
  return a && b;
}
bool snapshot(Settings& out,Target target){
  out={};
  auto* store=forAppliance(target);
  Guard lock(boilerMutex);if(!lock.held||!store)return false;
  if(!store->known&&!reload(*store))return false;
  const uint64_t word=store->word;
  out.configured=word!=0;out.revision=revisionOf(word);
  out.onC=uint8_t(word>>8);out.offC=uint8_t(word);
  return true;
}
Result save(uint32_t expectedRevision,int onC,int offC,Target target){
  auto* store=forAppliance(target);
  if(!store||!validPair(onC,offC))return Result::Invalid;
  Guard lock(boilerMutex);if(!lock.held)return Result::Unavailable;
  return saveValue(*store,expectedRevision,uint16_t((onC<<8)|offC));
}
} // namespace panel_boiler

namespace panel_targets {
bool begin(){
  if(!targetMutex)targetMutex=xSemaphoreCreateMutex();
  Guard lock(targetMutex);if(!lock.held)return false;
  bool allKnown=true;
  for(auto& store:temperatures)if(!reload(store))allKnown=false;
  return allKnown;
}
bool snapshot(unsigned channel,Setting& out){
  out={};
  if(channel>=panel::CHANNELS)return false;
  Guard lock(targetMutex);if(!lock.held)return false;
  auto& store=temperatures[channel];
  if(!store.known&&!reload(store))return false;
  if(store.word){
    out.configured=true;out.revision=revisionOf(store.word);
    out.valueC=MIN_C+int(uint16_t(store.word));
  }
  return true;
}
Result save(unsigned channel,uint32_t expectedRevision,int valueC){
  // Keep legacy NVS bytes intact but refuse new outdoor setpoint writes.
  if(channel==OUTDOOR_CHANNEL||channel>=panel::CHANNELS||valueC<MIN_C||valueC>MAX_C)return Result::Invalid;
  Guard lock(targetMutex);if(!lock.held)return Result::Unavailable;
  return saveValue(temperatures[channel],expectedRevision,uint16_t(valueC-MIN_C));
}
} // namespace panel_targets
