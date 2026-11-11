#include "panel_heating_settings.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs.h>

namespace panel_heating_settings {
namespace {
using namespace panel_heating;
constexpr uint32_t FORMAT=0x48540101;
constexpr int16_t ABSENT=INT16_MIN;
constexpr const char* SPACE="bp_heating";
constexpr const char* KEY="settings";
struct Record {
  uint32_t format,revision,stepMs;
  int16_t temperatures[10];
  uint8_t rooms[14],season,reheat,awayDiesel,reserved[3];
  uint32_t checksum;
};
static_assert(sizeof(Record)==56 && offsetof(Record,checksum)==52,"Heating record layout");
SemaphoreHandle_t mutex=nullptr;
bool known=false;
State current;
struct Guard {
  bool held=mutex && xSemaphoreTake(mutex,pdMS_TO_TICKS(50))==pdTRUE;
  ~Guard(){if(held)xSemaphoreGive(mutex);}
};

uint32_t digest(const Record& r){
  uint32_t hash=2166136261U;
  const auto* bytes=reinterpret_cast<const uint8_t*>(&r);
  for(size_t i=0;i<offsetof(Record,checksum);i++)hash=(hash^bytes[i])*16777619U;
  return hash;
}

constexpr float Config::* fields[]={
  &Config::roomOnC,&Config::roomRadiatorOffC,&Config::roomOffC,
  &Config::supplyTargetC,&Config::returnTargetC,&Config::returnRestartC,
  &Config::tankOnC,&Config::tankOffC,&Config::coilStartDeltaC,&Config::coilStopDeltaC
};

bool encode(const State& s,Record& r){
  r={};r.format=FORMAT;r.revision=s.revision;
  const auto& c=s.config;
  if(s.season!=Season::Unknown && s.season!=Season::Summer && s.season!=Season::Winter)return false;
  if(c.reheat!=Reheat::Unset && c.reheat!=Reheat::All && c.reheat!=Reheat::OneByOne)return false;
  r.season=uint8_t(s.season);r.reheat=uint8_t(c.reheat);r.awayDiesel=c.allowAwayDiesel?1:0;
  if(c.reheatStepMs>86400000U || (c.reheat==Reheat::OneByOne && !c.reheatStepMs))return false;
  r.stepMs=c.reheatStepMs;
  for(unsigned i=0;i<14;i++){
    if(c.roomForLoop[i]>14)return false;
    r.rooms[i]=c.roomForLoop[i];
  }
  for(unsigned i=0;i<10;i++){
    const float v=c.*fields[i];
    if(isnan(v) && i>=3){r.temperatures[i]=ABSENT;continue;}
    if(!isfinite(v) || v<(i<8?-55.0f:0.0f) || v>(i<8?125.0f:180.0f))return false;
    const long scaled=lroundf(v*10);
    if(fabsf(v-float(scaled)/10.0f)>0.0001f)return false;
    r.temperatures[i]=int16_t(scaled);
  }
  if(!(c.roomOnC<c.roomRadiatorOffC && c.roomRadiatorOffC<c.roomOffC) ||
     c.returnRestartC>=c.returnTargetC || c.tankOnC>=c.tankOffC ||
     c.coilStartDeltaC<=c.coilStopDeltaC)return false;
  r.checksum=digest(r);return true;
}

bool decode(const Record& r,State& out){
  if(r.format!=FORMAT || !r.revision || r.checksum!=digest(r))return false;
  State s{};s.revision=r.revision;s.season=Season(r.season);
  auto& c=s.config;c.reheat=Reheat(r.reheat);c.reheatStepMs=r.stepMs;
  c.allowAwayDiesel=r.awayDiesel!=0;
  memcpy(c.roomForLoop,r.rooms,sizeof(r.rooms));
  for(unsigned i=0;i<10;i++)c.*fields[i]=r.temperatures[i]==ABSENT?NAN:float(r.temperatures[i])/10.0f;
  Record checked{};
  if(!encode(s,checked)||memcmp(&checked,&r,sizeof(r))!=0)return false;
  out=s;return true;
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
  candidate.revision++;candidate.checksum=digest(candidate);
  State readback{};
  if(writeRecord(&candidate)&&readState(readback)&&matches(readback,candidate)){
    current=readback;return Result::Saved;
  }
  known=writeRecord(previous.revision?&previous:nullptr)&&readState(readback)&&matches(readback,previous);
  if(known)current=readback;
  return Result::Unavailable;
}
}
