// Optional build branches against mocks only, not an MCU firmware build.
#include "../src/panel_radio.cpp"
#include <cassert>
int main(){using namespace panel_radio;assert(begin());scanBle();
#if PANEL_BLE
assert(state.bleCount==0);
#else
assert(state.bleMessage[0]);
#endif
vSemaphoreDelete(stateMutex);stateMutex=nullptr;}
