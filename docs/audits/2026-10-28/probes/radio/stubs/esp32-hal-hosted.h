#pragma once
inline bool hostedSetPins(int,int,int,int,int,int,int){return true;}
inline void hostedGetHostVersion(uint32_t*a,uint32_t*b,uint32_t*c){*a=2;*b=9;*c=6;}
inline void hostedGetSlaveVersion(uint32_t*,uint32_t*,uint32_t*){}
