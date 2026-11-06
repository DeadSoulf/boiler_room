#!/usr/bin/env python3
"""Execute the firmware's actual telemetry publisher against host-only fakes."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/panel_web.cpp").read_text()
shared = source[source.index("struct TempSnap {"):source.index("SemaphoreHandle_t guard=")]
publish = source[source.index("void publish(const Measurement"):source.index("} // namespace panel_web")]
test = r'''
#include "panel_types.h"
#include <cassert>
#include <cstdio>
using namespace panel;
bool guard=true,lockAllowed=true;constexpr int pdTRUE=1;
int xSemaphoreTake(bool,int){return lockAllowed?pdTRUE:0;}
void xSemaphoreGive(bool){}
struct Esp {unsigned getFreeHeap(){return 100000;}} ESP;
namespace panel_screen {
 bool touchReady(){return true;}
 struct {uint32_t displayMs=1,lvglMs=2,firstFrameAt=3,touchMs=4;}boot;
}
SHARED_SOURCE
PUBLISH_SOURCE
int main(){
 Measurement temps[CHANNELS];FuelLevel fuel;
 publish(temps,fuel,1000);assert(latest.published&&!latest.temp[0].valid&&!latest.lowerValid&&!latest.upperValid);
 temps[0].put(85.0f,1000);fuel.put(true,false,1000);
 publish(temps,fuel,1000);assert(latest.temp[0].valid&&latest.temp[0].value==85.0f);
 assert(latest.lowerValid&&latest.lowerReached&&latest.upperValid&&!latest.upperReached);
 publish(temps,fuel,1000+STALE_MS-1);assert(latest.temp[0].valid&&latest.lowerValid&&latest.upperValid);
 publish(temps,fuel,1000+STALE_MS);assert(!latest.temp[0].valid&&!latest.lowerValid&&!latest.upperValid);
 assert(latest.temp[0].ever&&latest.temp[0].count==1&&temps[0].valid&&fuel.lower.valid);
 assert(latest.fuelStatus==uint8_t(FuelLevelStatus::Stale));
 // Regular publishing observes expiration, then the 32-bit uptime completes a turn.
 const uint32_t wrapped=uint32_t((1ULL<<32)+1001);
 publish(temps,fuel,wrapped);assert(!latest.temp[0].valid&&!latest.lowerValid&&!latest.upperValid);
 assert(latest.fuelStatus==uint8_t(FuelLevelStatus::Stale));
 temps[0].put(26.0f,wrapped);fuel.put(true,true,wrapped);
 publish(temps,fuel,wrapped);assert(latest.temp[0].valid&&latest.temp[0].value==26.0f);
 assert(latest.lowerValid&&latest.upperValid&&latest.fuelStatus==uint8_t(FuelLevelStatus::UpperReached));
 // Unknown other reed must not prevent this input's stale latch from being observed.
 fuel.clear();fuel.upper.put(true,2000);publish(temps,fuel,2000+STALE_MS);
 fuel.lower.put(true,2001);publish(temps,fuel,2001);
 assert(latest.lowerValid&&!latest.upperValid&&latest.fuelStatus==uint8_t(FuelLevelStatus::Stale));
 puts("PASS: actual web telemetry, fresh/stale/missing, exact expiry, full-wrap persistence and recovery");
}
'''.replace("SHARED_SOURCE", shared).replace("PUBLISH_SOURCE", publish)
with tempfile.TemporaryDirectory(prefix="boiler-web-telemetry-") as temp:
    cpp = Path(temp) / "test.cpp"
    binary = Path(temp) / "test"
    cpp.write_text(test)
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I", str(root / "src"), str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
