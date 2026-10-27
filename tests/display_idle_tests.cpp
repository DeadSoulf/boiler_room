#include "display_idle.h"
#include <cassert>
#include <cstdio>
int main(){
 using panel::DisplayIdle;
 DisplayIdle idle;assert(!idle.tick(1000000)); // Default never.
 idle.configure(30,100);assert(!idle.tick(30099));assert(idle.tick(30100));
 assert(!idle.touch(true,30101));assert(!idle.sleeping);assert(idle.consumeTouch);
 assert(!idle.touch(true,70000));assert(!idle.tick(100000)); // Held wake gesture cannot click or sleep.
 assert(!idle.touch(false,70001));assert(!idle.consumeTouch);
 assert(idle.touch(true,70002));assert(idle.touch(false,70003)); // Only next gesture passes.
 assert(!idle.tick(100001));assert(idle.tick(100002));
 idle.configure(0,100003);assert(!idle.sleeping && !idle.tick(999999));
 idle.configure(30,0xfffffff0U);assert(!idle.tick(10));assert(idle.tick(30000));
 idle.configure(30,100);idle.touch(true,200);assert(!idle.tick(100000));
 idle.touch(false,100001);assert(idle.tick(100002));
 puts("PASS: display never/timeout/wraparound/wake-gesture consumption/held touch/radio-independent state");
}
