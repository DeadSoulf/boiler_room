#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdarg>
extern uint32_t mock_now;
inline uint32_t millis(){return mock_now;}
constexpr int INPUT=0, INPUT_PULLUP=1, LOW=0;
inline void pinMode(int,int){}
inline int digitalRead(int){return 1;}
struct SerialMock { void println(const char*){} void printf(const char*,...){} };
inline SerialMock Serial;
