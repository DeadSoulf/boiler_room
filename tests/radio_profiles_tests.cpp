// Actual worker, public command/snapshot, independent per-key NVS and ESP fakes.
// Each mode is launched as a new process. Only the fixture's persistent bytes
// and autoconnect flag survive; no firmware selection/credential RAM survives.
#include "../src/panel_radio.cpp"
#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <vector>
using namespace panel;
using namespace panel_radio;
namespace {
const char HOME_NET[]="fixture-home", OFFICE[]="fixture-office";
const char HOME_KEY[]="home-fixture-key", OFFICE_KEY[]="office-fixture-key";
const char WRONG_KEY[]="rejected-fixture-key", BULK_KEY[]="bulk-fixture-key";
constexpr char BOOK[]="wifi_profiles";
std::map<std::string,std::string> available;
bool allow(const char* ssid,const char* password) {
 const auto it=available.find(ssid);return it!=available.end()&&it->second==(password?password:"");
}
void driver(){WiFi.acceptBegin=allow;available={{HOME_NET,HOME_KEY},{OFFICE,OFFICE_KEY}};}
// Exact ESP32 little-endian schema of the pre-v0.10.7 104-byte Credentials
// struct, including trailing padding. Do not build it with the new serializer.
void legacy(){
 std::vector<unsigned char> bytes(104,0);
 bytes[0]=0x33;bytes[1]=0x30;bytes[2]=0x50;bytes[3]=0x42;
 std::memcpy(bytes.data()+4,HOME_NET,sizeof(HOME_NET));std::memcpy(bytes.data()+37,HOME_KEY,sizeof(HOME_KEY));
 assert(prefs.putBytes("credentials",bytes.data(),bytes.size())==bytes.size());
}
void transfer(const char* file,bool writing){
 std::fstream io(file,writing?std::ios::out|std::ios::binary|std::ios::trunc:std::ios::in|std::ios::binary);assert(io);
 uint8_t active=prefs.autoValue;
 if(writing)io.write(reinterpret_cast<char*>(&active),1);else{io.read(reinterpret_cast<char*>(&active),1);prefs.autoValue=active!=0;}
 for(const char* key:{"credentials",BOOK}){
  unsigned size=writing?unsigned(prefs.getBytesLength(key)):0;
  if(writing)io.write(reinterpret_cast<char*>(&size),sizeof(size));else io.read(reinterpret_cast<char*>(&size),sizeof(size));
  assert(io&&size<=4096);
  if(size){
   std::vector<unsigned char> data(size);
   if(writing){assert(prefs.getBytes(key,data.data(),size)==size);io.write(reinterpret_cast<char*>(data.data()),size);}
   else{io.read(reinterpret_cast<char*>(data.data()),size);assert(io);assert(prefs.putBytes(key,data.data(),size)==size);}
  }
 }
 assert(io.good());io.close();assert(io.good());prefs.credentialWrites=0;
}
wifi_ap_record_t access(const char* name,int signal,int auth=WIFI_AUTH_WPA2_PSK){
 wifi_ap_record_t ap{};std::snprintf(reinterpret_cast<char*>(ap.ssid),sizeof(ap.ssid),"%s",name);
 ap.authmode=auth;ap.rssi=int8_t(signal);return ap;
}
Command command(Job job,const char* ssid="",const char* key="",bool open=false,bool saved=false){
 Command cmd{};cmd.job=job;cmd.open=open;cmd.useSaved=saved;
 std::snprintf(cmd.ssid,sizeof(cmd.ssid),"%s",ssid);std::snprintf(cmd.password,sizeof(cmd.password),"%s",key);return cmd;
}
void click(const char* ssid,bool open=false){
 assert(submit(command(Job::Connect,ssid,"",open,true)));
 // The public action/mailbox is just an identifier, not a round trip of the key.
 for(auto c:pending.password)assert(c==0);
}
RadioState shot(){RadioState out{};assert(snapshot(out));return out;}
bool saved(const RadioState& s,const char* id,bool open=false){
 for(unsigned i=0;i<s.savedCount;i++)if(s.savedNetworks[i].open==open&&!std::strcmp(s.savedNetworks[i].ssid,id))return true;
 return false;
}
void privateKeys(){
 auto s=shot();const std::string dump(reinterpret_cast<const char*>(&s),sizeof(s));
 for(const char* secret:{HOME_KEY,OFFICE_KEY,WRONG_KEY,BULK_KEY}){
  assert(dump.find(secret)==std::string::npos);assert(Serial.output.find(secret)==std::string::npos);
 }
}
struct EndWorker{};
std::function<void()> onIdle;
void idle(){
 assert(fakeMillis<1800000U); // All scenarios finish with bounded fake time.
 if(state.ready&&!state.busy&&onIdle)onIdle();
}
void run(const std::function<void()>& step){
 onIdle=step;delayHook=idle;
 try{worker(nullptr);assert(false);}catch(const EndWorker&){}
 delayHook=nullptr;onIdle=nullptr;privateKeys();
}
void expectBook(const std::vector<unsigned char>& expected){const auto* b=prefs.blob(BOOK);assert(b&&*b==expected);}
void setup(const char* file){
 legacy();prefs.autoValue=true;prefs.credentialWrites=0;
 wifiDriver.records={access(HOME_NET,-75),access(HOME_NET,-28),access(HOME_NET,-15,WIFI_AUTH_OPEN),
                     access(HOME_NET,-10,90),access(OFFICE,-45)};
 unsigned n=0,initialBegins=0;std::vector<unsigned char> before;
 run([&]{const auto s=shot();switch(n++){
  case 0:assert(s.connected&&s.autoConnect&&s.savedCount==1&&saved(s,HOME_NET));
   assert(WiFi.lastBeginSsid==HOME_NET&&WiFi.lastBeginPassword==HOME_KEY&&prefs.credentialWrites==0);
   assert(submit(command(Job::WifiScan)));break;
  case 1:{
   unsigned known=0,open=0,unsupported=0,unknown=0;
   for(unsigned i=0;i<s.apCount;i++){const auto& ap=s.aps[i];
    if(!std::strcmp(ap.ssid,HOME_NET)){
     if(ap.auth==WIFI_AUTH_OPEN){++open;assert(ap.supported&&!ap.saved);}
     else if(ap.auth==WIFI_AUTH_WPA2_PSK){++known;assert(ap.supported&&ap.saved&&ap.rssi==-28);}
     else{++unsupported;assert(!ap.supported&&!ap.saved);}
    }else if(!std::strcmp(ap.ssid,OFFICE)){++unknown;assert(ap.supported&&!ap.saved);}
   }
   assert(known==1&&open==1&&unsupported==1&&unknown==1);
   assert(submit(command(Job::Connect,OFFICE,OFFICE_KEY)));break;
  }
  case 2:assert(s.connected&&s.savedCount==2&&saved(s,HOME_NET)&&saved(s,OFFICE));
   assert(prefs.isKey(BOOK));before=*prefs.blob(BOOK);initialBegins=WiFi.beginCount;
   // Malformed saved commands must not inject a supplied password.
   assert(!submit(command(Job::Connect,HOME_NET,WRONG_KEY,false,true)));
   // Unknown security class, regardless of whether admission or worker rejects it.
   (void)submit(command(Job::Connect,HOME_NET,"",true,true));break;
  case 3:assert(WiFi.beginCount==initialBegins);expectBook(before);
   assert(submit(command(Job::Connect,HOME_NET,WRONG_KEY)));break;
  case 4:assert(!s.connected&&s.savedCount==2);expectBook(before);click(HOME_NET);break;
  case 5:assert(s.connected&&WiFi.lastBeginSsid==HOME_NET&&WiFi.lastBeginPassword==HOME_KEY);
   click(OFFICE);break;
  case 6:assert(s.connected&&s.autoConnect&&WiFi.lastBeginSsid==OFFICE&&WiFi.lastBeginPassword==OFFICE_KEY);
   transfer(file,true);throw EndWorker{};
  default:assert(false);
 }});
}
void roam(){
 available.erase(OFFICE);bool reached=false;unsigned seen=0;
 run([&]{auto s=shot();
  if(!s.connected){assert(WiFi.beginCount<=1);return;}
  if(!reached){
   assert(s.savedCount==2&&s.autoConnect&&WiFi.attemptedSsids==std::vector<String>({OFFICE,HOME_NET}));
   assert(WiFi.lastBeginPassword==HOME_KEY&&prefs.credentialWrites==0);reached=true;
   seen=WiFi.beginCount;fakeMillis+=3600000U/4;return;
  }
  assert(seen==WiFi.beginCount&&prefs.credentialWrites==0);throw EndWorker{};
 });assert(reached);
}
void tapOffline(){
 const auto before=*prefs.blob(BOOK);bool requested=false;uint32_t at=0;
 run([&]{const auto s=shot();
  if(!requested){
   assert(s.connected&&s.autoConnect&&s.savedCount==2&&WiFi.lastBeginSsid==OFFICE);
   available.erase(HOME_NET);requested=true;at=fakeMillis;click(HOME_NET);return;
  }
  assert(fakeMillis-at<120000U);
  if(!s.connected)return;
  assert(s.autoConnect&&s.savedCount==2&&WiFi.attemptedSsids.size()>=3);
  assert(WiFi.attemptedSsids[1]==HOME_NET&&WiFi.lastBeginSsid==OFFICE&&WiFi.lastBeginPassword==OFFICE_KEY);
  expectBook(before);assert(prefs.credentialWrites==0);throw EndWorker{};
 });
}
void forgetActive(const char* file){
 bool requested=false;uint32_t at=0;
 run([&]{const auto s=shot();
  if(!requested){
   assert(s.connected&&s.autoConnect&&s.savedCount==2&&WiFi.lastBeginSsid==OFFICE);
   at=fakeMillis;requested=true;assert(submit(command(Job::Forget,OFFICE)));return;
  }
  assert(fakeMillis-at<120000U&&s.autoConnect&&prefs.autoValue&&s.savedCount==1&&saved(s,HOME_NET)&&!saved(s,OFFICE));
  if(!s.connected)return;
  assert(WiFi.attemptedSsids==std::vector<String>({OFFICE,HOME_NET})&&WiFi.lastBeginPassword==HOME_KEY);
  transfer(file,true);throw EndWorker{};
 });
}
void afterForget(){
 run([&]{const auto s=shot();assert(s.connected&&s.autoConnect&&s.savedCount==1&&saved(s,HOME_NET));
  assert(WiFi.beginCount==1&&WiFi.lastBeginSsid==HOME_NET&&WiFi.lastBeginPassword==HOME_KEY);throw EndWorker{};
 });
}
void faults(){
 const auto before=*prefs.blob(BOOK);unsigned n=0,beginCount=0,writes=0,disconnects=0;
 std::vector<unsigned char> beforeForget;
 run([&]{const auto s=shot();switch(n++){
  case 0:assert(s.connected&&s.savedCount==2);beginCount=WiFi.beginCount;
   legacy();writes=prefs.credentialWrites;prefs.failLengthKey=BOOK;assert(!prefs.isKey(BOOK));click(HOME_NET);break;
  case 1:assert(WiFi.beginCount==beginCount&&prefs.credentialWrites==writes);expectBook(before);
   available["fixture-extra"]=BULK_KEY;
   // The high-level wrapper reports false both for missing and for failed metadata.
   // A stale legacy profile must not become a substitute book for a manual join.
   assert(submit(command(Job::Connect,"fixture-extra",BULK_KEY)));break;
  case 2:assert(s.connected&&WiFi.beginCount==beginCount&&prefs.credentialWrites==writes);expectBook(before);
   prefs.failLengthKey.clear();prefs.failWriteKey=BOOK;
   assert(submit(command(Job::Connect,"fixture-extra",BULK_KEY)));break;
  case 3:assert(s.savedCount==2&&saved(s,HOME_NET)&&saved(s,OFFICE)&&!saved(s,"fixture-extra"));
   prefs.failWriteKey.clear();expectBook(before);click(HOME_NET);break;
  case 4:assert(s.connected&&s.autoConnect&&WiFi.lastBeginSsid==HOME_NET&&WiFi.lastBeginPassword==HOME_KEY);
   beforeForget=*prefs.blob(BOOK);beginCount=WiFi.beginCount;disconnects=WiFi.disconnectCount;writes=prefs.credentialWrites;
   prefs.failLengthKey=BOOK;assert(submit(command(Job::Forget,OFFICE)));break;
  case 5: // An unreadable inactive profile must not disturb this working link or its policy.
   expectBook(beforeForget);assert(s.connected&&s.autoConnect&&prefs.autoValue&&s.savedCount==2);
   assert(WiFi.beginCount==beginCount&&WiFi.disconnectCount==disconnects&&prefs.credentialWrites==writes);
   assert(submit(command(Job::Forget,HOME_NET)));break;
  case 6: // An unreadable ACTIVE profile cannot be claimed deleted or automatically rejoined.
   expectBook(beforeForget);assert(!s.connected&&!s.autoConnect&&!prefs.autoValue&&s.savedCount==2);
   assert(saved(s,HOME_NET)&&saved(s,OFFICE)&&std::strstr(s.wifiMessage,"Не удалось"));
   assert(WiFi.disconnectCount==disconnects+1&&prefs.credentialWrites==writes);
   prefs.failLengthKey.clear();fakeMillis+=310000U;break;
  case 7:assert(!s.connected&&WiFi.beginCount==beginCount&&prefs.credentialWrites==writes);
   expectBook(beforeForget);throw EndWorker{};
  default:assert(false);
 }});
}
void openTwin(){
 unsigned n=0;wifiDriver.records={access(HOME_NET,-25),access(HOME_NET,-37,WIFI_AUTH_OPEN),access(HOME_NET,-50,90)};
 run([&]{const auto s=shot();switch(n++){
  case 0:available[HOME_NET]="";assert(submit(command(Job::Connect,HOME_NET,"",true)));break;
  case 1:assert(s.connected&&WiFi.lastBeginOpen&&s.savedCount==3&&saved(s,HOME_NET,true)&&saved(s,HOME_NET)&&saved(s,OFFICE));
   assert(submit(command(Job::WifiScan)));break;
  case 2:{
   unsigned secure=0,open=0,unsupported=0;
   for(unsigned i=0;i<s.apCount;++i){const auto& ap=s.aps[i];
    if(ap.auth==WIFI_AUTH_OPEN){++open;assert(ap.saved);}
    else if(ap.auth==WIFI_AUTH_WPA2_PSK){++secure;assert(ap.saved);}
    else{++unsupported;assert(!ap.saved&&!ap.supported);}
   }
   assert(secure==1&&open==1&&unsupported==1);
   available[HOME_NET]=HOME_KEY;click(HOME_NET);break;
  }
  case 3:assert(s.connected&&!WiFi.lastBeginOpen&&WiFi.lastBeginPassword==HOME_KEY);
   available[HOME_NET]="";click(HOME_NET,true);break;
  case 4:assert(s.connected&&WiFi.lastBeginOpen&&WiFi.lastBeginPassword.empty()&&s.savedCount==3);throw EndWorker{};
  default:assert(false);
 }});
}
void capacity(){
 unsigned n=0,atLimit=0;std::vector<unsigned char> full;
 run([&]{const auto s=shot();
  if(n<MAX_WIFI_PROFILES-2U){
   const auto next="bulk-"+std::to_string(n++);available[next]=BULK_KEY;
   assert(submit(command(Job::Connect,next.c_str(),BULK_KEY)));return;
  }
  if(n==MAX_WIFI_PROFILES-2U){
   assert(s.savedCount==MAX_WIFI_PROFILES&&saved(s,HOME_NET)&&saved(s,OFFICE));
   full=*prefs.blob(BOOK);atLimit=WiFi.beginCount;++n;available["bulk-over-limit"]=BULK_KEY;
   (void)submit(command(Job::Connect,"bulk-over-limit",BULK_KEY));return;
  }
  assert(s.savedCount==MAX_WIFI_PROFILES&&saved(s,HOME_NET)&&saved(s,OFFICE)&&!saved(s,"bulk-over-limit"));
  assert(s.connected&&WiFi.beginCount==atLimit);expectBook(full);throw EndWorker{};
 });
}
void disconnect(const char* file){
 unsigned n=0,begins=0;run([&]{auto s=shot();
  if(n++==0){assert(s.connected);begins=WiFi.beginCount;assert(submit(command(Job::Disconnect)));return;}
  assert(!s.connected&&!s.autoConnect&&!prefs.autoValue&&WiFi.beginCount==begins&&s.savedCount==2&&saved(s,HOME_NET)&&saved(s,OFFICE));
  if(n==2){fakeMillis+=310000U;return;}
  transfer(file,true);throw EndWorker{};
 });
}
void manual(const char* file){
 unsigned n=0;run([&]{auto s=shot();if(n++==0){assert(!s.connected&&!s.autoConnect&&s.savedCount==2&&!WiFi.beginCount);click(OFFICE);return;}
  assert(s.connected&&s.autoConnect&&WiFi.lastBeginSsid==OFFICE&&WiFi.lastBeginPassword==OFFICE_KEY);
  transfer(file,true);throw EndWorker{};
 });
}
void forget(const char* file){
 unsigned n=0,originalBegins=0,originalDisconnects=0;
 run([&]{const auto s=shot();switch(n++){
  case 0:assert(s.connected&&s.savedCount==2&&WiFi.lastBeginSsid==OFFICE);
   originalBegins=WiFi.beginCount;originalDisconnects=WiFi.disconnectCount;(void)submit(command(Job::Forget));break;
  case 1:assert(s.savedCount==2&&s.connected&&s.autoConnect);assert(submit(command(Job::Forget,HOME_NET)));break;
  case 2:assert(s.savedCount==1&&!saved(s,HOME_NET)&&saved(s,OFFICE)&&s.connected&&s.autoConnect);
   assert(WiFi.beginCount==originalBegins&&WiFi.disconnectCount==originalDisconnects);
   assert(submit(command(Job::Forget,OFFICE)));break;
  case 3:assert(!s.connected&&!s.autoConnect&&!s.hasSaved&&s.savedCount==0);
   assert(!prefs.isKey("credentials")&&prefs.isKey(BOOK));privateKeys();
   // A leftover/restored old record must not supersede an authoritative empty book.
   legacy();prefs.autoValue=true;transfer(file,true);throw EndWorker{};
  default:assert(false);
 }});
}
void noFallback(bool damaged,bool unreadable){
 if(damaged||unreadable){legacy();prefs.autoValue=true;}
 if(damaged){auto& b=prefs.blobs.at(BOOK);assert(!b.empty());b[0]^=0xff;}
 if(unreadable)prefs.failReadKey=BOOK;
 prefs.credentialWrites=0;unsigned n=0;
 run([&]{auto s=shot();assert(!s.hasSaved&&s.savedCount==0&&!WiFi.beginCount&&!s.connected&&prefs.credentialWrites==0);
  if(n++==0){fakeMillis+=310000U;return;}throw EndWorker{};
 });
}
} // namespace
int main(int argc,char** argv){
 assert(argc==3);assert(begin());driver();const std::string mode=argv[1];
 if(mode=="seed")setup(argv[2]);else{
  transfer(argv[2],false);
  if(mode=="roam")roam();else if(mode=="faults")faults();else if(mode=="limit")capacity();else if(mode=="open")openTwin();
  else if(mode=="tap-offline")tapOffline();else if(mode=="forget-active")forgetActive(argv[2]);else if(mode=="after-forget")afterForget();
  else if(mode=="disconnect")disconnect(argv[2]);else if(mode=="manual")manual(argv[2]);else if(mode=="forget")forget(argv[2]);
  else if(mode=="corrupt"||mode=="unreadable"||mode=="empty")noFallback(mode=="corrupt",mode=="unreadable");else assert(false);
 }
 vSemaphoreDelete(stateMutex);stateMutex=nullptr;
 std::cout<<"PASS: saved Wi-Fi profiles "<<mode<<" (fresh host process; synthetic RF/NVS only)\n";
}
