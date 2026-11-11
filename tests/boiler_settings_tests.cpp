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
  if(std::string(argv[1])=="reboot"){
    std::ifstream file(argv[2],std::ios::binary);
    file.read(reinterpret_cast<char*>(&f::word),sizeof(f::word));assert(file.good());
    f::key=f::space=true;
    assert(begin() && snapshot(settings) && settings.configured && settings.revision==2);
    assert((settings.onC==15 && settings.offC==30)||(settings.onC==18 && settings.offC==35));
    assert(f::writes==0);
    const auto previous=f::word;
    // An unsupported schema/read error is never initialized as a new pair.
    f::word^=1ULL<<63;
    assert(!begin() && !snapshot(settings) && !settings.configured);
    assert(save(0,12,22)==Result::Unavailable && f::writes==0);
    f::word=previous;f::badRead=true;
    assert(!snapshot(settings) && save(0,12,22)==Result::Unavailable && f::writes==0);
    f::badRead=false;assert(snapshot(settings) && settings.revision==2);
    return 0;
  }
  assert(begin() && snapshot(settings) && !settings.configured && settings.revision==0);
  assert(save(0,-1,5)==Result::Invalid && save(0,11,100)==Result::Invalid);
  assert(save(0,11,11)==Result::Invalid && save(0,21,10)==Result::Invalid && !f::key);
  // First write looks committed, but fresh-handle readback detects a bad value.
  f::failVerifyOnce=true;assert(save(0,10,20)==Result::Unavailable && !f::key);
  assert(snapshot(settings) && !settings.configured);
  assert(save(0,0,99)==Result::Saved);
  assert(snapshot(settings) && settings.configured && settings.revision==1 && settings.onC==0 && settings.offC==99);
  const auto stored=f::word;const auto writes=f::writes;
  assert(save(1,0,99)==Result::Saved && f::writes==writes);
  assert(save(0,0,99)==Result::Conflict && f::writes==writes);
  // Simulate NVS making the new integer visible before reporting commit failure.
  f::badCommitOnce=true;assert(save(1,10,20)==Result::Unavailable && f::word==stored);
  assert(snapshot(settings) && settings.revision==1 && settings.offC==99);
  f::denyLock=true;
  assert(!snapshot(settings) && !settings.configured && save(1,10,20)==Result::Unavailable);
  f::denyLock=false;
  Result left=Result::Invalid,right=Result::Invalid;
  std::thread physical([&]{left=save(1,15,30);});
  std::thread web([&]{right=save(1,18,35);});
  physical.join();web.join();
  assert((left==Result::Saved && right==Result::Conflict)||(right==Result::Saved && left==Result::Conflict));
  // Only the bytes stored by the real backend cross into the fresh test process.
  std::ofstream file(argv[2],std::ios::binary);file.write(reinterpret_cast<const char*>(&f::word),sizeof(f::word));
  assert(file.good());
}
