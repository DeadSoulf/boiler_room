// The panel must list equipment without implementing any actuator commands.
#include "panel_equipment.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace panel_equipment;
int main() {
  static_assert(COUNT==7, "Seven real-equipment entries are required");
  const char* expected[COUNT]={
    "Бойлер косвенного нагрева","Электрокотёл","Контур 1",
    "Контур 2","Контур 3","Дизельный отопитель","Подогрев топлива"
  };
  for(unsigned i=0;i<COUNT;i++){
    assert(name(i) && strcmp(name(i),expected[i])==0);
    assert(strcmp(name(i),NOT_CONNECTED)!=0);
  }
  assert(strcmp(name(COUNT),"Неизвестное оборудование")==0);
  assert(strcmp(NOT_CONNECTED,"Контроллер не подключён")==0);
  puts("PASS: equipment inventory (7 entries, not connected; no simulated commands)");
}
