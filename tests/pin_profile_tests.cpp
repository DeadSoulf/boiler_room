#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "panel_pin_profile.h"

int main(){
  constexpr int expectedGPIO[]={52,33,51,31,50,30,49,29,34,32,28};
  constexpr unsigned expectedJP1[]={7,8,9,10,11,12,13,14,17,19,21};
  static_assert(panel_pins::HEADER_COUNT==11,"Only 11 usable GPIOs on JP1");
  for(unsigned i=0;i<11;i++){
    assert(panel_pins::sensorPin(expectedGPIO[i]));
    assert(panel_pins::connectorPin(expectedGPIO[i])==expectedJP1[i]);
  }
  for(int gpio=0;gpio<=54;gpio++){
    bool expected=false;
    for(int pin:expectedGPIO)if(pin==gpio)expected=true;
    assert(panel_pins::sensorPin(gpio)==expected);
    assert((panel_pins::blockReason(gpio)==nullptr)==expected);
  }
  for(int reserved:{3,5,7,8,9,10,11,12,13,14,15,16,17,18,19,
                     23,26,27,35,37,38,39,40,41,42,43,44,48,54}){
    assert(!panel_pins::sensorPin(reserved));
  }
  std::puts("PASS: 11 validated JC4880P443 JP1 sensor GPIOs, boot and buses locked");
  return 0;
}
