#pragma once
#include <Arduino.h>
enum arduino_event_id_t {ARDUINO_EVENT_WIFI_STA_DISCONNECTED};
struct arduino_event_info_t {struct {unsigned reason;}wifi_sta_disconnected;};
enum {WIFI_REASON_AUTH_EXPIRE=2,WIFI_REASON_ASSOC_LEAVE=8,WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT=15,WIFI_REASON_NO_AP_FOUND=201,WIFI_REASON_AUTH_FAIL=202,WIFI_REASON_HANDSHAKE_TIMEOUT=204};
enum {WIFI_AUTH_OPEN,WIFI_AUTH_WPA2_PSK,WIFI_AUTH_WPA_WPA2_PSK,WIFI_AUTH_WPA3_PSK,WIFI_AUTH_WPA2_WPA3_PSK,WIFI_STA,WL_CONNECTED=10,WL_CONNECT_FAILED=11};
struct IP {unsigned value=0;operator uint32_t()const{return value;}String toString()const{return "192.0.2.1";}};
struct Wifi {
 int scanResult=0;unsigned scanElapsed=0,scanCount=0;
 bool connectSuccess=false,connected=false,reconnect=false;unsigned beginCount=0,disconnectCount=0;
 bool disconnectSuccess=true,disconnectChangesState=true,beginFailure=false,hasIp=true;
 String connectedSsid="test",beginSsidOverride;
 void onEvent(void(*)(arduino_event_id_t,arduino_event_info_t),arduino_event_id_t){}
 void persistent(bool){} void setHostname(const char*){} bool mode(int){return true;}
 void setSleep(bool){} void setScanTimeout(unsigned){} void setAutoReconnect(bool b){reconnect=b;}
 bool disconnect(bool,bool){++disconnectCount;if(!disconnectSuccess)return false;if(disconnectChangesState)connected=false;return true;} void setMinSecurity(int){}
 int begin(const char* ssid,const char*){++beginCount;if(beginFailure)return WL_CONNECT_FAILED;connected=connectSuccess;if(connected)connectedSsid=beginSsidOverride.empty()?ssid:beginSsidOverride;return status();}
 int status(){return connected?WL_CONNECTED:0;} IP localIP(){return {connected&&hasIp?1u:0u};}
 String SSID(int index=-1){return index<0?connectedSsid:"test";}int RSSI(int=-1){return -45;}
 void scanDelete(){}int scanNetworks(bool,bool,bool,int){++scanCount;fakeMillis+=scanElapsed;return scanResult;}int encryptionType(int){return WIFI_AUTH_WPA2_PSK;}
};inline Wifi WiFi;
