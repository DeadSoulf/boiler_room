#pragma once
#include <cstdint>
#include <cassert>
// Public signatures/boolean success contract verified against junkfix/esp32-ds18b20
// c2aa0147aa7414419fce61e89ed07ef6ff1d535b. Simulates a retained CRC-good scratchpad.
struct OneWire32 {
  inline static uint64_t rom=0;
  inline static unsigned writes=0,requests=0,reads=0,temperatures=0;
  inline static bool resetOk=true,readOk=true;
  inline static unsigned failWrite=0;
  inline static uint8_t completion=1,tempError=0;
  inline static float scratchpad=85.0f;
  explicit OneWire32(uint8_t){}
  uint8_t search(uint64_t* devices,uint8_t){devices[0]=rom;return 1;}
  bool reset(){writes=0;return resetOk;}
  bool write(const uint8_t data,uint8_t len=8){
    assert(len==8);++writes;
    assert(data==(writes==1?0xCC:0x44));
    return writes!=failWrite;
  }
  // Deliberately mirrors the original lossy API for before-fix regression runs.
  void request(){++requests;if(reset()){write(0xCC);write(0x44);}}
  bool read(uint8_t& data,uint8_t len=8){assert(len==1);++reads;data=completion;return readOk;}
  uint8_t getTemp(uint64_t&,float& out){++temperatures;out=scratchpad;return tempError;}
};
