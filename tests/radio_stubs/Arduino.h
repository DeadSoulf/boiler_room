#pragma once
#include <string>
#include <stdint.h>
using String=std::string;
inline uint32_t fakeMillis=0;
inline uint32_t millis(){return fakeMillis;}
inline void configTime(long,int,const char*,const char*){}
