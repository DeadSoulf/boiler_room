#include "config_pin_fixture.h"
#include <array>
#include <limits>
using namespace panel_config;
using namespace config_test;

void validationAndAvailability(){
  fresh();powerOff();uint32_t saved=123;
  assert(setPin(52,Channel::Room,Type::DS18B20,50,0,saved)==PinResult::Unavailable);
  assert(clearPin(52,0,saved)==PinResult::Unavailable&&saved==123);
  config_host::createLock=false;assert(!begin());config_host::createLock=true;assert(begin());
  config_host::lockAvailable=false;
  assert(setPin(52,Channel::Room,Type::DS18B20,50,0,saved)==PinResult::Unavailable);
  assert(clearPin(52,0,saved)==PinResult::Unavailable);
  config_host::lockAvailable=true;
  constexpr std::array<int,11> allowed={52,33,51,31,50,30,49,29,34,32,28};
  const State empty=state();
  for(int pin=-2;pin<=60;pin++){
    bool permitted=false;for(const int candidate:allowed)if(candidate==pin)permitted=true;
    if(permitted)continue;
    assert(setPin(pin,Channel::Room,Type::DS18B20,50,0,saved)==PinResult::Invalid);
    assert(clearPin(pin,0,saved)==PinResult::Invalid);
  }
  for(const Type type:{Type::None,Type::RemoteModbus,Type::RemoteMQTT,static_cast<Type>(255)})
    assert(setPin(52,Channel::Room,type,50,0,saved)==PinResult::Invalid);
  for(unsigned channel=0;channel<COUNT;channel++){
    const auto ch=static_cast<Channel>(channel);
    if(ch!=Channel::FuelLower&&ch!=Channel::FuelUpper){
      for(const Type type:{Type::ReedNO,Type::ReedNC})
        assert(setPin(52,ch,type,50,0,saved)==PinResult::Invalid);
    }else assert(setPin(52,ch,Type::DS18B20,50,0,saved)==PinResult::Invalid);
  }
  assert(setPin(52,static_cast<Channel>(COUNT),Type::DS18B20,50,0,saved)==PinResult::Invalid);
  assert(setPin(52,static_cast<Channel>(255),Type::DS18B20,50,0,saved)==PinResult::Invalid);
  for(const unsigned debounce:{0U,49U,51U,5000U})
    assert(setPin(52,Channel::Room,Type::DS18B20,debounce,0,saved)==PinResult::Invalid);
  assert(setPin(52,Channel::FuelLower,Type::ReedNC,5001,0,saved)==PinResult::Invalid);
  unchanged(empty);assert(config_host::writes==0&&saved==123);
  for(const int pin:allowed){
    const auto before=state();const auto writes=config_host::writes;
    assert(setPin(pin,Channel::Room,Type::DS18B20,50,before.revision,saved)==PinResult::Saved);
    assert(saved==before.revision+1&&state().item[0].gpio==pin&&config_host::writes==writes+1);
  }
  for(const Type type:{Type::ReedNO,Type::ReedNC})for(const uint16_t debounce:{0,50,5000}){
    assert(setPin(52,Channel::FuelLower,type,debounce,state().revision,saved)==PinResult::Saved);
    assert(state().item[4].type==type&&state().item[4].debounceMs==debounce);
  }
}

void atomicOwnersAndRevision(){
  State x,y;twoAccepted(x,y);uint32_t saved=0;
  const unsigned writes=config_host::writes;
  // Move Outside from GPIO51 onto Room's GPIO52 in one verified slot write.
  assert(setPin(52,Channel::Outside,Type::DS18B20,50,2,saved)==PinResult::Saved);
  assert(saved==3&&config_host::writes==writes+1);
  const auto moved=state();
  assert(moved.item[0].type==Type::None&&moved.item[0].gpio==-1);
  assert(moved.item[1].type==Type::DS18B20&&moved.item[1].gpio==52);
  for(unsigned i=2;i<COUNT;i++)assert(moved.item[i].type==Type::None);
  // Legacy set retains its collision rejection; it is not a hidden second write.
  assert(!set(Channel::Coolant,Type::DS18B20,52,0,50));unchanged(moved);
  assert(config_host::writes==writes+1);
  assert(rollback());assert(state().revision==4&&sameBindings(state(),y));
  assert(rollback());assert(state().revision==5&&sameBindings(state(),moved));
  for(const Type remote:{Type::RemoteModbus,Type::RemoteMQTT}){
    assert(set(Channel::Coolant,remote,-1,remote==Type::RemoteModbus?7:0,50));
    const auto old=state();const unsigned beforeWrites=config_host::writes;
    assert(setPin(52,Channel::Coolant,Type::DS18B20,50,old.revision,saved)==PinResult::Saved);
    const auto after=state();assert(config_host::writes==beforeWrites+1);
    assert(after.item[1].type==Type::None&&after.item[2].gpio==52&&after.item[2].busAddress==0);
  }
  const auto assigned=state();const unsigned beforeNoOp=config_host::writes;
  assert(setPin(52,Channel::Coolant,Type::DS18B20,50,assigned.revision,saved)==PinResult::Saved);
  assert(saved==assigned.revision&&config_host::writes==beforeNoOp);unchanged(assigned);
  assert(setPin(52,Channel::Room,Type::DS18B20,50,assigned.revision-1,saved)==PinResult::Conflict);
  assert(clearPin(52,assigned.revision-1,saved)==PinResult::Conflict);
  assert(clearPin(34,assigned.revision-1,saved)==PinResult::Conflict);
  unchanged(assigned);assert(config_host::writes==beforeNoOp);
  assert(clearPin(52,assigned.revision,saved)==PinResult::Saved);
  assert(saved==assigned.revision+1&&state().item[2].type==Type::None);
  const auto cleared=state();const unsigned beforeClearNoOp=config_host::writes;
  assert(clearPin(52,cleared.revision,saved)==PinResult::Saved);
  assert(clearPin(34,cleared.revision,saved)==PinResult::Saved);
  assert(saved==cleared.revision&&config_host::writes==beforeClearNoOp);unchanged(cleared);
  // A competing legacy writer finishes immediately before this caller acquires the mutex.
  config_host::beforeTake=[](){assert(set(Channel::Room,Type::DS18B20,34,0,50));};
  assert(setPin(52,Channel::Outside,Type::DS18B20,50,cleared.revision,saved)==PinResult::Conflict);
  assert(state().revision==cleared.revision+1&&state().item[0].gpio==34&&state().item[1].type==Type::None);
  assert(config_host::writes==beforeClearNoOp+1);
}

void storageRecovery(){
  for(unsigned fault=0;fault<5;fault++){
    State x,y;twoAccepted(x,y);const auto beforeNvs=config_host::nvs;uint32_t saved=555;
    if(fault==0)config_host::failWrite=true;
    if(fault==1)config_host::writeThenFail=true;
    if(fault==2)config_host::partialWrite=true;
    if(fault==3)config_host::afterWrite=[](){config_host::failRead=true;};
    if(fault==4)config_host::afterWrite=[](){config_host::corruptRead=true;};
    assert(setPin(52,Channel::Outside,Type::DS18B20,50,y.revision,saved)==PinResult::StorageError);
    assert(saved==555);unchanged(y);assert(config_host::nvs==beforeNvs);
    reboot();unchanged(y);
    assert(rollback());assert(state().revision==3&&sameBindings(state(),x));
    assert(rollback());assert(state().revision==4&&sameBindings(state(),y));
    reboot();assert(state().revision==4&&sameBindings(state(),y));
  }
  State x,y;twoAccepted(x,y);uint32_t saved=333;const auto beforeNvs=config_host::nvs;
  unsigned writes=config_host::writes;
  // Existing inactive data cannot be backed up: refuse before overwriting anything.
  config_host::failRead=true;
  assert(setPin(28,Channel::Coolant,Type::DS18B20,50,2,saved)==PinResult::StorageError);
  assert(config_host::writes==writes&&config_host::nvs==beforeNvs);unchanged(y);
  config_host::failWriteBegin=true;
  assert(clearPin(52,2,saved)==PinResult::StorageError);
  assert(config_host::writes==writes&&config_host::nvs==beforeNvs);unchanged(y);
  // Clear and rollback use the same verified commit/recovery path.
  config_host::afterWrite=[](){config_host::failRead=true;};
  assert(clearPin(52,2,saved)==PinResult::StorageError);unchanged(y);
  assert(config_host::nvs==beforeNvs);
  config_host::afterWrite=[](){config_host::failRead=true;};
  assert(!rollback());unchanged(y);assert(config_host::nvs==beforeNvs);
  assert(rollback());assert(state().revision==3&&sameBindings(state(),x));
  assert(rollback());assert(state().revision==4&&sameBindings(state(),y));
  // First-ever failed candidate has no preimage, so cleanup removes its blob.
  fresh();const auto empty=state();config_host::afterWrite=[](){config_host::failRead=true;};
  assert(setPin(52,Channel::Room,Type::DS18B20,50,0,saved)==PinResult::StorageError);
  unchanged(empty);assert(config_host::nvs.empty()&&config_host::removes==1);
  reboot();unchanged(empty);assert(!rollback());
}

void persistentFaultBoundaryAndWrap(){
  State x,y;twoAccepted(x,y);uint32_t saved=0;
  config_host::afterWrite=[](){config_host::failRead=true;config_host::failAllWrites=true;};
  assert(setPin(28,Channel::Coolant,Type::DS18B20,50,2,saved)==PinResult::StorageError);
  unchanged(y);
  // Failed restoration leaves a newer candidate in NVS; it cannot become a rollback.
  assert(!rollback());unchanged(y);
  config_host::resetFaults();reboot();
  // Deliberately document the dual-slot recovery limit: after permanent cleanup failure,
  // reboot can select that candidate. This is not a durable commit protocol.
  assert(state().revision==3&&state().item[2].gpio==28);
  fresh();current.revision=std::numeric_limits<uint32_t>::max()-2;
  assert(setPin(52,Channel::Room,Type::DS18B20,50,current.revision,saved)==PinResult::Saved);x=state();
  assert(setPin(51,Channel::Outside,Type::DS18B20,50,current.revision,saved)==PinResult::Saved);y=state();
  assert(saved==std::numeric_limits<uint32_t>::max());
  assert(setPin(28,Channel::Coolant,Type::DS18B20,50,saved,saved)==PinResult::Saved);
  const auto wrapped=state();assert(saved==0);
  reboot();unchanged(wrapped);
  assert(rollback());assert(state().revision==1&&sameBindings(state(),y));
  assert(rollback());assert(state().revision==2&&sameBindings(state(),wrapped));
  // Equal revisions are not an older accepted configuration either.
  const std::string active=activeSlot?"bp_cfg_b":"bp_cfg_a",inactive=activeSlot?"bp_cfg_a":"bp_cfg_b";
  config_host::nvs[inactive]=config_host::nvs[active];const auto before=state();
  assert(!rollback());unchanged(before);
}

void migrateLegacySlots(){
  // Exact schema-1 layout: six 8-byte assignments, then a 32-bit FNV checksum.
  auto legacy=[](uint32_t revision){
    uint8_t raw[]={
      0x36,0x43,0x50,0x42,1,0,0,0,0,0,0,0,
      1,0,52,0,0,0,50,0, 0,0,255,255,0,0,50,0,
      1,0,51,0,0,0,50,0, 5,0,255,255,0,0,50,0,
      3,0,29,0,0,0,75,0, 2,0,30,0,0,0,80,0, 0,0,0,0
    };
    static_assert(sizeof(raw)==64,"legacy ESP32 config size");
    memcpy(raw+8,&revision,4);
    const uint32_t hash=fnv(raw,60);
    memcpy(raw+60,&hash,4);
    return std::vector<uint8_t>(raw,raw+sizeof(raw));
  };
  fresh();config_host::nvs["bp_cfg_a"]=legacy(7);config_host::nvs["bp_cfg_b"]=legacy(6);
  const auto originals=config_host::nvs;reboot();const auto before=state();
  assert(before.revision==7&&before.item[0].gpio==52&&before.item[2].gpio==51);
  assert(before.item[3].type==Type::RemoteMQTT&&before.item[1].type==Type::None);
  assert(before.item[4].type==Type::ReedNC&&before.item[4].gpio==29&&before.item[4].debounceMs==75);
  assert(before.item[5].type==Type::ReedNO&&before.item[5].gpio==30&&before.item[5].debounceMs==80);
  assert(before.item[6].type==Type::None&&before.item[6].gpio==-1&&before.item[7].gpio==-1&&config_host::nvs==originals);
  config_host::afterWrite=[](){config_host::failRead=true;};
  uint32_t saved=0;
  assert(setPin(28,Channel::CoolantReturn,Type::DS18B20,50,7,saved)==PinResult::StorageError);
  assert(config_host::nvs==originals);reboot();unchanged(before);
  assert(setPin(28,Channel::CoolantReturn,Type::DS18B20,50,7,saved)==PinResult::Saved&&saved==8);
  reboot();assert(state().revision==8&&state().item[2].gpio==51&&state().item[6].gpio==28);
  assert(rollback());reboot();assert(state().revision==9&&sameBindings(state(),before));
  config_host::nvs=originals;config_host::nvs["bp_cfg_a"][27]^=0x80;
  reboot();assert(state().revision==6&&sameBindings(state(),before));
  static_assert(temperatureIndex(2)==2&&temperatureIndex(6)==4&&temperatureIndex(4)==-1&&temperatureIndex(5)==-1,
                "supply, return and float switches have distinct destinations");
}

void migrateSevenChannelSlots(){
  // Schema 2 as written by v0.10.9/v0.10.10: seven 8-byte assignments.
  uint8_t bytes[]={
    0x36,0x43,0x50,0x42,2,0,0,0,17,0,0,0,
    1,0,52,0,0,0,50,0, 0,0,255,255,0,0,50,0,
    1,0,51,0,0,0,50,0, 1,0,33,0,0,0,50,0,
    3,0,29,0,0,0,75,0, 2,0,30,0,0,0,80,0,
    1,0,28,0,0,0,50,0, 0,0,0,0
  };
  static_assert(sizeof(bytes)==72,"seven-channel ESP32 config size");
  const uint32_t hash=fnv(bytes,68);
  memcpy(bytes+68,&hash,4);
  fresh();
  config_host::nvs["bp_cfg_a"]=std::vector<uint8_t>(bytes,bytes+sizeof(bytes));
  config_host::nvs["bp_cfg_b"]=config_host::nvs["bp_cfg_a"];
  const auto originals=config_host::nvs;
  reboot();const State before=state();
  assert(before.revision==17&&before.item[3].gpio==33&&before.item[6].gpio==28);
  assert(before.item[7].type==Type::None&&before.item[7].gpio==-1&&config_host::nvs==originals);
  uint32_t saved=0;config_host::afterWrite=[](){config_host::failRead=true;};
  assert(setPin(49,Channel::FuelOutlet,Type::DS18B20,50,17,saved)==PinResult::StorageError);
  assert(config_host::nvs==originals);reboot();unchanged(before);
  assert(setPin(49,Channel::FuelOutlet,Type::DS18B20,50,17,saved)==PinResult::Saved&&saved==18);
  reboot();const State after=state();
  for(unsigned i=0;i<7;i++)assert(!memcmp(&before.item[i],&after.item[i],sizeof(Assignment)));
  assert(after.item[7].type==Type::DS18B20&&after.item[7].gpio==49);
  assert(rollback());reboot();assert(state().revision==19&&sameBindings(state(),before));
  assert(rollback());reboot();assert(state().revision==20&&sameBindings(state(),after));
  static_assert(temperatureIndex(3)==3&&temperatureIndex(7)==5&&temperatureIndex(8)==-1,
                "tank and outlet temperatures have distinct destinations");
}

int main(){
  validationAndAvailability();atomicOwnersAndRevision();storageRecovery();persistentFaultBoundaryAndWrap();migrateLegacySlots();migrateSevenChannelSlots();
  powerOff();
  puts("PASS: real config/NVS 11-pin validation, atomic owner replacement/move, remote replacement, clear/no-op, mutex revision race, write/read-back recovery, repeated rollback, transient-fault reboot and revision wrap; persistent cleanup failure boundary demonstrated");
}
