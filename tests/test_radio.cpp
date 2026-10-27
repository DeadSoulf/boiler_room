// Host logic tests only: mocks do NOT emulate RF, C6, DHCP, real NVS or task races.
#include "../src/panel_radio.cpp"
#include <cassert>
#include <iostream>
using namespace panel;using namespace panel_radio;
void reset(){
 state=RadioState{};state.ready=true;pendingCommand=false;urgent=Job::None;
 wipe(&pending,sizeof(pending));WiFi=Wifi{};prefs=Preferences{};nvsOK=true;screenPrefs=Preferences{};screenNvsOK=true;
 Serial.output.clear();wifiStarted=false;haveCredentials=false;fakeMillis=0;lastDisconnectReason=0;delayHook=nullptr;
}
Command connection(){Command c;c.job=Job::Connect;strcpy(c.ssid,"test");strcpy(c.password,"12345678");return c;}
int main(){
 taskResult=0;assert(!begin());assert(!stateMutex);taskResult=1;assert(begin());auto mutex=stateMutex;assert(begin()&&stateMutex==mutex);
 reset();Command c=connection();assert(submit(c));assert(!submit(c));assert(state.busy);
 Command received;assert(takeCommand(received));assert(!strcmp(received.password,c.password));
 for(const auto byte:pending.password){assert(byte==0);}assert(!pendingCommand);
 reset();assert(submit(c));Command forget;forget.job=Job::Forget;assert(submit(forget));assert(cancel());
 assert(takeCommand(received));assert(received.job==Job::Forget);assert(!pendingCommand);
 for(const auto byte:pending.password){assert(byte==0);}assert(!takeCommand(received));
 reset();memset(c.ssid,'x',sizeof(c.ssid));assert(!submit(c));c=connection();memset(c.password,'x',sizeof(c.password));assert(!submit(c));
 reset();c=connection();connectWifi(c,true);assert(fakeMillis==20000);assert(prefs.credentialWrites==0);assert(!haveCredentials);assert(strstr(state.wifiMessage,"20"));
 reset();delayHook=[](){arduino_event_info_t info{};info.wifi_sta_disconnected.reason=WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT;wifiEvent(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,info);};
 connectWifi(c,true);assert(strstr(state.wifiMessage,"4WAY_HANDSHAKE_TIMEOUT"));assert(prefs.credentialWrites==0);
 reset();delayHook=[](){cancel();};connectWifi(c,true);assert(fakeMillis==100);assert(!WiFi.connected);assert(prefs.credentialWrites==0);
 reset();WiFi.connectSuccess=true;connectWifi(c,true);assert(prefs.credentialWrites==1);assert(haveCredentials&&state.autoConnect&&WiFi.reconnect);
 updateConnection();assert(strstr(state.wifiMessage,"синхронизируется"));
 reset();WiFi.connectSuccess=true;prefs.writes=false;connectWifi(c,true);assert(!state.autoConnect&&!haveCredentials);updateConnection();assert(strstr(state.wifiMessage,"не сохранены"));
 reset();state.autoConnect=true;WiFi.connectSuccess=true;connectWifi(c,false);assert(prefs.credentialWrites==0&&state.autoConnect);
 reset();pinsReady=false;versions();assert(!strcmp(state.c6Version,"Не прочитана"));
 reset();prefs.credentialKey=true;haveCredentials=true;state.hasSaved=true;state.autoConnect=true;prefs.autoValue=true;prefs.writes=false;
 disconnectWifi(true);assert(state.hasSaved&&state.autoConnect&&prefs.credentialKey);assert(strstr(state.wifiMessage,"Не удалось"));
 prefs.writes=true;disconnectWifi(true);assert(!state.hasSaved&&!state.autoConnect&&!prefs.credentialKey);
 reset();for(uint16_t t:{0,30,60,120,300,600,1800})assert(validDisplayTimeout(t));for(uint16_t t:{1,29,31,65535})assert(!validDisplayTimeout(t));
 screenPrefs.displayValue=1800;assert(initializeDisplaySettings()==1800);assert(screenPrefs.lastNamespace=="boiler_screen"&&screenPrefs.lastKey=="off_sec");screenPrefs.opens=false;assert(initializeDisplaySettings()==0);screenPrefs.opens=true;assert(initializeDisplaySettings()==1800);
 screenPrefs.displayValue=65535;assert(loadDisplayTimeout()==0);screenPrefs.displayValue=65566;assert(loadDisplayTimeout()==0);screenPrefs.displayValue=1800;assert(loadDisplayTimeout()==1800);screenPrefs.displayValue=120;assert(loadDisplayTimeout()==120);screenNvsOK=false;assert(loadDisplayTimeout()==0);screenNvsOK=true;screenPrefs.displayValue=0;
 setDisplayTimeout(0);assert(screenPrefs.displayWrites==0);setDisplayTimeout(60);assert(screenPrefs.lastKey=="off_sec");assert(screenPrefs.displayWrites==1&&state.displayTimeoutSec==60&&screenPrefs.displayValue==60);
 setDisplayTimeout(60);assert(screenPrefs.displayWrites==1);setDisplayTimeout(31);assert(screenPrefs.displayWrites==1);
 screenPrefs.writes=false;setDisplayTimeout(120);assert(screenPrefs.displayWrites==2&&state.displayTimeoutSec==60);assert(strstr(state.displayMessage,"Ошибка NVS"));
 reset();Command screen;screen.job=Job::SetDisplayTimeout;screen.displayTimeoutSec=31;assert(!submit(screen));screen.displayTimeoutSec=300;assert(submit(screen));assert(cancel());
 assert(takeCommand(received)&&received.job==Job::Disconnect);assert(takeCommand(received)&&received.job==Job::SetDisplayTimeout&&received.displayTimeoutSec==300);
 reset();fakeMillis=0xffffff00U;WiFi.scanResult=-2;WiFi.scanElapsed=15000;state.apCount=4;
 scanWifi();assert(WiFi.scanCount==1&&state.apCount==0);assert(strstr(state.wifiMessage,"код -2, 15000 мс"));
 assert(Serial.output=="[BoilerPanel][WiFiScan] result=-2 elapsed_ms=15000\n");
 reset();WiFi.scanResult=-1;WiFi.scanElapsed=10;scanWifi();assert(strstr(state.wifiMessage,"код -1, 10 мс"));assert(WiFi.scanCount==1);
 reset();scanWifi();assert(state.apCount==0&&strstr(state.wifiMessage,"Найдено сетей: 0"));
 vSemaphoreDelete(stateMutex);stateMutex=nullptr;
 std::cout<<"PASS: radio admission, priority, secrets, timeout/reason, cancellation, persistence failures, display timeout commands, scan code/timing (host mocks only)\n";
}
