#pragma once
#include <Arduino.h>
class BLEAdvertisedDevice {public:bool haveName(){return false;}String getName(){return "";}
 struct Address{String toString(){return "00:00:00:00:00:00";}};
 Address getAddress(){return {};}int getRSSI(){return -50;}};
class BLEScanResults {public:int getCount(){return 0;}BLEAdvertisedDevice getDevice(int){return {};}};
class BLEScan {public:void setActiveScan(bool){}void setInterval(int){}void setWindow(int){}
 BLEScanResults* start(unsigned,bool){static BLEScanResults r;return &r;}void clearResults(){}};
class BLEDevice {public:static bool init(const char*){return true;}static BLEScan* getScan(){static BLEScan s;return &s;}};
