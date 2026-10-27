// Host logic tests only: mocks do NOT emulate RF, C6, DHCP, real NVS or task races.
#include "../src/panel_radio.cpp"
#include <cassert>
#include <iostream>
using namespace panel;using namespace panel_radio;
void reset(){
 state=RadioState{};state.ready=true;pendingCommand=false;urgent=Job::None;
 wipe(&pending,sizeof(pending));WiFi=Wifi{};prefs=Preferences{};nvsOK=true;
 wifiStarted=false;haveCredentials=false;fakeMillis=0;lastDisconnectReason=0;delayHook=nullptr;
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
 reset();for(uint16_t t:{0,30,60,120,300,600})assert(validDisplayTimeout(t));for(uint16_t t:{1,29,31,65535})assert(!validDisplayTimeout(t));
 prefs.displayValue=65535;assert(loadDisplayTimeout()==0);prefs.displayValue=120;assert(loadDisplayTimeout()==120);nvsOK=false;assert(loadDisplayTimeout()==0);nvsOK=true;prefs.displayValue=0;
 setDisplayTimeout(0);assert(prefs.displayWrites==0);setDisplayTimeout(60);assert(prefs.displayWrites==1&&state.displayTimeoutSec==60&&prefs.displayValue==60);
 setDisplayTimeout(60);assert(prefs.displayWrites==1);setDisplayTimeout(31);assert(prefs.displayWrites==1);
 prefs.writes=false;setDisplayTimeout(120);assert(prefs.displayWrites==2&&state.displayTimeoutSec==60);assert(strstr(state.displayMessage,"Ошибка NVS"));
 reset();Command screen;screen.job=Job::SetDisplayTimeout;screen.displayTimeoutSec=31;assert(!submit(screen));screen.displayTimeoutSec=300;assert(submit(screen));assert(cancel());
 assert(takeCommand(received)&&received.job==Job::Disconnect);assert(takeCommand(received)&&received.job==Job::SetDisplayTimeout&&received.displayTimeoutSec==300);
 vSemaphoreDelete(stateMutex);stateMutex=nullptr;
 std::cout<<"PASS: radio admission, priority, secrets, timeout/reason, cancellation, persistence failures, display timeout commands (host mocks only)\n";
}
