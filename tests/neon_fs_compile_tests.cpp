// Host-portable tests of schematic reed states and read-only FS signatures.
#include "../src/fuel_level_state.h"
#include "../src/panel_fs_signature.h"
#include <assert.h>
#include <string.h>
#include <stdint.h>
using namespace fuel_level;
using namespace panel_fs_signature;
int main(){
  assert(interpret(Sensors{})==Level::Unknown);
  Sensors s; s.connected=true;
  assert(interpret(s)==Level::Full);
  s.upperActive=true;assert(interpret(s)==Level::Half);
  s.lowerActive=true;assert(interpret(s)==Level::Low);
  s.upperActive=false;assert(interpret(s)==Level::Low);
  s.stale=true;assert(interpret(s)==Level::Unknown);
  uint8_t boot[512]={};boot[510]=0x55;boot[511]=0xAA;
  memcpy(boot+3,"EXFAT   ",8);assert(bootSector(boot,512)==Type::ExFat);
  memset(boot+3,0,8);memcpy(boot+82,"FAT32   ",8);
  assert(bootSector(boot,512)==Type::Fat);
  memset(boot+82,0,8);
  boot[446+4]=0x07;boot[446+8]=0x00;boot[446+9]=0x20;boot[446+12]=1;
  assert(mbrFirstPartition(boot,512)==8192);
  boot[446+4]=0xEE;assert(protectiveGpt(boot,512));
  assert(mbrFirstPartition(boot,512)==0);
  return 0;
}
