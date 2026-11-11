#include "panel_boiler.h"
#include "fake.h"
#include <fstream>
#include <string>
#include <thread>
using namespace panel_boiler;
namespace f=fixture;

int main(int argc,char** argv){
  assert(argc==3);
  Settings settings;
  constexpr auto electric=Target::Electric;
  if(std::string(argv[1])=="reboot"){
    std::ifstream file(argv[2],std::ios::binary);
    file.read(reinterpret_cast<char*>(&f::dhw.word),sizeof(f::dhw.word));assert(file.good());
    file.read(reinterpret_cast<char*>(&f::electric.word),sizeof(f::electric.word));assert(file.good());
    f::electric.key=f::electric.space=true;
    f::dhw.key=f::dhw.space=true;
    assert(begin() && snapshot(settings) && settings.configured && settings.revision==2);
    assert((settings.onC==15 && settings.offC==30)||(settings.onC==18 && settings.offC==35));
    assert(snapshot(settings,electric) && settings.revision==1 && settings.onC==32 && settings.offC==46);
    assert(f::dhw.writes==0 && f::electric.writes==0);
    const auto previous=f::dhw.word;
    // An unsupported schema/read error is never initialized as a new pair.
    f::dhw.word^=1ULL<<63;
    assert(!begin() && !snapshot(settings) && !settings.configured);
    assert(save(0,12,22)==Result::Unavailable && f::dhw.writes==0);
    assert(snapshot(settings,electric) && settings.revision==1);
    assert(save(1,31,48,electric)==Result::Saved);
    f::dhw.word=previous;f::dhw.badRead=true;
    assert(!snapshot(settings) && save(0,12,22)==Result::Unavailable && f::dhw.writes==0);
    f::dhw.badRead=false;assert(snapshot(settings) && settings.revision==2);
    return 0;
  }
  assert(begin() && snapshot(settings) && !settings.configured && settings.revision==0);
  assert(save(0,-1,5)==Result::Invalid && save(0,11,100)==Result::Invalid);
  assert(save(0,11,11)==Result::Invalid && save(0,21,10)==Result::Invalid && !f::dhw.key);
  // First write looks committed, but fresh-handle readback detects a bad value.
  f::dhw.failVerifyOnce=true;assert(save(0,10,20)==Result::Unavailable && !f::dhw.key);
  assert(snapshot(settings) && !settings.configured);
  assert(save(0,0,99)==Result::Saved);
  assert(snapshot(settings) && settings.configured && settings.revision==1 && settings.onC==0 && settings.offC==99);
  const auto stored=f::dhw.word;const auto writes=f::dhw.writes;
  assert(save(1,0,99)==Result::Saved && f::dhw.writes==writes);
  assert(save(0,0,99)==Result::Conflict && f::dhw.writes==writes);
  // Simulate NVS making the new integer visible before reporting commit failure.
  f::dhw.badCommitOnce=true;assert(save(1,10,20)==Result::Unavailable && f::dhw.word==stored);
  assert(snapshot(settings) && settings.revision==1 && settings.offC==99);
  f::denyLock=true;
  assert(!snapshot(settings) && !settings.configured && save(1,10,20)==Result::Unavailable);
  f::denyLock=false;
  Result left=Result::Invalid,right=Result::Invalid;
  std::thread physical([&]{left=save(1,15,30);});
  std::thread web([&]{right=save(1,18,35);});
  physical.join();web.join();
  assert((left==Result::Saved && right==Result::Conflict)||(right==Result::Saved && left==Result::Conflict));
  // A populated DHW record must not initialize or seed the electric pair.
  assert(begin() && snapshot(settings,electric) && !settings.configured && !settings.revision);
  const auto dhwWord=f::dhw.word;
  assert(save(0,99,99,electric)==Result::Invalid && !f::electric.key);
  f::electric.failVerifyOnce=true;
  assert(save(0,32,46,electric)==Result::Unavailable && !f::electric.key && f::dhw.word==dhwWord);
  assert(save(0,32,46,electric)==Result::Saved && f::dhw.word==dhwWord);
  const auto electricWord=f::electric.word;
  assert(save(0,10,30,electric)==Result::Conflict && f::electric.word==electricWord);
  f::electric.badCommitOnce=true;
  assert(save(1,25,42,electric)==Result::Unavailable && f::electric.word==electricWord);
  f::electric.badRead=true;
  assert(!begin() && !snapshot(settings,electric) && snapshot(settings) && settings.revision==2);
  assert(save(1,10,30,electric)==Result::Unavailable && f::dhw.word==dhwWord);
  f::electric.badRead=false;
  assert(snapshot(settings,electric) && settings.revision==1 && settings.offC==46);
  // Only the bytes stored by the real backend cross into the fresh test process.
  std::ofstream file(argv[2],std::ios::binary);file.write(reinterpret_cast<const char*>(&f::dhw.word),sizeof(f::dhw.word));
  file.write(reinterpret_cast<const char*>(&f::electric.word),sizeof(f::electric.word));assert(file.good());
}
