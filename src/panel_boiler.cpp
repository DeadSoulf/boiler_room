#include "panel_boiler.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs.h>

namespace panel_boiler {
namespace {
constexpr char SPACE[]="bp_dhw", KEY[]="thresholds";
constexpr uint16_t FORMAT=0x4201;
SemaphoreHandle_t mutex=nullptr;
bool known=false;
Settings current;

struct Guard {
  bool held=mutex && xSemaphoreTake(mutex,pdMS_TO_TICKS(50))==pdTRUE;
  ~Guard(){if(held)xSemaphoreGive(mutex);}
};
bool valid(int on,int off){return on>=0 && off<=99 && on<off;}
// One native NVS integer: revision and both setpoints share one CRC-protected
// entry. Missing key is distinct from a failed read, wrong type or bad format.
uint64_t encode(const Settings& s){
  return (uint64_t(FORMAT)<<48) | (uint64_t(s.revision)<<16) | (uint64_t(s.onC)<<8) | s.offC;
}
bool read(Settings& out){
  out={};
  nvs_handle_t handle;
  esp_err_t err=nvs_open(SPACE,NVS_READONLY,&handle);
  if(err==ESP_ERR_NVS_NOT_FOUND)return true; // fresh namespace
  if(err!=ESP_OK)return false;
  uint64_t word=0;
  err=nvs_get_u64(handle,KEY,&word);
  nvs_close(handle);
  if(err==ESP_ERR_NVS_NOT_FOUND)return true;
  if(err!=ESP_OK || uint16_t(word>>48)!=FORMAT)return false;
  const uint32_t revision=uint32_t(word>>16);
  const uint8_t on=uint8_t(word>>8),off=uint8_t(word);
  if(!revision || !valid(on,off))return false;
  out.configured=true;out.revision=revision;out.onC=on;out.offC=off;
  return true;
}
bool write(const Settings& s){
  nvs_handle_t handle;
  if(nvs_open(SPACE,NVS_READWRITE,&handle)!=ESP_OK)return false;
  esp_err_t err=s.configured?nvs_set_u64(handle,KEY,encode(s)):nvs_erase_key(handle,KEY);
  if(!s.configured && err==ESP_ERR_NVS_NOT_FOUND)err=ESP_OK;
  if(err==ESP_OK)err=nvs_commit(handle);
  nvs_close(handle);
  return err==ESP_OK;
}
bool same(const Settings& a,const Settings& b){
  return a.configured==b.configured && a.revision==b.revision && a.onC==b.onC && a.offC==b.offC;
}
} // namespace

bool begin(){
  if(!mutex)mutex=xSemaphoreCreateMutex();
  Guard lock;if(!lock.held)return false;
  known=read(current);return known;
}
bool snapshot(Settings& out){
  out={};Guard lock;if(!lock.held)return false;
  // Cached reads avoid flash traffic during normal UI refreshes.
  if(!known)known=read(current);
  if(!known)return false;
  out=current;return true;
}
Result save(uint32_t expectedRevision,int onC,int offC){
  if(!valid(onC,offC))return Result::Invalid;
  Guard lock;if(!lock.held)return Result::Unavailable;
  Settings before;
  known=read(before);
  if(!known)return Result::Unavailable;
  current=before;
  if(before.revision!=expectedRevision)return Result::Conflict;
  if(before.configured && before.onC==onC && before.offC==offC)return Result::Saved;
  if(before.revision==UINT32_MAX)return Result::Unavailable; // never reuse a revision
  Settings next=before;
  next.configured=true;next.onC=uint8_t(onC);next.offC=uint8_t(offC);++next.revision;
  Settings check;
  if(write(next) && read(check) && same(check,next)){
    current=check;return Result::Saved;
  }
  // A commit error may be ambiguous. Try to restore the previous pair, but do
  // not report either value as confirmed if storage is still unreadable.
  known=write(before) && read(check) && same(check,before);
  return Result::Unavailable;
}
} // namespace panel_boiler
