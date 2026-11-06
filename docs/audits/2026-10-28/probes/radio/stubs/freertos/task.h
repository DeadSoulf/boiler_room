#pragma once
#include <Arduino.h>
inline void (*delayHook)()=nullptr;
inline void vTaskDelay(unsigned n){fakeMillis+=n;if(delayHook)delayHook();}
inline int taskResult=1;
inline int xTaskCreate(void(*)(void*),const char*,unsigned,void*,unsigned,void*){return taskResult;}
