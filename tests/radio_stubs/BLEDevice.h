#pragma once
#include <Arduino.h>
#include <vector>
class BLEAdvertisedDevice {public:
 String name,address="00:00:00:00:00:00";int rssi=-50;
 bool haveName(){return !name.empty();}String getName(){return name;}
 struct Address{String value;String toString(){return value;}};
 Address getAddress(){return {address};}int getRSSI(){return rssi;}};
class BLEScanResults {public:
 std::vector<BLEAdvertisedDevice> devices;
 int getCount(){return int(devices.size());}BLEAdvertisedDevice getDevice(unsigned i){return devices.at(i);}};
// Controlled API outcomes, not an emulation of RF or the NimBLE event task.
class BLEScan {public:
 bool scanning=false,startSuccess=true,stopSuccess=true,autoComplete=true,notifyCompletion=true,resultsAvailable=true;
 unsigned startCalls=0,stopCalls=0,clearCalls=0,completionCalls=0;
 uint32_t started=0,completionDelay=100;
 BLEScanResults results,nextResults;
 void (*callback)(BLEScanResults)=nullptr;
 void setActiveScan(bool){}void setInterval(int){}void setWindow(int){}
 bool start(uint32_t,void(*cb)(BLEScanResults),bool keep=false){
  ++startCalls;callback=cb;if(!keep)clearResults();
  if(!startSuccess)return false;
  scanning=true;started=fakeMillis;return true;
 }
 void finish(){scanning=false;results=nextResults;if(callback&&notifyCompletion){++completionCalls;callback(results);}}
 bool isScanning(){if(scanning&&autoComplete&&uint32_t(fakeMillis-started)>=completionDelay)finish();return scanning;}
 // Like the pinned SDK's blocking wrapper, a pointer alone does not prove launch.
 BLEScanResults* start(uint32_t duration,bool keep=false){if(start(duration,nullptr,keep)){fakeMillis+=completionDelay;if(autoComplete)finish();}return &results;}
 bool stop(){++stopCalls;if(!stopSuccess)return false;if(scanning){scanning=false;if(callback&&notifyCompletion){++completionCalls;callback(results);}}return true;}
 BLEScanResults* getResults(){return resultsAvailable?&results:nullptr;}
 void clearResults(){++clearCalls;results.devices.clear();}
};
class BLEDevice {public:static bool init(const char*){return true;}static BLEScan* getScan(){static BLEScan s;return &s;}};
