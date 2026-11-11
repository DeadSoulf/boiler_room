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
 // Historical indices remain stable; missing or stale return affects only return.
 static_assert(CHANNELS==6);
 temps[2].put(63.5f,60000);temps[3].put(28.0f,60000);
 publish(temps,fuel,60000);
 assert(latest.temp[2].value==63.5f&&latest.temp[3].value==28.0f&&!latest.temp[4].ever);
 temps[4].put(49.0f,60000);temps[4].put(50.0f,61000);
 temps[2].put(65.0f,62000);publish(temps,fuel,62000);
 assert(latest.temp[2].count==2&&latest.temp[2].history[0]==63.5f);
 assert(latest.temp[4].count==2&&latest.temp[4].history[0]==49.0f&&latest.temp[4].maximum==50.0f);
 publish(temps,fuel,61000+STALE_MS);assert(latest.temp[2].valid&&!latest.temp[4].valid);
 // The outlet is independent of both fuel-tank history and the two level switches.
 assert(!latest.temp[5].ever);
 temps[5].put(39.0f,95000);temps[5].put(41.5f,96000);publish(temps,fuel,96000);
 assert(!latest.temp[3].valid&&latest.temp[3].count==1&&latest.temp[3].history[0]==28.0f);
 assert(latest.temp[5].valid&&latest.temp[5].count==2&&latest.temp[5].minimum==39.0f&&latest.temp[5].maximum==41.5f);
 assert(latest.temp[5].history[0]==39.0f&&latest.temp[5].history[1]==41.5f);
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
