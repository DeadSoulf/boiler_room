#pragma once
#include <stdint.h>
#include <stdio.h>
extern uint32_t fakeMillis;
inline uint32_t millis(){return fakeMillis;}
inline void delay(uint32_t){}
struct SerialStub{void begin(int){} void println(const char* s){puts(s);}};
extern SerialStub Serial;
struct ESPStub{uint32_t getFreeHeap(){return 180000;}};
extern ESPStub ESP;
