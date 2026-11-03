#pragma once
#include <stdint.h>
#include "panel_config.h"
namespace panel_sensors {
struct Event { uint32_t revision=0,atMs=0; uint8_t channel=0; bool reedReached=false; float temperature=0; };
struct ChannelState { bool configured=false,active=false,valid=false; int16_t gpio=-1; uint8_t type=0,found=0; uint32_t lastGoodMs=0,errors=0; char rom[17]={}; char status[76]={}; };
struct Snapshot { bool started=false; uint32_t configRevision=0; ChannelState channels[panel_config::COUNT]; };
bool begin();
bool poll(Event& out);
bool snapshot(Snapshot& out);
}
