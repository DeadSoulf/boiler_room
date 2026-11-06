#pragma once
#include "FreeRTOS.h"
#include <deque>
#include <vector>
#include <cstring>
struct Q {unsigned capacity,size; std::deque<std::vector<unsigned char>> data;};
using QueueHandle_t=Q*;
inline Q* xQueueCreate(unsigned n,unsigned sz){return new Q{n,sz,{}};}
inline int xQueueSend(Q* q,const void* value,uint32_t){if(q->data.size()>=q->capacity)return pdFALSE; const auto* p=(const unsigned char*)value;q->data.emplace_back(p,p+q->size);return pdTRUE;}
inline int xQueueReceive(Q* q,void* out,uint32_t){if(q->data.empty())return pdFALSE;memcpy(out,q->data.front().data(),q->size);q->data.pop_front();return pdTRUE;}
inline void xQueueReset(Q* q){q->data.clear();}
