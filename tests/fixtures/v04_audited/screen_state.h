#pragma once
#include <stdint.h>
// Pure state machine. Caller serializes it with the LVGL mutex.
// No MCU sleep, no radio operations, no pins, no persistence here.
namespace panel_screen {
inline bool validTimeout(uint32_t seconds) {
  return seconds==0 || seconds==30 || seconds==60 || seconds==120 ||
         seconds==300 || seconds==600 || seconds==1800;
}
struct State {
  uint32_t timeoutSeconds=0, lastActivity=0, releaseSince=0;
  bool initialized=false, touchReady=false, awake=true;
  bool consume=false, releaseSeen=false, observedTouch=false, touchDown=false;
  void begin(uint32_t now,uint32_t seconds,bool ready) {
    timeoutSeconds=validTimeout(seconds)?seconds:0;
    lastActivity=now;releaseSince=now;
    initialized=true;touchReady=ready;awake=true;consume=false;releaseSeen=false;observedTouch=false;touchDown=false;
  }
  bool touch(bool pressed,uint32_t now) {
    if(!initialized || !touchReady)return false;
    touchDown=pressed;if(pressed)observedTouch=true;
    if(!awake && pressed) {
      awake=true;consume=true;releaseSeen=false;lastActivity=now;
      return false; // Wake never clicks, drags or swipes the underlying UI.
    }
    if(consume) {
      lastActivity=now;
      if(pressed)releaseSeen=false;
      else if(!releaseSeen){releaseSeen=true;releaseSince=now;}
      else if(uint32_t(now-releaseSince)>=120U){consume=false;releaseSeen=false;}
      return false;
    }
    if(pressed)lastActivity=now;
    return awake && pressed;
  }
  bool tick(uint32_t now) {
    if(initialized && touchReady && observedTouch && !touchDown && awake && !consume && timeoutSeconds &&
       uint32_t(now-lastActivity)>=timeoutSeconds*1000U) {
      awake=false;return true;
    }
    return false;
  }
  void changeTimeout(uint32_t seconds,uint32_t now) {
    timeoutSeconds=validTimeout(seconds)?seconds:0;lastActivity=now;awake=true;
  }
};
} // namespace panel_screen
