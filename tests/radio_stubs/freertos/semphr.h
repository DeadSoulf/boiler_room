#pragma once
using SemaphoreHandle_t=int*;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return new int(0);}
inline int xSemaphoreTake(SemaphoreHandle_t p,unsigned){if(*p)return 0;*p=1;return 1;}
inline void xSemaphoreGive(SemaphoreHandle_t p){*p=0;}
inline void vSemaphoreDelete(SemaphoreHandle_t p){delete p;}
