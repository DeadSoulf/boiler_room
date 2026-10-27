#include <Arduino.h>
#include <Preferences.h>
#include "board_p4.h"
#include "panel_screen.h"
#include "screen_state.h"
namespace panel_screen {
BootTimes boot;
namespace {
State state;
Preferences prefs;
bool storageReady=false;
const char* message="Настройки экрана ещё не прочитаны";
}
void begin(bool touchReady) {
  // Separate namespace: never clears/rewrites Wi-Fi or clock settings.
  storageReady=prefs.begin("boiler_screen",false);
  const uint32_t saved=storageReady?prefs.getUInt("off_sec",0):0;
  state.begin(millis(),saved,touchReady);
  board_p4_backlight(true); // Every normal boot starts with the screen ON.
  message=!touchReady?"Сенсор недоступен. Автовыключение запрещено":
          !storageReady?"Память настроек недоступна. Экран всегда включён":
          !validTimeout(saved)?"Некорректная настройка. Экран всегда включён":
          "Настройка сохранится после выключения питания";
}
void tick(){if(state.tick(millis()))board_p4_backlight(false);}
bool setTimeout(uint32_t seconds) {
  if(!validTimeout(seconds)){message="Недопустимое время";return false;}
  if(seconds && !state.touchReady){message="Нет сенсора. Автовыключение запрещено";return false;}
  if(!storageReady){message="Не удалось открыть память настроек";return false;}
  if(seconds!=state.timeoutSeconds && prefs.putUInt("off_sec",seconds)!=sizeof(uint32_t)){
    message="Не удалось сохранить. Настройка не изменена";return false;
  }
  state.changeTimeout(seconds,millis());board_p4_backlight(true);
  message="Сохранено. Первое касание только включает экран";return true;
}
uint32_t timeout(){return state.timeoutSeconds;}
bool touchReady(){return state.initialized && state.touchReady;}
bool awake(){return state.awake;}
const char* status(){return message;}
} // namespace panel_screen
extern "C" bool panel_screen_touch_ready(void){return panel_screen::touchReady();}
extern "C" bool panel_screen_filter_touch(bool pressed) {
  const bool wasAwake=panel_screen::state.awake;
  const bool forward=panel_screen::state.touch(pressed,millis());
  if(!wasAwake && panel_screen::state.awake)board_p4_backlight(true);
  return forward;
}
