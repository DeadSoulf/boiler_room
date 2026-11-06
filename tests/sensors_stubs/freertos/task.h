#pragma once
#include "FreeRTOS.h"
using TaskHandle_t=void*;
inline void vTaskDelay(uint32_t){}
inline int xTaskCreate(void(*)(void*),const char*,uint32_t,void*,int,TaskHandle_t*){return pdPASS;}
