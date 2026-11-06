#pragma once
// Host-only NVS/RTOS faults; the configuration implementation is compiled unchanged.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
namespace config_host {
struct Mutex { bool held=false; };
inline std::map<std::string,std::vector<uint8_t>> nvs;
inline unsigned writes=0,removes=0;
inline bool lockAvailable=true,createLock=true;
inline bool failWrite=false,writeThenFail=false,partialWrite=false,failAllWrites=false;
inline bool failRead=false,corruptRead=false,failWriteBegin=false;
inline std::function<void()> beforeTake,afterWrite;
inline void resetFaults(){
  lockAvailable=createLock=true;
  failWrite=writeThenFail=partialWrite=failAllWrites=false;
  failRead=corruptRead=failWriteBegin=false;
  beforeTake=afterWrite=nullptr;
}
}
using SemaphoreHandle_t=config_host::Mutex*;
constexpr int pdTRUE=1;
#define pdMS_TO_TICKS(ms) (ms)
inline SemaphoreHandle_t xSemaphoreCreateMutex(){
  return config_host::createLock?new config_host::Mutex:nullptr;
}
inline int xSemaphoreTake(SemaphoreHandle_t mutex,unsigned){
  if(config_host::beforeTake){
    auto callback=std::move(config_host::beforeTake);config_host::beforeTake=nullptr;callback();
  }
  if(!config_host::lockAvailable||!mutex||mutex->held)return 0;
  mutex->held=true;return pdTRUE;
}
inline void xSemaphoreGive(SemaphoreHandle_t mutex){mutex->held=false;}
struct Preferences {
  std::string space;
  bool begin(const char* name,bool readOnly){
    if(!readOnly&&config_host::failWriteBegin){config_host::failWriteBegin=false;return false;}
    space=name;return true;
  }
  void end(){}
  size_t getBytesLength(const char*)const{
    const auto found=config_host::nvs.find(space);
    return found==config_host::nvs.end()?0:found->second.size();
  }
  size_t getBytes(const char*,void* out,size_t length){
    if(config_host::failRead){config_host::failRead=false;return 0;}
    const auto found=config_host::nvs.find(space);
    if(found==config_host::nvs.end()||found->second.size()!=length)return 0;
    memcpy(out,found->second.data(),length);
    if(config_host::corruptRead){config_host::corruptRead=false;static_cast<uint8_t*>(out)[0]^=1;}
    return length;
  }
  size_t putBytes(const char*,const void* bytes,size_t length){
    ++config_host::writes;
    if(config_host::failAllWrites||config_host::failWrite){config_host::failWrite=false;return 0;}
    const auto* first=static_cast<const uint8_t*>(bytes);
    if(config_host::partialWrite){
      config_host::partialWrite=false;config_host::nvs[space]={first,first+length/2};return length/2;
    }
    config_host::nvs[space]={first,first+length};
    if(config_host::afterWrite){
      auto callback=std::move(config_host::afterWrite);config_host::afterWrite=nullptr;callback();
    }
    if(config_host::writeThenFail){config_host::writeThenFail=false;return 0;}
    return length;
  }
  bool remove(const char*){
    ++config_host::removes;
    if(config_host::failAllWrites)return false;
    return config_host::nvs.erase(space)!=0;
  }
};
