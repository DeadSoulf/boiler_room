#include "panel_heating_settings.h"
#include "panel_heating_rust.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs.h>

namespace panel_heating_settings {
namespace {
using namespace panel_heating;
using Record = rust_abi::SettingsRecord;
constexpr const char* SPACE="bp_heating";
constexpr const char* KEY="settings";
SemaphoreHandle_t mutex=nullptr;
bool known=false;
State current;
struct Guard {
  bool held=mutex && xSemaphoreTake(mutex,pdMS_TO_TICKS(50))==pdTRUE;
  ~Guard(){if(held)xSemaphoreGive(mutex);}
};

bool encode(const State& state, Record& record) {
  const auto config = rust_abi::toRaw(state.config);
  return rust_abi::boiler_heating_settings_encode(&config, uint8_t(state.season),
                                                 state.revision, &record) != 0;
}

bool decode(const Record& record, State& state) {
  rust_abi::RawConfig config{};
  uint8_t season;
  uint32_t revision;
  if (!rust_abi::boiler_heating_settings_decode(&record, &config, &season, &revision)) return false;
  state.config = rust_abi::fromRaw(config);
  state.season = Season(season);
  state.revision = revision;
  return true;
}

bool readState(State& out){
  nvs_handle_t h;
  esp_err_t err=nvs_open(SPACE,NVS_READONLY,&h);
  if(err==ESP_ERR_NVS_NOT_FOUND){out=State{};return true;}
  if(err!=ESP_OK)return false;
  Record r{};size_t n=sizeof(r);
  err=nvs_get_blob(h,KEY,&r,&n);
  nvs_close(h);
  if(err==ESP_ERR_NVS_NOT_FOUND){out=State{};return true;}
  return err==ESP_OK && n==sizeof(r) && decode(r,out);
}

bool writeRecord(const Record* r){
  nvs_handle_t h;
  if(nvs_open(SPACE,NVS_READWRITE,&h)!=ESP_OK)return false;
  esp_err_t err=r?nvs_set_blob(h,KEY,r,sizeof(*r)):nvs_erase_key(h,KEY);
  if(!r&&err==ESP_ERR_NVS_NOT_FOUND)err=ESP_OK;
  if(err==ESP_OK)err=nvs_commit(h);
  nvs_close(h);return err==ESP_OK;
}
bool matches(const State& s,const Record& expected){
  Record actual{};return encode(s,actual)&&memcmp(&actual,&expected,sizeof(actual))==0;
}
}
bool begin(){
  if(!mutex)mutex=xSemaphoreCreateMutex();
  Guard guard;if(!guard.held)return false;
  known=readState(current);return known;
}
bool snapshot(State& out){
  Guard guard;if(!guard.held)return false;
  if(!known)known=readState(current);
  if(known)out=current;
  return known;
}
Result save(uint32_t expectedRevision,const State& next){
  Guard guard;if(!guard.held)return Result::Unavailable;
  if(!readState(current)){known=false;return Result::Unavailable;}
  known=true;
  if(current.revision!=expectedRevision)return Result::Conflict;
  Record previous{},candidate{};
  if(!encode(current,previous))return Result::Unavailable;
  State changed=next;changed.revision=expectedRevision;
  if(!encode(changed,candidate))return Result::Invalid;
  if(!memcmp(&previous,&candidate,sizeof(candidate)))return Result::Saved;
  if(expectedRevision==UINT32_MAX)return Result::Unavailable;
  ++changed.revision;
  if(!encode(changed,candidate))return Result::Invalid;
  State readback{};
  if(writeRecord(&candidate)&&readState(readback)&&matches(readback,candidate)){
    current=readback;return Result::Saved;
  }
  known=writeRecord(current.revision?&previous:nullptr)&&readState(readback)&&matches(readback,previous);
  if(known)current=readback;
  return Result::Unavailable;
}
}
