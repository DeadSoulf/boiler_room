#pragma once
// Deterministic ESP-IDF API fakes, not a radio/C6/DHCP simulation.
#include <WiFi.h>
#include <esp_err.h>
#include <algorithm>
#include <cstring>
#include <vector>
enum wifi_mode_t { WIFI_MODE_NULL, WIFI_MODE_STA, WIFI_MODE_APSTA };
enum { WIFI_STORAGE_RAM, WIFI_IF_STA, WIFI_PS_NONE, WIFI_SCAN_TYPE_ACTIVE };
struct wifi_ap_record_t {uint8_t ssid[33]={};int8_t rssi=0;int authmode=WIFI_AUTH_OPEN;};
struct wifi_config_t {uint8_t bytes[128]={};};
struct wifi_scan_config_t {
 bool show_hidden=false;int scan_type=0;
 struct {struct {uint32_t min=0,max=0;} active;} scan_time;
 uint8_t home_chan_dwell_time=0;
};
struct WifiDriver {
 esp_err_t modeResult=ESP_OK, linkResult=ESP_ERR_WIFI_NOT_CONNECT;
 esp_err_t startResult=ESP_OK, stopResult=ESP_OK, storageResult=ESP_OK, configResult=ESP_OK;
 esp_err_t powerResult=ESP_OK, disconnectResult=ESP_OK, countResult=ESP_OK, recordsResult=ESP_OK;
 wifi_mode_t mode=WIFI_MODE_STA;
 bool startEvent=true,stopEvent=true,runtimeConfigCleared=false;
 unsigned startCalls=0,stopCalls=0,storageCalls=0,configCalls=0,disconnectCalls=0;
 unsigned scanCalls=0,scanStopCalls=0,clearCalls=0,scanElapsed=0;
 std::vector<esp_err_t> scanResults;
 std::vector<wifi_ap_record_t> records;
};
inline WifiDriver wifiDriver;
inline esp_err_t esp_wifi_get_mode(wifi_mode_t* mode){*mode=wifiDriver.mode;return wifiDriver.modeResult;}
inline esp_err_t esp_wifi_set_ps(int){return wifiDriver.powerResult;}
inline esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t* ap){
 if(wifiDriver.linkResult!=ESP_ERR_WIFI_NOT_CONNECT)return wifiDriver.linkResult;
 if(!WiFi.connected)return ESP_ERR_WIFI_NOT_CONNECT;
 std::snprintf(reinterpret_cast<char*>(ap->ssid),sizeof(ap->ssid),"%s",WiFi.connectedSsid.c_str());return ESP_OK;
}
inline esp_err_t esp_wifi_start(){
 ++wifiDriver.startCalls;
 if(wifiDriver.startResult==ESP_OK&&wifiDriver.startEvent)WiFi.emit(ARDUINO_EVENT_WIFI_STA_START);
 return wifiDriver.startResult;
}
inline esp_err_t esp_wifi_stop(){
 ++wifiDriver.stopCalls;
 if(wifiDriver.stopResult==ESP_OK){WiFi.connected=false;if(wifiDriver.stopEvent)WiFi.emit(ARDUINO_EVENT_WIFI_STA_STOP);}
 return wifiDriver.stopResult;
}
inline esp_err_t esp_wifi_set_storage(int){++wifiDriver.storageCalls;return wifiDriver.storageResult;}
inline esp_err_t esp_wifi_set_config(int,const wifi_config_t* config){
 ++wifiDriver.configCalls;const wifi_config_t empty={};
 wifiDriver.runtimeConfigCleared=std::memcmp(config,&empty,sizeof(empty))==0;return wifiDriver.configResult;
}
inline esp_err_t esp_wifi_disconnect(){++wifiDriver.disconnectCalls;return wifiDriver.disconnectResult;}
inline esp_err_t esp_wifi_scan_start(const wifi_scan_config_t*,bool){
 const auto index=wifiDriver.scanCalls++;fakeMillis+=wifiDriver.scanElapsed;
 return index<wifiDriver.scanResults.size()?wifiDriver.scanResults[index]:ESP_OK;
}
inline esp_err_t esp_wifi_scan_stop(){++wifiDriver.scanStopCalls;return ESP_OK;}
inline esp_err_t esp_wifi_clear_ap_list(){++wifiDriver.clearCalls;return ESP_OK;}
inline esp_err_t esp_wifi_scan_get_ap_num(uint16_t* count){*count=uint16_t(wifiDriver.records.size());return wifiDriver.countResult;}
inline esp_err_t esp_wifi_scan_get_ap_records(uint16_t* count,wifi_ap_record_t* records){
 *count=uint16_t(std::min<size_t>(*count,wifiDriver.records.size()));
 std::copy_n(wifiDriver.records.begin(),*count,records);return wifiDriver.recordsResult;
}
