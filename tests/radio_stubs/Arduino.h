#pragma once
#include <string>
#include <stdint.h>
using String=std::string;
inline uint32_t fakeMillis=0;
inline uint32_t millis(){return fakeMillis;}
inline void configTime(long,int,const char*,const char*){}

#include <cstdio>
struct SerialCapture {
 std::string output;
 template<class... Args>void printf(const char* format,Args... args){char text[256];std::snprintf(text,sizeof(text),format,args...);output+=text;}
};inline SerialCapture Serial;
