#pragma once
#include <cstdint>
using BaseType_t=int;
constexpr int pdTRUE=1, pdFALSE=0,pdPASS=1;
inline uint32_t pdMS_TO_TICKS(uint32_t n){return n;}
