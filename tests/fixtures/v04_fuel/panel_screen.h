#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Called ONLY from lvgl_glue.c, under its recursive LVGL mutex.
bool panel_screen_touch_ready(void);
bool panel_screen_filter_touch(bool pressed);
#ifdef __cplusplus
}
namespace panel_screen {
// All calls below must be made under the same LVGL mutex.
void begin(bool touchReady);
void tick();
bool setTimeout(uint32_t seconds);
uint32_t timeout();
bool touchReady();
bool awake();
const char* status();
struct BootTimes {
  uint32_t setupAt=0, displayMs=0, lvglMs=0, firstFrameAt=0, touchMs=0;
};
extern BootTimes boot;
}
#endif
