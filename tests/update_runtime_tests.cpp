// Actual panel_update.cpp with isolated, fault-injectable host implementations.
#include "../src/panel_update.cpp"
#include <iostream>
namespace panel_radio {bool snapshot(panel::RadioState& out){out.connected=wifi;return true;}}
namespace panel_storage {bool event(const char*,const char*){return true;}}
using namespace panel_update;

static std::string digest(const std::vector<uint8_t>& data){
  uint8_t h[32];fakeDigest(data.data(),data.size(),h);char out[65];
  for(unsigned i=0;i<32;i++)snprintf(out+2*i,3,"%02x",unsigned(h[i]));
  return out;
}
static ::Response response(const std::string& body){::Response r;r.body.assign(body.begin(),body.end());return r;}
static void reset(){
  if(!guard)assert(begin());
  if(bootDeadline){delete bootDeadline;bootDeadline=nullptr;}
  nvs.clear();nvs["user-wifi"]={1,2,3,4};nvsWrites=failNvsWrite=0;
  requests->has=false;responses.clear();requestedUrls.clear();trace.clear();faultHook={};persistenceHook={};
  fakeMillis=12000;runningIndex=bootIndex=0;partitionStates[0]=ESP_OTA_IMG_VALID;partitionStates[1]=ESP_OTA_IMG_UNDEFINED;
  images[0]=makeImage();images[1]=makeImage();otaData.assign(8192,255);makeTable();
  wifi=rollbackPossible=true;badBootloader=failVerifyTarget=failMarkValid=false;
  failBegin=failEnd=failBoot=bootFailureAfterSelect=failTimer=failTimerStart=false;
  badBootReadbackOnce=bootReadbackFault=false;
  otaBegins=otaWrites=otaEnds=otaAborts=bootSelections=restarts=failWriteAt=0;
  state=Snapshot{};state.started=state.supported=true;state.busy=state.cancelAllowed=true;
  cancelRequested.store(false);heartbeatAt.store(fakeMillis);healthySince.store(1000);heartbeatReady.store(true);
  bootConfirmed.store(false);record=Record{};haveRecord=haveOffer=pendingBoot=pendingIdentityOK=false;
  assert(layoutReady());assert(bootloaderMatches());platformReady=true;
}
static void offer(){
  const std::string hash=digest(makeImage());
  const std::string text=std::string("{\"schema\":1,\"project\":\"DeadSoulf/boiler_room\",\"board\":\"jc4880p443\",\"chip\":\"esp32p4\",\"chip_revision_min\":103,\"chip_revision_max\":103,\"flash_bytes\":16777216,\"partition_layout\":\"guition-16mb-ota-640000-v1\",\"version\":\"v99.1.2\",\"source_commit\":\"0123456789abcdef0123456789abcdef01234567\",\"firmware\":{\"name\":\"BoilerPanel-v99.1.2-firmware.bin\",\"size\":100000,\"sha256\":\"")+hash+"\",\"url\":\"https://github.com/DeadSoulf/boiler_room/releases/download/v99.1.2/BoilerPanel-v99.1.2-firmware.bin\"}}";
  assert(policy::parseManifest(text.data(),text.size(),"v99.1.2",offered));haveOffer=true;
  release=policy::Release{};policy::copy(release.version,offered.version);
  assert(policy::assetUrl(release.manifestUrl,sizeof(release.manifestUrl),offered.version,policy::MANIFEST_NAME));
  policy::copy(release.firmwareUrl,offered.url);release.firmwareSize=offered.size;release.manifestSize=uint32_t(text.size());
  responses[release.manifestUrl]=response(text);::Response firmware;firmware.body=makeImage();responses[offered.url]=firmware;
  state.available=true;policy::copy(state.latestVersion,offered.version);
}
static bool install(){try{installRelease("v99.1.2");}catch(const Restart&){return true;}return false;}
static void noSelection(){assert(bootSelections==0&&bootIndex==runningIndex&&restarts==0);assert(nvs["user-wifi"]==std::vector<uint8_t>({1,2,3,4}));}
static void checks(){
  for(unsigned scenario=0;scenario<3;scenario++){
    reset();offer();
    const std::string selected=scenario?panel::VERSION:"v99.1.2";
    auto bytes=responses[release.manifestUrl].body;std::string manifest(bytes.begin(),bytes.end());
    for(size_t at=0;(at=manifest.find("v99.1.2",at))!=std::string::npos;at+=selected.size())manifest.replace(at,7,selected);
    if(scenario==2){const size_t at=manifest.find("jc4880p443");assert(at!=std::string::npos);manifest.replace(at,10,"otherboard");}
    char manifestUrl[256],name[80],firmwareUrl[256];
    assert(policy::assetUrl(manifestUrl,sizeof(manifestUrl),selected.c_str(),policy::MANIFEST_NAME));
    assert(policy::firmwareName(name,sizeof(name),selected.c_str()));
    assert(policy::assetUrl(firmwareUrl,sizeof(firmwareUrl),selected.c_str(),name));
    responses[manifestUrl]=response(manifest);
    const std::string api=std::string("{\"tag_name\":\"")+selected+"\",\"draft\":false,\"prerelease\":false,\"assets\":[{\"name\":\"boiler-panel-ota.json\",\"size\":"+std::to_string(manifest.size())+",\"browser_download_url\":\""+manifestUrl+"\"},{\"name\":\""+name+"\",\"size\":100000,\"browser_download_url\":\""+firmwareUrl+"\"}]}";
    responses[policy::LATEST_URL]=response(api);haveOffer=false;state.available=false;
    checkRelease();noSelection();assert(otaBegins==0&&requestedUrls.size()==2);
    assert(requestedUrls[0]==policy::LATEST_URL&&requestedUrls[1]==manifestUrl);
    if(!scenario){assert(state.phase==Phase::Available&&state.available&&!state.busy);assert(!requestInstall("v99.1.3"));}
    else if(scenario==1)assert(state.phase==Phase::Current&&!state.available&&!haveOffer);
    else assert(state.phase==Phase::Error&&!state.available&&!haveOffer);
  }
}
static void transfers(){
  reset();offer();assert(install());assert(bootSelections==1&&otaEnds==1&&otaAborts==0&&record.stage==uint32_t(Stage::Armed));
  assert(trace[trace.size()-2]=="select"&&trace.back()=="restart");assert(state.phase==Phase::Rebooting);
  reset();offer();responses[offered.url].body[99999]^=1;assert(!install());noSelection();assert(otaAborts==1&&otaEnds==0&&state.errorCode==HashError);
  reset();offer();responses[offered.url].body.pop_back();responses[offered.url].contentLength=false;
  assert(!install());noSelection();assert(otaAborts==1&&otaEnds==0);
  reset();offer();responses[offered.url].body.push_back(0);responses[offered.url].contentLength=false;
  assert(!install());noSelection();assert(otaAborts==1&&otaEnds==0);
  reset();offer();responses[offered.url].body[12]=9;assert(!install());noSelection();assert(otaBegins==0);
  reset();offer();responses[offered.url].readFailAfter=1000;assert(!install());noSelection();assert(otaAborts==1);
  reset();offer();failBegin=true;assert(!install());noSelection();assert(otaWrites==0);
  reset();offer();failWriteAt=3;assert(!install());noSelection();assert(otaAborts==1&&otaEnds==0);
  reset();offer();failEnd=true;assert(!install());noSelection();assert(otaEnds==1&&otaAborts==0);
  reset();offer();assert(install());
  assert(record.stage==uint32_t(Stage::Armed)&&memcmp(record.targetHash,images[1].data()+images[1].size()-32,32)==0);
  assert(record.fromHash[0]==images[0][images[0].size()-32]);
  assert(fakeMillis>12000+50); // validation and readback deliberately yield
  reset();offer();responses[offered.url].body[5000]^=1;
  assert(!install());noSelection();assert(otaEnds==0); // download digest gate rejects mutation
  reset();offer();failVerifyTarget=true;assert(!install());noSelection();assert(otaEnds==1);
  reset();offer();failNvsWrite=1;assert(!install());noSelection();assert(otaBegins==0);
  reset();offer();failNvsWrite=2;assert(!install());noSelection();assert(otaEnds==1);
  reset();offer();badBootloader=true;assert(!install());noSelection();assert(otaBegins==0&&!state.supported);
  reset();offer();partitionTable[28]=1;assert(!install());noSelection();assert(otaBegins==0);
  reset();offer();installRelease("v99.1.3");noSelection();assert(otaBegins==0);
}
static void cancellationAndCommit(){
  for(const char* boundary:{"write","end"}){
    reset();offer();faultHook=[boundary](const char* stage){if(!strcmp(stage,boundary))assert(requestCancel());};
    assert(!install());noSelection();assert(state.phase==Phase::Cancelled);
    assert(!strcmp(boundary,"end")?otaAborts==0:otaAborts==1);
  }
  reset();offer();cancelRequested.store(true);assert(!install());noSelection();assert(otaBegins==0);
  reset();offer();persistenceHook=[](unsigned write){if(write==2)assert(!requestCancel());};
  assert(install());assert(bootSelections==1&&state.phase==Phase::Rebooting);
  reset();offer();failBoot=true;assert(!install());assert(bootSelections==1&&restarts==0&&bootIndex==0&&record.stage==uint32_t(Stage::Failed));
  reset();offer();failBoot=bootFailureAfterSelect=true;assert(!install());
  assert(bootSelections==1&&restarts==0&&bootIndex==1&&record.stage==uint32_t(Stage::Armed));
  reset();offer();badBootReadbackOnce=true;assert(!install());
  assert(bootSelections==1&&restarts==0&&partitionStates[0]==ESP_OTA_IMG_VALID&&record.stage==uint32_t(Stage::Armed));
  // A power cut may leave the selector on the old VALID app. It is recognized
  // as interrupted; no second boot-selection write consumes the VALID fallback.
  bootIndex=runningIndex=0;assert(layoutReady());reconcileBoot();
  assert(state.phase==Phase::Error&&record.stage==uint32_t(Stage::Interrupted)&&partitionStates[0]==ESP_OTA_IMG_VALID);
  reset();offer();badBootReadbackOnce=true;assert(!install());
  assert(bootSelections==1&&restarts==0&&partitionStates[0]==ESP_OTA_IMG_VALID&&record.stage==uint32_t(Stage::Armed));
  // Simulate the target build after a power cut. This host translation unit is
  // compiled once, so bind its expected target version to that compiled VERSION.
  Record nextBoot=record;policy::copy(nextBoot.targetVersion,panel::VERSION);assert(writeRecord(nextBoot));
  bootIndex=runningIndex=1;partitionStates[1]=ESP_OTA_IMG_PENDING_VERIFY;assert(layoutReady());
  heartbeatReady.store(false);assert(verifyRollbackLater());reconcileBoot();
  assert(pendingBoot&&pendingIdentityOK&&record.stage==uint32_t(Stage::Armed)&&partitionStates[0]==ESP_OTA_IMG_VALID);
  assert(bootDeadline&&bootDeadline->armed&&bootSelections==1&&restarts==0);
}
static void transport(){
  reset();offer();
  const std::string cdn="https://release-assets.githubusercontent.com/github-production-release-asset/123/abc?sig=local-test";
  responses[cdn]=responses[offered.url];::Response redirect;redirect.status=302;redirect.headers.push_back({"Location",cdn});responses[offered.url]=redirect;
  assert(install());assert(std::find(requestedUrls.begin(),requestedUrls.end(),cdn)!=requestedUrls.end());
  for(const char* location:{"http://release-assets.githubusercontent.com/github-production-release-asset/123/abc","https://evil.example/firmware","https://github.com/attacker/repo/firmware"}){
    reset();offer();::Response bad;bad.status=302;bad.headers.push_back({"Location",location});responses[offered.url]=bad;
    assert(!install());noSelection();assert(otaBegins==0);
  }
  reset();offer();::Response loop;loop.status=302;loop.headers.push_back({"Location",offered.url});responses[offered.url]=loop;
  assert(!install());noSelection();assert(requestedUrls.size()==6); // manifest plus five redirect attempts
  for(const auto& header:std::vector<std::pair<std::string,std::string>>{{"Content-Range","bytes 0-99999/100000"},{"Content-Encoding","gzip"},{"Content-Length","100000"}}){
    reset();offer();responses[offered.url].headers.push_back(header);assert(!install());noSelection();assert(otaBegins==0);
  }
  reset();offer();responses[offered.url].complete=false;assert(!install());noSelection();assert(otaAborts==1);
}
static void pendingFixture(){
  reset();runningIndex=bootIndex=1;partitionStates[1]=ESP_OTA_IMG_PENDING_VERIFY;assert(layoutReady());
  Record r;r.stage=uint32_t(Stage::Armed);r.fromAddress=policy::SLOT0;r.targetAddress=policy::SLOT1;r.size=100000;
  policy::copy(r.fromVersion,"v0.9.1");policy::copy(r.targetVersion,panel::VERSION);
  policy::copy(r.source,"0123456789abcdef0123456789abcdef01234567");
  memcpy(r.fromHash,images[0].data()+images[0].size()-32,32);
  memcpy(r.targetHash,images[1].data()+images[1].size()-32,32);
  assert(writeRecord(r));assert(verifyRollbackLater());assert(bootDeadline&&bootDeadline->armed&&bootDeadline->delay==60000000);
  heartbeatReady.store(false);reconcileBoot();assert(pendingBoot&&pendingIdentityOK);
}
static void goodTicks(){for(unsigned i=0;i<=10;i++){uiHeartbeat(true);checkBootHealth();fakeMillis+=1000;}}
static void bootHealth(){
  pendingFixture();checkBootHealth();assert(partitionStates[1]==ESP_OTA_IMG_PENDING_VERIFY);
  goodTicks();assert(!pendingBoot&&record.stage==uint32_t(Stage::Confirmed)&&partitionStates[1]==ESP_OTA_IMG_VALID&&!bootDeadline->armed);
  pendingFixture();try{bootDeadline->callback(nullptr);assert(false);}catch(const Restart&){}
  assert(restarts==1&&partitionStates[1]==ESP_OTA_IMG_PENDING_VERIFY);
  for(unsigned fault=0;fault<2;fault++){
    reset();runningIndex=bootIndex=1;partitionStates[1]=ESP_OTA_IMG_PENDING_VERIFY;
    failTimer=fault==0;failTimerStart=fault==1;try{verifyRollbackLater();assert(false);}catch(const Restart&){}assert(restarts==1);
  }
  pendingFixture();failMarkValid=true;goodTicks();assert(pendingBoot&&bootDeadline->armed&&partitionStates[1]==ESP_OTA_IMG_PENDING_VERIFY);
  pendingFixture();failNvsWrite=2;goodTicks();assert(!pendingBoot&&partitionStates[1]==ESP_OTA_IMG_VALID&&record.stage==uint32_t(Stage::Armed));
  failNvsWrite=0;reconcileBoot();assert(record.stage==uint32_t(Stage::Confirmed)&&state.phase==Phase::Success);
  pendingFixture();Record wrong=record;policy::copy(wrong.targetVersion,"v77.1.1");assert(writeRecord(wrong));reconcileBoot();
  assert(pendingBoot&&!pendingIdentityOK);goodTicks();assert(partitionStates[1]==ESP_OTA_IMG_PENDING_VERIFY);
  pendingFixture();Record corrupt=record;corrupt.targetHash[3]^=1;assert(writeRecord(corrupt));reconcileBoot();
  assert(pendingBoot&&!pendingIdentityOK);goodTicks();assert(partitionStates[1]==ESP_OTA_IMG_PENDING_VERIFY);
  pendingFixture();Record previous=record;policy::copy(previous.fromVersion,panel::VERSION);assert(writeRecord(previous));
  runningIndex=bootIndex=0;partitionStates[1]=ESP_OTA_IMG_ABORTED;assert(layoutReady());reconcileBoot();
  assert(state.phase==Phase::RolledBack&&record.stage==uint32_t(Stage::RolledBack));
  pendingFixture();previous=record;policy::copy(previous.fromVersion,panel::VERSION);assert(writeRecord(previous));
  runningIndex=bootIndex=0;partitionStates[1]=ESP_OTA_IMG_UNDEFINED;assert(layoutReady());reconcileBoot();
  assert(state.phase==Phase::Error&&record.stage==uint32_t(Stage::Interrupted));
}
static void rollbackAndUsb(){
  reset();Record r;r.stage=uint32_t(Stage::Confirmed);r.manual=1;r.fromAddress=policy::SLOT1;r.targetAddress=policy::SLOT0;
  policy::copy(r.fromVersion,"v0.9.1");policy::copy(r.targetVersion,panel::VERSION);
  memcpy(r.fromHash,images[1].data()+images[1].size()-32,32);
  memcpy(r.targetHash,images[0].data()+images[0].size()-32,32);
  partitionStates[1]=ESP_OTA_IMG_VALID;assert(writeRecord(r));assert(previousReady());wifi=false;
  try{rollback("v0.9.1");assert(false);}catch(const Restart&){}
  assert(bootIndex==1&&otaBegins==0&&requestedUrls.empty()&&record.manual==1);
  reset();partitionStates[0]=ESP_OTA_IMG_UNDEFINED;put32(otaData.data(),1);put32(otaData.data()+4096,0);
  put32(otaData.data()+28,esp_rom_crc32_le(UINT32_MAX,otaData.data(),4));
  assert(originalUsbState());assert(currentStateReady(true));assert(partitionStates[0]==ESP_OTA_IMG_VALID);
  partitionStates[0]=ESP_OTA_IMG_UNDEFINED;otaData[1000]=0;assert(!originalUsbState());assert(!currentStateReady(true));
  reset();makeTable();assert(policy::validPartitionTable(partitionTable.data(),partitionTable.size()));
  for(unsigned at:{2U,3U,4U,8U,12U,28U,32U+28U,4U*32U+4U,5U*32U+8U,192U,208U,223U,224U}){
    auto changed=partitionTable;changed[at]^=1;assert(!policy::validPartitionTable(changed.data(),changed.size()));
  }
}
int main(){checks();transfers();cancellationAndCommit();transport();bootHealth();rollbackAndUsb();
  assert(!strcmp(phaseName(Phase::RolledBack),"rolled_back"));
  std::cout<<"PASS: actual OTA adapter faults, inactive-slot-only writes, abort/no selection, commit boundary, recovery metadata, independent boot deadline, offline rollback and exact USB/layout gates\n";
}
