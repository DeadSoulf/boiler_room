#pragma once
#define ESP_OK 0
inline int board_p4_display_init(){return ESP_OK;}

inline bool hostBacklight=true;
inline bool hostTouchAvailable=true;
inline void board_p4_backlight(bool on){hostBacklight=on;}
inline void* board_p4_get_touch_handle(){return hostTouchAvailable?reinterpret_cast<void*>(1):nullptr;}
