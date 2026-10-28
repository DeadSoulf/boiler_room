#pragma once
// Minimal host substitutes around the unchanged Arduino-ESP32 3.3.6 wrapper.
// See official_sources.json and README.md for provenance and scope.
#include <Arduino.h>
#include <cstdint>
#define log_w(...) ((void)0)
#define log_e(...) ((void)0)
constexpr uint32_t BLE_NPL_TIME_FOREVER=UINT32_MAX;
class BLEScan;
struct BLETaskData { explicit BLETaskData(BLEScan*){} };
struct BLEUtils { static void taskWait(BLETaskData&,uint32_t){} };
class BLEAdvertisedDevice {public: bool haveName(){return false;} String getName(){return "";}
 struct Address{String toString(){return "00:00:00:00:00:00";}}; Address getAddress(){return {};} int getRSSI(){return -50;} };
class BLEScanResults {public:int getCount(){return 0;} BLEAdvertisedDevice getDevice(int){return {};}};
class BLEScan { public:
 BLEScanResults m_scanResults; BLETaskData* m_pTaskData=nullptr; unsigned startCalls=0; bool startSucceeded=false;
 void setActiveScan(bool){} void setInterval(int){} void setWindow(int){} void clearResults(){}
 bool start(uint32_t,void(*)(BLEScanResults),bool){++startCalls;return startSucceeded;}
 BLEScanResults* start(uint32_t duration,bool is_continue);
};
class BLEDevice {public:static bool init(const char*){return true;} static BLEScan* getScan(){static BLEScan s;return &s;}};
// Exact Arduino-ESP32 3.3.6 blocking wrapper, BLEScan.cpp lines 790-809.
#include "ble_scan_blocking_wrapper.inc"
