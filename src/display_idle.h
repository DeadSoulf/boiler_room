#pragma once
#include <stdint.h>

namespace panel {
// UI inactivity only: never suspends the CPU, radio, LVGL ticks, or touch driver.
struct DisplayIdle {
  uint32_t timeoutMs=0; // Fail-safe default: never turn off.
  uint32_t lastActivity=0;
  bool sleeping=false, consumeTouch=false, touchDown=false;
  void configure(uint32_t seconds,uint32_t now) {
    timeoutMs=seconds*1000U;
    lastActivity=now;
    if(!timeoutMs)sleeping=false;
  }
  // Returns true when this physical touch is allowed into LVGL.
  bool touch(bool pressed,uint32_t now) {
    touchDown=pressed;
    if(pressed) {
      lastActivity=now;
      if(sleeping){sleeping=false;consumeTouch=true;}
    }
    if(consumeTouch) {
      if(!pressed)consumeTouch=false;
      return false;
    }
    return true;
  }
  bool tick(uint32_t now) {
    if(timeoutMs && !sleeping && !touchDown && !consumeTouch && uint32_t(now-lastActivity)>=timeoutMs)
      sleeping=true;
    return sleeping;
  }
};
} // namespace panel
