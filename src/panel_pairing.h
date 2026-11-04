#pragma once
// Bounded pairing challenge. Serialized by panel_web's mutex.
// Code is only read by LVGL. HTTP gets expiry/status, never the code.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace panel_pairing {
constexpr uint32_t CODE_MS=120000U, ISSUE_PAUSE_MS=30000U, LOCKOUT_MS=120000U;
enum class Request { Started, Existing, Busy, Cooldown, Invalid };
enum class Verify { Accepted, Rejected, Locked };
inline void wipe(void* p,size_t n){volatile unsigned char* b=static_cast<volatile unsigned char*>(p);while(n--)*b++=0;}
inline bool digits6(const char* s){if(!s||strlen(s)!=6)return false;for(unsigned i=0;i<6;i++)if(s[i]<'0'||s[i]>'9')return false;return true;}
inline bool ticket32(const char* s){if(!s||strlen(s)!=32)return false;for(unsigned i=0;i<32;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;return true;}
inline bool equal(const char* a,const char* b,size_t n){unsigned char diff=0;for(size_t i=0;i<n;i++)diff|=static_cast<unsigned char>(a[i])^static_cast<unsigned char>(b[i]);return diff==0;}
class State {
  char pin_[7]={},ticket_[33]={};
  uint32_t created_=0,pausedAt_=0,pauseMs_=0,generation_=0;
  unsigned failures_=0;
  void clearCode(){wipe(pin_,sizeof(pin_));wipe(ticket_,sizeof(ticket_));failures_=0;}
public:
  bool active(uint32_t now){if(pin_[0]&&uint32_t(now-created_)>=CODE_MS)clearCode();return pin_[0]!=0;}
  uint32_t remaining(uint32_t now){return active(now)?(CODE_MS-uint32_t(now-created_)+999U)/1000U:0;}
  uint32_t retryAfter(uint32_t now)const{return pauseMs_&&uint32_t(now-pausedAt_)<pauseMs_?(pauseMs_-uint32_t(now-pausedAt_)+999U)/1000U:0;}
  Request request(const char* ticket,const char* code,uint32_t now){
    if(!ticket32(ticket)||!digits6(code))return Request::Invalid;
    if(active(now))return equal(ticket_,ticket,32)?Request::Existing:Request::Busy;
    if(retryAfter(now))return Request::Cooldown;
    memcpy(ticket_,ticket,33);memcpy(pin_,code,7);created_=pausedAt_=now;pauseMs_=ISSUE_PAUSE_MS;failures_=0;++generation_;
    return Request::Started;
  }
  bool cancel(const char* ticket,uint32_t now){
    if(!ticket32(ticket)||!active(now)||!equal(ticket_,ticket,32))return false;
    clearCode();return true;
  }
  Verify verify(const char* ticket,const char* code,uint32_t now){
    if(!ticket32(ticket)||!digits6(code)||!active(now)||!equal(ticket_,ticket,32))return Verify::Rejected;
    if(!equal(pin_,code,6)){
      if(++failures_>=5){clearCode();pausedAt_=now;pauseMs_=LOCKOUT_MS;return Verify::Locked;}
      return Verify::Rejected;
    }
    clearCode();return Verify::Accepted;
  }
  bool display(char out[7],uint32_t& seconds,uint32_t& generation,uint32_t now){
    out[0]=0;seconds=remaining(now);generation=generation_;
    if(!seconds)return false;
    memcpy(out,pin_,7);return true;
  }
  void revoke(){clearCode();}
};
// A non-simple custom header prevents ordinary cross-origin HTML forms from
// starting a challenge. No CORS/OPTIONS permission is granted by the server.
inline bool localRequest(const char* marker,const char* origin,const char* expected,const char* expectedPort,const char* site){
  if(!marker||strcmp(marker,"1"))return false;
  if(site&&site[0]&&strcmp(site,"same-origin")&&strcmp(site,"none"))return false;
  return !origin||!origin[0]||!strcmp(origin,expected)||!strcmp(origin,expectedPort);
}
} // namespace panel_pairing
