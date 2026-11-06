#pragma once
#include "FreeRTOS.h"
using SemaphoreHandle_t=void*;
inline void* xSemaphoreCreateMutex(){return (void*)1;}
inline int xSemaphoreTake(void*,uint32_t){return pdTRUE;}
inline void xSemaphoreGive(void*){}
