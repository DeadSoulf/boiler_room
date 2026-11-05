#include "panel_update_protocol.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
using namespace panel_update::policy;

static std::string replace(std::string s,const std::string& a,const std::string& b){
  const auto at=s.find(a);assert(at!=std::string::npos);s.replace(at,a.size(),b);return s;
}
static std::string manifest(){
  return R"({"schema":1,"project":"DeadSoulf/boiler_room","board":"jc4880p443","chip":"esp32p4","chip_revision_min":103,"chip_revision_max":103,"flash_bytes":16777216,"partition_layout":"guition-16mb-ota-640000-v1","version":"v1.2.3","source_commit":"0123456789abcdef0123456789abcdef01234567","firmware":{"name":"BoilerPanel-v1.2.3-firmware.bin","size":100000,"sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","url":"https://github.com/DeadSoulf/boiler_room/releases/download/v1.2.3/BoilerPanel-v1.2.3-firmware.bin"}})";
}
static void versions(){
  Version v;
  for(const char* s:{"v0.0.0","v1.2.3","v10.100.1000","v4294967295.1.2"})assert(version(s,v));
  for(const char* s:{"","1.2.3","v01.2.3","v1.02.3","v1.2.03","v1.2.3-rc1","v1.2.3+build","v1.2","v1.2.3\n","v4294967296.0.0","v1..2","v1.2.-3","v1.2.3.4"})assert(!version(s,v));
  assert(newer("v0.10.0","v0.9.9"));assert(newer("v2.0.0","v1.999.999"));
  assert(!newer("v1.2.3","v1.2.3"));assert(!newer("v1.9.9","v1.10.0"));assert(!newer("v1.2.3-rc1","v1.0.0"));
}
static void urls(){
  char original[256];assert(assetUrl(original,sizeof(original),"v1.2.3",MANIFEST_NAME));
  assert(allowedUrl(LATEST_URL,Resource::Api));assert(allowedUrl(original,Resource::Asset,original));
  const char* cdn="https://release-assets.githubusercontent.com/github-production-release-asset/1234/abcd-1234?sp=r&se=2026-11-05T12%3A00%3A00Z&sig=a%2Bb%3D";
  assert(allowedUrl(cdn,Resource::Asset,original));
  assert(allowedUrl("https://objects.githubusercontent.com:443/github-production-release-asset/1234/abcdef",Resource::Asset,original));
  const std::vector<std::string> forbidden={
    "http://github.com/DeadSoulf/boiler_room/releases/download/v1.2.3/boiler-panel-ota.json",
    std::string(original)+"?redirect=evil",std::string(original)+"#ignored",
    "https://github.com.evil.example/github-production-release-asset/123/abc",
    "https://release-assets.githubusercontent.com.evil.example/github-production-release-asset/123/abc",
    "https://release-assets.githubusercontent.com@evil.example/github-production-release-asset/123/abc",
    "https://attacker@release-assets.githubusercontent.com/github-production-release-asset/123/abc",
    "https://release-assets.githubusercontent.com:444/github-production-release-asset/123/abc",
    "https://release-assets.githubusercontent.com./github-production-release-asset/123/abc",
    "https://release-assets.githubusercontent.com/github-production-release-asset/../secret",
    "https://release-assets.githubusercontent.com/github-production-release-asset/./secret",
    "https://release-assets.githubusercontent.com/github-production-release-asset//123/abc",
    "https://release-assets.githubusercontent.com/github-production-release-asset/%2e%2e/abc",
    "https://release-assets.githubusercontent.com/github-production-release-asset/123/abc?sig=%0d%0aevil",
    "https://release-assets.githubusercontent.com/github-production-release-asset/123/abc?sig=%GG",
    "https://release-assets.githubusercontent.com/github-production-release-asset/123/abc?sig=%",
    "https://release-assets.githubusercontent.com/github-production-release-asset/123/abc\\@evil",
    "https://release-assets.githubusercontent.com/github-production-release-asset/123/abc\n",
    "https://release-assets.githubusercontent.com/other/123/abc",
    "https://github.com/attacker/boiler_room/releases/download/v1.2.3/boiler-panel-ota.json",
    "https://api.github.com/repos/DeadSoulf/boiler_room/releases/latest"
  };
  for(const auto& s:forbidden)assert(!allowedUrl(s.c_str(),Resource::Asset,original));
  assert(!allowedUrl(cdn,Resource::Api));assert(!allowedUrl((std::string(LATEST_URL)+"?x=1").c_str(),Resource::Api));
  assert(!allowedUrl((std::string(cdn)+std::string(2048,'x')).c_str(),Resource::Asset,original));
}
static void manifests(){
  const std::string good=manifest();Manifest m;
  assert(parseManifest(good.data(),good.size(),"v1.2.3",m));assert(m.size==100000);
  const std::vector<std::pair<std::string,std::string>> changes={
    {"\"schema\":1","\"schema\":2"},{"DeadSoulf/boiler_room","Other/boiler_room"},
    {"jc4880p443","jc4880p444"},{"\"esp32p4\"","\"esp32s3\""},
    {"\"chip_revision_min\":103","\"chip_revision_min\":102"},{"\"chip_revision_max\":103","\"chip_revision_max\":104"},
    {"16777216","8388608"},{"guition-16mb-ota-640000-v1","different-layout"},
    {"\"version\":\"v1.2.3\"","\"version\":\"v1.2.4\""},
    {"0123456789abcdef0123456789abcdef01234567","g123456789abcdef0123456789abcdef01234567"},
    {"\"size\":100000","\"size\":0"},{"\"size\":100000","\"size\":65535"},
    {"\"size\":100000","\"size\":6553601"},{"\"size\":100000","\"size\":1e5"},
    {"\"size\":100000","\"size\":100000.0"},{"\"size\":100000","\"size\":-100000"},
    {"\"size\":100000","\"size\":0100000"},{"\"size\":100000","\"size\":4294967296"},
    {"\"size\":100000","\"size\":100000,\"size\":100000"},
    {"\"schema\":1","\"schema\":1,\"schema\":1"},
    {"https://github.com/DeadSoulf","http://github.com/DeadSoulf"},
    {"\"name\":\"BoilerPanel-v1.2.3-firmware.bin\"","\"name\":\"../firmware.bin\""},
    {"\"sha256\":\"0","\"sha256\":\"G"},
    {"\"v1.2.3\"","\"v1.2.3-rc1\""},{"\"v1.2.3\"","\"v1.2.3\\u0000\""}
  };
  for(const auto& c:changes){auto bad=replace(good,c.first,c.second);assert(!parseManifest(bad.data(),bad.size(),"v1.2.3",m));}
  for(size_t n=0;n<good.size();n++)assert(!parseManifest(good.data(),n,"v1.2.3",m));
  assert(!parseManifest(good.data(),good.size(),"v1.2.4",m));
  auto junk=good+"{}";assert(!parseManifest(junk.data(),junk.size(),"v1.2.3",m));
  auto whitespace=" \r\n"+good+"\t";assert(parseManifest(whitespace.data(),whitespace.size(),"v1.2.3",m));
  auto depth=replace(good,"\"schema\":1","\"ignored\":"+std::string(14,'[')+"0"+std::string(14,']')+",\"schema\":1");
  assert(!parseManifest(depth.data(),depth.size(),"v1.2.3",m));
}
static void releases(){
  const std::string good=R"({"tag_name":"v1.2.3","draft":false,"prerelease":false,"body":"Text\n\u0410","assets":[{"name":"boiler-panel-ota.json","size":612,"browser_download_url":"https://github.com/DeadSoulf/boiler_room/releases/download/v1.2.3/boiler-panel-ota.json"},{"name":"BoilerPanel-v1.2.3-firmware.bin","size":100000,"browser_download_url":"https://github.com/DeadSoulf/boiler_room/releases/download/v1.2.3/BoilerPanel-v1.2.3-firmware.bin"}]})";
  Release release;assert(parseRelease(good.data(),good.size(),release));assert(release.firmwareSize==100000);
  for(const auto& c:std::vector<std::pair<std::string,std::string>>{
      {"\"draft\":false","\"draft\":true"},{"\"prerelease\":false","\"prerelease\":true"},
      {"\"tag_name\":\"v1.2.3\"","\"tag_name\":\"v1.2.3\",\"tag_name\":\"v1.2.4\""},
      {"\"size\":612","\"size\":4097"},{"\"size\":100000","\"size\":6553601"},
      {"github.com/DeadSoulf","github.com/attacker"},{"\"draft\":false,",""},
      {"boiler-panel-ota.json","wrong.json"},{"\"v1.2.3\"","\"v1.2.3-beta\""}}){
    auto bad=replace(good,c.first,c.second);assert(!parseRelease(bad.data(),bad.size(),release));
  }
  const auto first=good.find("{\"name\":");const auto end=good.find("}",first)+1;
  auto dup=good;dup.insert(end,","+good.substr(first,end-first));assert(!parseRelease(dup.data(),dup.size(),release));
  for(size_t n=0;n<good.size();n++)assert(!parseRelease(good.data(),n,release));
}
static void imagesAndTransfers(){
  Hardware hardware{FLASH_BYTES,SLOT0,SLOT1,SLOT_BYTES,CHIP_ID,CHIP_REVISION};assert(validHardware(hardware));
  hardware.revision=104;assert(!validHardware(hardware));hardware.revision=103;hardware.slot1+=4096;assert(!validHardware(hardware));
  uint8_t prefix[36]={};prefix[0]=0xe9;prefix[1]=4;prefix[3]=0x40;prefix[12]=18;
  prefix[15]=103;prefix[17]=255;prefix[18]=255;prefix[23]=1;
  prefix[28]=0;prefix[29]=1;prefix[32]=0x32;prefix[33]=0x54;prefix[34]=0xcd;prefix[35]=0xab;
  assert(validImagePrefix(prefix,sizeof(prefix)));assert(!validImagePrefix(prefix,35));
  for(unsigned at:{0U,1U,3U,12U,15U,23U,32U}){
    const uint8_t old=prefix[at];prefix[at]=at==15?104:0;assert(!validImagePrefix(prefix,sizeof(prefix)));prefix[at]=old;
  }
  uint8_t digest[32]={},wrong[32]={};wrong[17]=1;
  TransferGate complete(100000);complete.header(true);assert(complete.add(100000));complete.digest(digest,digest);
  assert(complete.canEnd()&&!complete.canSelect());complete.end(true);assert(complete.canSelect());
  for(unsigned point=0;point<5;point++){
    TransferGate cancelled(100000);if(point==0)cancelled.cancel();cancelled.header(true);
    if(point==1)cancelled.cancel();
    cancelled.add(100000);if(point==2)cancelled.cancel();
    cancelled.digest(digest,digest);if(point==3)cancelled.cancel();cancelled.end(true);
    if(point==4)cancelled.cancel();
    assert(!cancelled.canSelect());
  }
  for(unsigned fault=0;fault<5;fault++){
    TransferGate t(100000);t.header(fault!=0);t.add(fault==1?99999:100000);t.digest(digest,fault==2?wrong:digest);
    if(fault==3)t.add(1);
    t.end(fault!=4);assert(!t.canSelect());
  }
  TransferGate invalid(65535);invalid.header(true);invalid.add(65535);invalid.digest(digest,digest);invalid.end(true);assert(!invalid.canSelect());
  assert(observeBoot(SLOT1,"v1.2.3",SLOT0,"v1.0.0",SLOT1,"v1.2.3",true,false,true,false)==BootObservation::Pending);
  assert(observeBoot(SLOT1,"v1.2.3",SLOT0,"v1.0.0",SLOT1,"v1.2.3",false,true,false,false)==BootObservation::Confirmed);
  assert(observeBoot(SLOT0,"v1.0.0",SLOT0,"v1.0.0",SLOT1,"v1.2.3",true,false,false,true)==BootObservation::RolledBack);
  assert(observeBoot(SLOT0,"v1.0.0",SLOT0,"v1.0.0",SLOT1,"v1.2.3",true,false,false,false)==BootObservation::Interrupted);
  assert(observeBoot(SLOT1,"v1.2.2",SLOT0,"v1.0.0",SLOT1,"v1.2.3",true,false,true,false)==BootObservation::Mismatch);
}
int main(){versions();urls();manifests();releases();imagesAndTransfers();
  std::cout<<"PASS: update version/URL/manifest/release/image policy, cancellation/size/hash/end gates and boot outcomes\n";
}
