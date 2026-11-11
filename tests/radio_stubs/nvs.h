#pragma once
#include <Preferences.h>
#include <esp_err.h>
// The read-only IDF handle and the Arduino wrapper see the SAME namespace,
// also in adapter tests which set nvsOK directly instead of starting a worker.
namespace panel_radio { namespace { extern Preferences prefs; } }
using nvs_handle_t=Preferences*;
enum nvs_open_mode_t {NVS_READONLY};
constexpr esp_err_t ESP_ERR_NVS_NOT_FOUND=0x1102, ESP_ERR_NVS_INVALID_LENGTH=0x110c;
inline esp_err_t nvs_open(const char* name,nvs_open_mode_t mode,nvs_handle_t* out){
 if(!out||!name||std::strcmp(name,"boiler03")||mode!=NVS_READONLY||!panel_radio::prefs.opens)return ESP_FAIL;
 *out=&panel_radio::prefs;return ESP_OK;
}
inline esp_err_t nvs_get_blob(nvs_handle_t store,const char* key,void* data,size_t* size){
 if(!store||!key||!size||!store->reads||store->failReadKey==key||(!data&&store->failLengthKey==key))return ESP_FAIL;
 const auto* b=store->blob(key);if(!b)return ESP_ERR_NVS_NOT_FOUND;
 if(!data){*size=b->size();return ESP_OK;}
 if(*size<b->size()){*size=b->size();return ESP_ERR_NVS_INVALID_LENGTH;}
 const auto read=store->getBytes(key,data,*size);*size=read;return read==b->size()?ESP_OK:ESP_FAIL;
}
inline void nvs_close(nvs_handle_t){}
