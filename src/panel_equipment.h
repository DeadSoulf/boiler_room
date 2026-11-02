#pragma once
// Equipment inventory only. No simulated state, command queue, GPIO, or outputs.
// A real controller integration must explicitly provide a transport and status.
#include <stdint.h>

namespace panel_equipment {
enum class Id : uint8_t {
  Dhw, Electric, Circuit1, Circuit2, Circuit3, Diesel, FuelHeat, Count
};
constexpr unsigned COUNT=static_cast<unsigned>(Id::Count);
constexpr const char* NAMES[COUNT]={
  "Бойлер косвенного нагрева","Электрокотёл",
  "Контур 1","Контур 2","Контур 3",
  "Дизельный отопитель","Подогрев топлива"
};
constexpr const char* NOT_CONNECTED="Контроллер не подключён";
inline const char* name(unsigned index) {
  return index<COUNT?NAMES[index]:"Неизвестное оборудование";
}
} // namespace panel_equipment
