// Actual radio worker/supervisor with fake monotonic time and API/NVS failures.
// These are executable adapter tests, not RF/C6/DHCP/hardware acceptance.
#include "../src/panel_radio.cpp"
#include <cassert>
#include <iostream>
using namespace panel;using namespace panel_radio;
void resetReconnectTest(){
 state=RadioState{};state.ready=true;pendingCommand=false;urgent=Job::None;
 wipe(&pending,sizeof(pending));WiFi=Wifi{};prefs=Preferences{};screenPrefs=Preferences{};
 nvsOK=true;Serial.output.clear();wifiStarted=false;haveCredentials=false;fakeMillis=0;
 disconnectReason.store(0);observeConnect.store(false);associated.store(false);
 wifiDriver=WifiDriver{};eventInstalled=false;driverStoppedForRecovery=false;
 staStartEvents.store(0);staStopEvents.store(0);delayHook=nullptr;recoveryPending=false;pinsReady=false;
 lastGood=SavedNetwork{};resetReconnect(false);
}
Command network(const char* ssid="saved-network",const char* password="synthetic-key"){
 Command cmd;cmd.job=Job::Connect;snprintf(cmd.ssid,sizeof(cmd.ssid),"%s",ssid);
 snprintf(cmd.password,sizeof(cmd.password),"%s",password);return cmd;
}
void persist(const Command& cmd=network()){
 Credentials credentials;snprintf(credentials.ssid,sizeof(credentials.ssid),"%s",cmd.ssid);
 snprintf(credentials.password,sizeof(credentials.password),"%s",cmd.password);credentials.open=cmd.open;
 assert(prefs.putBytes("credentials",&credentials,sizeof(credentials))==sizeof(credentials));
 prefs.autoValue=true;haveCredentials=true;state.hasSaved=true;state.autoConnect=true;state.savedCount=1;
 snprintf(state.savedNetworks[0].ssid,sizeof(state.savedNetworks[0].ssid),"%s",cmd.ssid);state.savedNetworks[0].open=cmd.open;
 wipe(&credentials,sizeof(credentials));prefs.credentialWrites=0;
}
void tick(){updateConnection();superviseWifi();}
void due(){fakeMillis=reconnectSince+reconnectDelay;tick();}
void urgentDisconnect(bool forget=false){
 Command cmd;cmd.job=forget?Job::Forget:Job::Disconnect;assert(submit(cmd));
 tick();assert(takeCommand(cmd));disconnectWifi(forget);jobState(Job::None,false);
}
struct StopWorker{};
int main(){
 assert(begin());
 // Real boot worker: AP appears after 90 s, after multiple failed 20 s attempts.
 resetReconnectTest();persist();
 delayHook=[](){WiFi.connectSuccess=fakeMillis>=90000U;if(state.connected&&fakeMillis>90000U)throw StopWorker{};};
 try{worker(nullptr);}catch(const StopWorker&){}delayHook=nullptr;
 assert(state.connected&&WiFi.beginCount==4&&fakeMillis>=165000U&&fakeMillis<166000U);
 assert(prefs.credentialReads==5&&prefs.credentialWrites==0&&!WiFi.reconnect);
 assert(Serial.output.find("synthetic-key")==std::string::npos);
 RadioState snapshotCopy;assert(snapshot(snapshotCopy));
 assert(std::string(reinterpret_cast<char*>(&snapshotCopy),sizeof(snapshotCopy)).find("synthetic-key")==std::string::npos);
 const auto begins=WiFi.beginCount;fakeMillis+=3600000U;tick();assert(WiFi.beginCount==begins);
 // Later outage resets to 15 s, not the previous high backoff. No sleep in pause.
 WiFi.connected=false;tick();assert(reconnectDelay==15000U&&!state.busy);
 const auto now=fakeMillis;tick();assert(fakeMillis==now&&WiFi.beginCount==begins);
 fakeMillis+=14999;tick();assert(WiFi.beginCount==begins);++fakeMillis;tick();assert(state.connected&&WiFi.beginCount==begins+1);

 // Authentication/no-AP failure: grow, cap at five minutes, use rollover-safe time.
 resetReconnectTest();persist();resetReconnect(true,true);tick();assert(reconnectDelay==15000U);
 for(uint32_t expected:{30000U,60000U,120000U,240000U,300000U,300000U}){
   due();assert(reconnectDelay==expected&&!state.busy&&prefs.credentialWrites==0);
 }
 fakeMillis=0xfffffff0U;reconnectSince=fakeMillis;reconnectDelay=15000;
 fakeMillis+=14999;const auto previous=WiFi.beginCount;tick();assert(WiFi.beginCount==previous);
 ++fakeMillis;tick();assert(WiFi.beginCount==previous+1);

 // Queued commands and cancellation win even at the exact retry boundary.
 resetReconnectTest();persist();resetReconnect(true,true);
 Command scan;scan.job=Job::WifiScan;assert(submit(scan));tick();assert(!WiFi.beginCount);
 Command received;assert(takeCommand(received)&&received.job==Job::WifiScan);
 scanWifi();jobState(Job::None,false);WiFi.connectSuccess=true;tick();assert(state.connected);
 WiFi.connected=false;tick();urgentDisconnect();fakeMillis+=3600000;tick();
 assert(!reconnectEnabled&&!prefs.autoValue&&WiFi.beginCount==1);
 resetReconnectTest();persist();resetReconnect(true,true);
 delayHook=[](){if(fakeMillis>=400U)assert(cancel());};tick();delayHook=nullptr;
 assert(fakeMillis==400U&&urgent==Job::Disconnect&&WiFi.beginCount==1);
 assert(takeCommand(received));disconnectWifi(false);jobState(Job::None,false);fakeMillis+=3600000;tick();
 assert(!reconnectEnabled&&WiFi.beginCount==1&&!state.connected);

 // NVS failures cannot undo explicit offline intent; Forget never recreates data.
 resetReconnectTest();persist();resetReconnect(true,true);prefs.writes=false;
 urgentDisconnect();assert(state.autoConnect&&!reconnectEnabled);fakeMillis+=3600000;tick();assert(!WiFi.beginCount);
 resetReconnectTest();persist();resetReconnect(true,true);urgentDisconnect(true);
 assert(!prefs.credentialKey&&!haveCredentials&&!reconnectEnabled);fakeMillis+=3600000;tick();assert(!WiFi.beginCount);
 resetReconnectTest();persist();resetReconnect(true,true);prefs.writes=false;urgentDisconnect(true);
 assert(prefs.credentialKey&&state.autoConnect&&!reconnectEnabled);fakeMillis+=3600000;tick();assert(!WiFi.beginCount);

 // Each attempt reloads NVS, no stale boot/password copy and no retry writes.
 resetReconnectTest();persist();resetReconnect(true,true);tick();
 persist(network("changed-network","changed-key"));WiFi.connectSuccess=true;due();
 assert(state.connected&&WiFi.lastBeginSsid=="changed-network"&&WiFi.lastBeginPassword=="changed-key");
 assert(prefs.credentialWrites==0);
 resetReconnectTest();persist();resetReconnect(true,true);prefs.reads=false;tick();
 assert(!WiFi.beginCount&&reconnectDelay==15000U);prefs.reads=true;WiFi.connectSuccess=true;due();assert(state.connected);
 resetReconnectTest();persist();prefs.credentialBytes[0]^=0xff;resetReconnect(true,true);tick();
 assert(!WiFi.beginCount&&reconnectDelay==15000U);
 Credentials invalid;assert(!loadCredentials(invalid));for(auto b:std::string(reinterpret_cast<char*>(&invalid),sizeof(invalid)))assert(b==0);
 resetReconnectTest();persist();prefs.credentialBytes.pop_back();resetReconnect(true,true);tick();assert(!WiFi.beginCount);

 // Malformed fields cannot reach password validation or the Wi-Fi driver.
 for(unsigned bad=0;bad<4;bad++){
   resetReconnectTest();Credentials corrupt;strcpy(corrupt.ssid,"saved-network");strcpy(corrupt.password,"synthetic-key");
   if(bad==0)memset(corrupt.ssid,'x',sizeof(corrupt.ssid));
   if(bad==1)memset(corrupt.password,'x',sizeof(corrupt.password));
   if(bad==2)corrupt.open=2;
   if(bad==3)strcpy(corrupt.password,"short");
   prefs.putBytes("credentials",&corrupt,sizeof(corrupt));resetReconnect(true,true);tick();assert(!WiFi.beginCount);
 }

 // A failed manual choice suspends automatic attempts for this session. A
 // verified manual choice becomes preferred, without deleting the older profile.
 resetReconnectTest();persist();resetReconnect(true,true);const auto replacement=network("new-network","new-password");
 assert(!connectWifi(replacement,true));assert(!reconnectEnabled&&prefs.credentialWrites==0);
 fakeMillis+=3600000;const auto failed=WiFi.beginCount;tick();assert(WiFi.beginCount==failed);
 WiFi.connectSuccess=true;assert(connectWifi(replacement,true));assert(reconnectEnabled);
 WiFi.connected=false;tick();due();assert(state.connected&&WiFi.lastBeginSsid=="new-network");
 resetReconnectTest();persist();resetReconnect(true,true);WiFi.connectSuccess=true;prefs.writes=false;
 assert(connectWifi(replacement,true));assert(!reconnectEnabled);WiFi.connected=false;fakeMillis+=3600000;tick();assert(WiFi.beginCount==1);

 // Open networks remain explicit; corrupt or missing credentials never begin.
 resetReconnectTest();auto open=network("open-network","");open.open=true;persist(open);
 resetReconnect(true,true);WiFi.connectSuccess=true;tick();assert(state.connected&&WiFi.lastBeginOpen);
 resetReconnectTest();resetReconnect(true,true);tick();assert(!WiFi.beginCount);

 // Startup/mode/recovery failures are backed off, not hot-looped or fatal.
 resetReconnectTest();persist();resetReconnect(true,true);WiFi.modeSuccess=false;tick();
 assert(!WiFi.beginCount&&reconnectDelay==15000U);WiFi.modeSuccess=true;WiFi.connectSuccess=true;due();assert(state.connected);
 resetReconnectTest();persist();resetReconnect(true,true);wifiDriver.modeResult=ESP_FAIL;tick();
 assert(!WiFi.beginCount&&reconnectDelay==15000U);wifiDriver.modeResult=ESP_OK;WiFi.connectSuccess=true;due();assert(state.connected);
 resetReconnectTest();persist();resetReconnect(true,true);wifiStarted=true;eventInstalled=true;WiFi.onEvent(wifiEvent);
 driverStoppedForRecovery=true;wifiDriver.startResult=ESP_FAIL;tick();assert(driverStoppedForRecovery&&!WiFi.beginCount);
 wifiDriver.startResult=ESP_OK;WiFi.connectSuccess=true;due();assert(state.connected&&!driverStoppedForRecovery);

 // Scanning errors/recovery failure do not silently turn future retries off.
 resetReconnectTest();persist();resetReconnect(true,true);wifiDriver.scanResults={ESP_ERR_WIFI_TIMEOUT};
 scanWifi();WiFi.connectSuccess=true;tick();assert(state.connected&&WiFi.beginCount==1);
 resetReconnectTest();persist();resetReconnect(true,true);wifiDriver.scanResults={ESP_ERR_WIFI_STATE};
 wifiDriver.startResult=ESP_FAIL;scanWifi();assert(driverStoppedForRecovery&&reconnectEnabled);
 tick();assert(!WiFi.beginCount&&reconnectDelay==15000U);wifiDriver.startResult=ESP_OK;WiFi.connectSuccess=true;
 due();assert(state.connected&&!driverStoppedForRecovery);

 // Slow DHCP is allowed to finish after 20 s without a second disconnect/begin.
 resetReconnectTest();persist();resetReconnect(true,true);WiFi.connectSuccess=true;WiFi.hasIp=false;
 delayHook=[](){if(fakeMillis>=25200U)WiFi.hasIp=true;};tick();delayHook=nullptr;
 assert(state.connected&&fakeMillis==25200U&&WiFi.beginCount==1&&WiFi.disconnectCount==1);
 resetReconnectTest();persist();resetReconnect(true,true);WiFi.connectSuccess=true;WiFi.hasIp=false;tick();
 assert(!state.connected&&fakeMillis==60200U&&reconnectDelay==15000U);

 // Arduino may hold WL_CONNECTED false until DHCP, while the IDF is associated.
 resetReconnectTest();persist();resetReconnect(true,true);wifiDriver.associatedSsid="saved-network";
 delayHook=[](){if(fakeMillis>=25200U){WiFi.connected=true;WiFi.connectedSsid="saved-network";}};
 tick();delayHook=nullptr;assert(state.connected&&fakeMillis==25200U&&WiFi.beginCount==1);
 resetReconnectTest();persist();resetReconnect(true,true);wifiDriver.associatedSsid="another-network";
 tick();assert(fakeMillis==20200U&&!state.connected&&reconnectDelay==15000U);

 // Cancellation also works inside the DHCP grace period, even if NVS fails.
 resetReconnectTest();persist();resetReconnect(true,true);wifiDriver.associatedSsid="saved-network";
 delayHook=[](){if(fakeMillis>=25200U)assert(cancel());};tick();delayHook=nullptr;
 assert(fakeMillis==25200U&&urgent==Job::Disconnect);assert(takeCommand(received));prefs.writes=false;
 disconnectWifi(false);jobState(Job::None,false);
 for(unsigned i=0;i<10;i++){fakeMillis+=300000U;tick();}assert(!reconnectEnabled&&WiFi.beginCount==1);

 // Boot with disabled autoconnect or unreadable NVS does not start a radio.
 for(bool readable:{true,false}){
   resetReconnectTest();persist();prefs.autoValue=false;prefs.opens=readable;
   delayHook=[](){if(fakeMillis>=1000U)throw StopWorker{};};
   try{worker(nullptr);}catch(const StopWorker&){}delayHook=nullptr;
   assert(!WiFi.beginCount&&!reconnectEnabled);
 }
 vSemaphoreDelete(stateMutex);stateMutex=nullptr;
 std::cout<<"PASS: reconnect real worker boot, slow router/DHCP, capped backoff/wrap, commands/cancel/forget, NVS reload/faults, credential changes and radio failures (host fakes only)\n";
}
