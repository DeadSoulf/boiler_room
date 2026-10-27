#include "screen_state.h"
#include <cassert>
#include <cstdio>
#include <random>
int main(){
 using panel_screen::State;State s;
 s.begin(0,0,true);assert(!s.tick(1000000));
 s.begin(100,30,true);assert(!s.tick(100000)); // Must see a working touch this boot.
 assert(s.touch(true,100001));s.touch(false,100002);
 assert(!s.tick(130000));assert(s.tick(130001));assert(!s.awake);
 assert(!s.touch(true,130002));assert(s.awake && s.consume);
 assert(!s.touch(true,160000));assert(!s.tick(200000));
 assert(!s.touch(false,160001));assert(!s.touch(false,160120));assert(s.consume);
 assert(!s.touch(false,160121));assert(!s.consume);assert(s.touch(true,160122));
 s.touch(false,160123);assert(s.tick(190122));s.changeTimeout(0,190123);assert(s.awake && !s.tick(999999));
 s.begin(UINT32_MAX-10000,30,true);s.touch(true,UINT32_MAX-10000);s.touch(false,UINT32_MAX-9999);
 assert(!s.tick(10000));assert(s.tick(20000));
 s.begin(0,30,false);assert(!s.touch(true,1));assert(!s.tick(99999));
 s.begin(0,1800,true);assert(s.timeoutSeconds==1800);s.changeTimeout(UINT32_MAX,1);assert(s.timeoutSeconds==0);
 for(auto seconds:{0U,30U,60U,120U,300U,600U,1800U})assert(panel_screen::validTimeout(seconds));
 for(auto seconds:{1U,29U,65566U,UINT32_MAX})assert(!panel_screen::validTimeout(seconds));
 // Preserve v0.3.1 bounce guard: a renewed press restarts release debounce.
 s.begin(0,30,true);s.touch(true,0);s.touch(false,1);s.tick(30000);s.touch(true,30100);s.touch(false,30200);
 s.touch(true,30201);assert(s.consume);s.touch(false,30500);s.touch(false,30619);assert(s.consume);
 s.touch(false,30620);assert(!s.consume);
 std::mt19937 gen(314159);uint32_t now=UINT32_MAX-50000;s.begin(now,30,true);
 for(int i=0;i<100000;i++){now+=gen()%1000;const bool pressed=gen()%2,wasOff=!s.awake;
   bool forward=s.touch(pressed,now);assert(!forward||pressed);if(wasOff&&pressed)assert(!forward&&s.awake);
   s.tick(now);if(s.consume)assert(s.awake);}
 puts("PASS: v031 30min choice, 120ms wake debounce, first-touch safety, missing touch, wraparound, hold, 100000 random steps");
}
