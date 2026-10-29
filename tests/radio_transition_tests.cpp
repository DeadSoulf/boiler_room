// Regression checks for observable API failures. No radio hardware or task races.
#include "../src/panel_radio.cpp"
#include <cassert>
#include <iostream>
using namespace panel;using namespace panel_radio;
void resetTransition(){
 state=RadioState{};state.ready=true;pendingCommand=false;urgent=Job::None;
 wipe(&pending,sizeof(pending));WiFi=Wifi{};prefs=Preferences{};nvsOK=true;
 Serial.output.clear();wifiStarted=false;haveCredentials=false;fakeMillis=0;
 lastDisconnectReason=0;delayHook=nullptr;recoveryPending=false;pinsReady=false;
 bleScanPending=false;bleScanComplete=false;
 *BLEDevice::getScan()=BLEScan{};
}
Command newConnection(){Command cmd;cmd.job=Job::Connect;strcpy(cmd.ssid,"requested-network");strcpy(cmd.password,"test-key-123");return cmd;}
void oldConnection(){WiFi.connected=true;WiFi.connectedSsid="previous-network";haveCredentials=true;prefs.credentialKey=true;prefs.autoValue=true;state.autoConnect=true;}
int main(){
 assert(begin());const Command cmd=newConnection();
 resetTransition();oldConnection();WiFi.disconnectSuccess=false;WiFi.beginFailure=true;
 connectWifi(cmd,true);
 assert(prefs.credentialWrites==0&&prefs.credentialKey&&prefs.autoValue&&haveCredentials);
 assert(WiFi.beginCount==0&&WiFi.connected&&state.connected);
 assert(!strcmp(state.ssid,"previous-network"));assert(strstr(state.wifiMessage,"Не удалось"));

 resetTransition();oldConnection();WiFi.disconnectChangesState=false;
 connectWifi(cmd,true);assert(prefs.credentialWrites==0&&WiFi.beginCount==0&&state.connected);

 resetTransition();WiFi.beginFailure=true;connectWifi(cmd,true);
 assert(fakeMillis==0&&prefs.credentialWrites==0);assert(strstr(state.wifiMessage,"запустить"));

 resetTransition();oldConnection();WiFi.connectSuccess=true;WiFi.beginSsidOverride="unrelated-network";
 connectWifi(cmd,true);assert(prefs.credentialWrites==0&&haveCredentials&&prefs.autoValue);
 assert(strstr(state.wifiMessage,"другая сеть"));

 resetTransition();oldConnection();WiFi.connectSuccess=true;connectWifi(cmd,true);updateConnection();
 assert(prefs.credentialWrites==1&&state.connected&&haveCredentials&&state.autoConnect);
 assert(!strcmp(state.ssid,cmd.ssid));assert(strstr(state.wifiMessage,"Подключено"));

 resetTransition();WiFi.connectSuccess=true;WiFi.hasIp=false;
 delayHook=[](){assert(prefs.credentialWrites==0);if(fakeMillis>=300)WiFi.hasIp=true;};
 connectWifi(cmd,true);assert(fakeMillis==300&&prefs.credentialWrites==1);delayHook=nullptr;

 resetTransition();oldConnection();wifiStarted=true;WiFi.disconnectSuccess=false;
 disconnectWifi(false);assert(state.connected&&!strcmp(state.ssid,"previous-network"));
 assert(haveCredentials&&!state.autoConnect&&!prefs.autoValue);
 assert(strstr(state.wifiMessage,"Не удалось отключить Wi-Fi"));

 resetTransition();oldConnection();wifiStarted=true;WiFi.disconnectSuccess=false;
 disconnectWifi(true);assert(state.connected&&!haveCredentials&&!prefs.credentialKey);
 assert(strstr(state.wifiMessage,"Не удалось отключить Wi-Fi"));assert(strstr(state.wifiMessage,"сеть удалена"));

 resetTransition();oldConnection();wifiStarted=true;disconnectWifi(false);
 assert(!state.connected&&!state.ssid[0]&&!state.ip[0]&&haveCredentials&&!state.autoConnect);
 assert(strstr(state.wifiMessage,"Wi-Fi отключён"));

 resetTransition();auto* scanner=BLEDevice::getScan();scanner->startSuccess=false;state.bleCount=4;
 scanBle();assert(scanner->startCalls==1&&state.bleCount==0);assert(strstr(state.bleMessage,"запустить"));
 assert(!strstr(state.bleMessage,"завершён"));

 resetTransition();scanner=BLEDevice::getScan();scanner->scanning=true;scanner->autoComplete=false;
 scanBle();assert(scanner->startCalls==0&&scanner->stopCalls==0&&scanner->clearCalls==0);
 assert(strstr(state.bleMessage,"уже выполняется"));

 resetTransition();scanBle();scanner=BLEDevice::getScan();
 assert(scanner->completionCalls==1&&state.bleCount==0);assert(strstr(state.bleMessage,"Найдено: 0"));

 resetTransition();scanner=BLEDevice::getScan();
 for(int i=0;i<int(MAX_BLE)+3;i++){BLEAdvertisedDevice d;d.name="Device "+std::to_string(i);d.rssi=-90+i;scanner->nextResults.devices.push_back(d);}
 scanBle();assert(state.bleCount==MAX_BLE);assert(state.ble[0].rssi==-90+int(MAX_BLE)+2);
 for(unsigned i=1;i<MAX_BLE;i++)assert(state.ble[i-1].rssi>=state.ble[i].rssi);
 assert(scanner->results.getCount()==0&&strstr(state.bleMessage,"Поиск завершён"));

 resetTransition();scanner=BLEDevice::getScan();scanner->notifyCompletion=false;
 scanBle();assert(state.bleCount==0);assert(strstr(state.bleMessage,"подтверждения завершения"));
 assert(scanner->clearCalls==1&&bleScanPending);
 scanBle();assert(scanner->startCalls==1&&scanner->clearCalls==1);
 assert(strstr(state.bleMessage,"предыдущего поиска"));
 scanner->notifyCompletion=true;scanner->callback(scanner->results);
 scanBle();assert(scanner->startCalls==2&&!bleScanPending);
 assert(strstr(state.bleMessage,"Поиск завершён"));

 resetTransition();scanner=BLEDevice::getScan();scanner->autoComplete=false;
 scanBle();assert(scanner->stopCalls==1&&!scanner->scanning&&state.bleCount==0);
 assert(strstr(state.bleMessage,"Таймаут"));

 resetTransition();scanner=BLEDevice::getScan();scanner->autoComplete=false;delayHook=[](){cancel();};
 scanBle();delayHook=nullptr;assert(scanner->stopCalls==1&&!scanner->scanning&&urgent==Job::Disconnect);
 assert(strstr(state.bleMessage,"отменён"));

 resetTransition();scanner=BLEDevice::getScan();scanner->autoComplete=false;scanner->stopSuccess=false;
 scanBle();assert(scanner->scanning&&scanner->clearCalls==1&&state.bleCount==0);
 assert(strstr(state.bleMessage,"не удалось остановить"));

 resetTransition();scanner=BLEDevice::getScan();scanner->resultsAvailable=false;scanBle();
 assert(state.bleCount==0&&strstr(state.bleMessage,"результат недоступен"));
 vSemaphoreDelete(stateMutex);stateMutex=nullptr;
 std::cout<<"PASS: Wi-Fi transition/identity/persistence failures and BLE launch/completion/empty/results/cancel/timeout failures (host mocks only)\n";
}
