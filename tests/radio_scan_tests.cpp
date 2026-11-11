// Actual radio adapter with deterministic IDF fakes. No RF, C6, DHCP or NVS hardware.
#include "../src/panel_radio.cpp"
#include <cassert>
#include <iostream>
using namespace panel;using namespace panel_radio;
void resetScan(){
 state=RadioState{};state.ready=true;pendingCommand=false;urgent=Job::None;
 WiFi=Wifi{};wifiDriver=WifiDriver{};prefs=Preferences{};nvsOK=true;
 Serial.output.clear();wifiStarted=false;eventInstalled=false;pinsReady=false;
 driverStoppedForRecovery=false;recoveryPending=false;haveCredentials=true;
 Credentials c;strcpy(c.ssid,"test");strcpy(c.password,"scan-fixture-key");
 assert(prefs.putBytes("credentials",&c,sizeof(c))==sizeof(c));wipe(&c,sizeof(c));prefs.credentialWrites=0;
 prefs.autoValue=true;state.autoConnect=true;
 fakeMillis=0;delayHook=nullptr;staStartEvents.store(0);staStopEvents.store(0);
 disconnectReason.store(0);observeConnect.store(false);associated.store(false);
}
wifi_ap_record_t record(const char* name,int rssi,int auth=WIFI_AUTH_WPA2_PSK){
 wifi_ap_record_t ap;snprintf(reinterpret_cast<char*>(ap.ssid),sizeof(ap.ssid),"%s",name);
 ap.rssi=int8_t(rssi);ap.authmode=auth;return ap;
}
void unchangedCredentials(){assert(haveCredentials&&prefs.credentialKey&&prefs.autoValue&&prefs.credentialWrites==0);}
int main(){
 assert(begin());
 resetScan();wifiDriver.modeResult=ESP_FAIL;scanWifi();assert(!wifiDriver.scanCalls&&strstr(state.wifiMessage,"ESP_FAIL"));
 resetScan();wifiDriver.mode=WIFI_MODE_NULL;scanWifi();assert(!wifiDriver.scanCalls&&strstr(state.wifiMessage,"ESP_ERR_WIFI_MODE"));
 resetScan();wifiDriver.linkResult=ESP_FAIL;scanWifi();assert(!wifiDriver.scanCalls&&!wifiDriver.stopCalls);

 // IDF scan error is preserved; only WIFI_STATE while idle permits one retry.
 resetScan();wifiDriver.scanResults={ESP_ERR_WIFI_STATE};scanWifi();
 assert(wifiDriver.scanCalls==2&&wifiDriver.stopCalls==1&&wifiDriver.startCalls==1);
 assert(wifiDriver.storageCalls==1&&wifiDriver.configCalls==1&&wifiDriver.runtimeConfigCleared);
 assert(!driverStoppedForRecovery&&strstr(state.wifiMessage,"Найдено: 0"));unchangedCredentials();
 resetScan();wifiDriver.scanResults={ESP_ERR_WIFI_STATE,ESP_ERR_WIFI_STATE};scanWifi();
 assert(wifiDriver.scanCalls==2&&wifiDriver.stopCalls==1&&strstr(state.wifiMessage,"ESP_ERR_WIFI_STATE"));unchangedCredentials();

 // An associated station, even without DHCP, is never stopped to repair a scan.
 resetScan();assert(startWifi());WiFi.connected=true;WiFi.hasIp=false;WiFi.reconnect=true;
 wifiDriver.scanResults={ESP_ERR_WIFI_STATE};scanWifi();
 assert(wifiDriver.scanCalls==1&&!wifiDriver.stopCalls&&!wifiDriver.disconnectCalls&&WiFi.connected&&WiFi.reconnect);
 resetScan();wifiDriver.linkResult=ESP_OK;wifiDriver.scanResults={ESP_ERR_WIFI_STATE};scanWifi();
 assert(wifiDriver.scanCalls==1&&!wifiDriver.stopCalls&&!wifiDriver.disconnectCalls);

 resetScan();wifiDriver.scanResults={ESP_ERR_WIFI_STATE};wifiDriver.stopResult=ESP_FAIL;scanWifi();
 assert(wifiDriver.scanCalls==1&&wifiDriver.stopCalls==1&&!wifiDriver.startCalls&&!wifiDriver.configCalls);
 resetScan();wifiDriver.scanResults={ESP_ERR_WIFI_STATE};wifiDriver.stopEvent=false;scanWifi();
 assert(wifiDriver.scanCalls==1&&wifiDriver.startCalls==1&&!wifiDriver.configCalls);
 assert(strstr(state.wifiMessage,"STA_STOP")&&strstr(state.wifiMessage,"ESP_ERR_TIMEOUT"));unchangedCredentials();
 resetScan();wifiDriver.scanResults={ESP_ERR_WIFI_STATE};wifiDriver.startEvent=false;scanWifi();
 assert(wifiDriver.scanCalls==1&&wifiDriver.startCalls==1&&strstr(state.wifiMessage,"STA_START"));
 resetScan();wifiDriver.scanResults={ESP_ERR_WIFI_STATE};wifiDriver.startResult=ESP_FAIL;scanWifi();
 assert(driverStoppedForRecovery&&wifiDriver.scanCalls==1&&wifiDriver.startCalls==1);
 wifiDriver.startResult=ESP_OK;scanWifi();assert(!driverStoppedForRecovery&&wifiDriver.startCalls==2&&wifiDriver.scanCalls==2);
 resetScan();wifiDriver.scanResults={ESP_ERR_WIFI_STATE};wifiDriver.storageResult=ESP_FAIL;scanWifi();
 assert(wifiDriver.scanCalls==1&&wifiDriver.startCalls==1&&!wifiDriver.configCalls);unchangedCredentials();
 resetScan();wifiDriver.scanResults={ESP_ERR_WIFI_STATE};wifiDriver.configResult=ESP_FAIL;scanWifi();
 assert(wifiDriver.scanCalls==1&&wifiDriver.startCalls==1);unchangedCredentials();

 resetScan();assert(startWifi());WiFi.connected=true;WiFi.reconnect=true;wifiDriver.countResult=ESP_FAIL;scanWifi();
 assert(!state.apCount&&WiFi.reconnect&&wifiDriver.clearCalls==1);
 resetScan();assert(startWifi());WiFi.connected=true;WiFi.reconnect=true;wifiDriver.records={record("test",-20)};wifiDriver.recordsResult=ESP_FAIL;scanWifi();
 assert(!state.apCount&&WiFi.reconnect&&wifiDriver.clearCalls==1);
 resetScan();wifiDriver.records={record("duplicate",-80),record("",-10),record("duplicate",-25),record("open",-60,WIFI_AUTH_OPEN)};
 scanWifi();assert(state.apCount==2&&!strcmp(state.aps[0].ssid,"duplicate")&&state.aps[0].rssi==-25);
 assert(!strcmp(state.aps[1].ssid,"open")&&state.aps[1].supported);unchangedCredentials();
 vSemaphoreDelete(stateMutex);stateMutex=nullptr;
 std::cout<<"PASS: actual IDF scan, bounded recovery, STA acknowledgements, association preservation, errors and result handling (host fakes only)\n";
}
