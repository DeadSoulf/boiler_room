#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdarg>
extern uint32_t mock_now;
inline uint32_t millis(){return mock_now;}
constexpr int INPUT=0, INPUT_PULLUP=1, LOW=0;
inline unsigned pulls[55]={},reads[55]={};
inline void pinMode(int pin,int mode){if(mode==INPUT_PULLUP)++pulls[pin];}
inline int digitalRead(int pin){++reads[pin];return 1;}
struct SerialMock { void println(const char*){} void printf(const char*,...){} };
inline SerialMock Serial;
