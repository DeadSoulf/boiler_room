#include <Arduino.h>
#include "board_p4.h"
#include "panel_screen.h"
#include "screen_state.h"
namespace panel_screen {
BootTimes boot;
namespace { State state;const char* message="Настройки экрана загружаются"; }
void begin(bool touchReady){
  state.begin(millis(),0,touchReady);board_p4_backlight(true);
  message=touchReady?"Для автовыключения сначала коснитесь экрана":"Сенсор недоступен. Автовыключение запрещено";
}
void tick(){if(state.tick(millis()))board_p4_backlight(false);}
// Apply only an already validated/persisted worker snapshot. No NVS calls under LVGL lock.
bool setTimeout(uint32_t seconds){
  if(!validTimeout(seconds))return false;
  if(seconds && !state.touchReady)return false;
  state.changeTimeout(seconds,millis());board_p4_backlight(true);return true;
}
uint32_t timeout(){return state.timeoutSeconds;}
bool touchReady(){return state.initialized && state.touchReady;}
bool awake(){return state.awake;}
const char* status(){return message;}
}
extern "C" bool panel_screen_touch_ready(void){return panel_screen::touchReady();}
extern "C" bool panel_screen_filter_touch(bool pressed){
  const bool wasAwake=panel_screen::state.awake;
  const bool forward=panel_screen::state.touch(pressed,millis());
  if(!wasAwake && panel_screen::state.awake)board_p4_backlight(true);
  return forward;
}
