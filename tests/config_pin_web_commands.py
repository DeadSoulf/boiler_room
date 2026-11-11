"""Exercise actual pin HTTP handlers, their guards, and the real config/NVS module.

The HTTP transport, OTA backend and RTOS/NVS devices are host fakes. No request
is sent to a device, and pin commands never reach the unrelated command queue.
"""
from config_pin_tests import ROOT, run_host

source = (ROOT / "src/panel_web.cpp").read_text(encoding="utf-8")
session = source[source.index("panel_pairing::State pairing;"):source.index("void jsonQuote(")]
quote = source[source.index("void jsonQuote("):source.index("const char* fuelName(")]
digits = source[source.index("bool allDigits("):source.index("#if BP_WEB_ENABLED", source.index("bool allDigits("))]
responses = source[source.index("void responseHeaders() {"):source.index("void homepage() {")]
auth = source[source.index("bool authorized() {"):source.index("void pairStartRequest() {")]
pair = source[source.index("void pairRequest() {"):source.index("void statusRequest() {")]
start = source.index("void commandRequest() {")
guards = source[start:source.index('  if(kind=="ha_discover"){', start)]
pin_start = source.index('  if(kind=="connection_pin_set"||kind=="connection_pin_clear"){', start)
pins = source[pin_start:source.index('  if(kind=="connection_rollback"){', pin_start)]
commands = guards + pins + '\n  ++otherCommands;errorResponse(404,"unknown_action");\n}\n'

template = r'''
#include "config_pin_fixture.h"
#include "panel_update.h"
#include "panel_pairing.h"
#include <cstdlib>
#include <type_traits>
class String:public std::string{
public:
 using std::string::string;
 String()=default;String(const std::string& value):std::string(value){}
 template<class T,typename std::enable_if<std::is_arithmetic<T>::value,int>::type=0>
 String(T number):std::string(std::to_string(number)){}
 bool startsWith(const char* value)const{return rfind(value,0)==0;}
 bool isEmpty()const{return empty();}
};
struct Server{
 std::map<std::string,String> args,headers,sentHeaders;
 int status=0;String body,contentType;
 String arg(const char* key){return args[key];}
 String header(const char* key){return headers[key];}
 void sendHeader(const char* key,const char* value){sentHeaders[key]=value;}
 void send(int value,const char* type,const String& payload){status=value;body=payload;contentType=type;}
}server;
struct IP{String toString()const{return "192.0.2.10";}};
struct Wifi{IP localIP()const{return {};}}WiFi;
config_host::Mutex authMutex;SemaphoreHandle_t guard=&authMutex;
int64_t nowUs=1000000;
uint32_t millis(){return uint32_t(nowUs/1000);}
int64_t esp_timer_get_time(){return nowUs;}
void esp_fill_random(void* output,size_t length){memset(output,0xff,length);}
SESSION_SOURCE
QUOTE_SOURCE
DIGITS_SOURCE
RESPONSE_SOURCE
AUTH_SOURCE
PAIR_SOURCE
namespace panel_update{
 Snapshot fixture;bool available=true;unsigned calls=0;
 bool snapshot(Snapshot& out){out=fixture;return available;}
 bool requestCheck(){++calls;return true;}
 bool requestCancel(){++calls;return true;}
 bool requestInstall(const char*){++calls;return true;}
 bool requestRollback(const char*){++calls;return true;}
}
unsigned otherCommands=0;
COMMAND_SOURCE
using namespace panel_config;
using namespace config_test;
void request(const char* action="connection_pin_set",const char* revision="0"){
 config_host::resetFaults();authMutex.held=false;
 server={};server.args["action"]=action;server.args["gpio"]="52";server.args["revision"]=revision;
 if(std::string(action)=="connection_pin_set"){
  server.args["channel"]="0";server.args["type"]="1";server.args["debounce"]="50";
 }
 server.headers["Authorization"]="Bearer ffffffffffffffffffffffffffffffff";
 server.headers["X-Boiler-Pairing"]="1";server.headers["Origin"]="http://192.0.2.10";
 server.headers["Sec-Fetch-Site"]="same-origin";
 nowUs=1000000;pairing={};clearSessionLocked();
 const char* ticket="0123456789abcdef0123456789abcdef";
 assert(pairing.request(ticket,"123456",millis())==panel_pairing::Request::Started);
 server.args["request_id"]=ticket;server.args["code"]="123456";
 pairRequest();assert(server.status==200&&sessionToken[0]);
 server.status=0;server.body.clear();server.sentHeaders.clear();
 panel_update::fixture={};panel_update::available=true;panel_update::calls=0;otherCommands=0;
}
void expectError(int status,const char* code,bool noWrites=true){
 const unsigned writes=config_host::writes;const auto before=config_host::nvs;
 commandRequest();
 if(server.status!=status)fprintf(stderr,"Expected HTTP %d/%s, got %d/%s\n",status,code,server.status,server.body.c_str());
 assert(server.status==status);
 assert(server.body==std::string("{\"ok\":false,\"error\":\"")+code+"\"}");
 assert(!panel_update::calls&&!otherCommands);
 assert(server.sentHeaders["Cache-Control"]=="no-store, max-age=0");
 assert(server.contentType=="application/json; charset=utf-8");
 if(noWrites)assert(config_host::writes==writes&&config_host::nvs==before);
}
void expectSaved(uint32_t revision){
 commandRequest();assert(server.status==200);
 assert(server.body==std::string("{\"ok\":true,\"saved\":true,\"revision\":")+std::to_string(revision)+"}");
 assert(!panel_update::calls&&!otherCommands);
 assert(state().revision==revision);
}
void guards(){
 fresh();
 for(const char* action:{"connection_pin_set","connection_pin_clear"}){
  request(action);server.headers.erase("Authorization");expectError(401,"pair_required");
  request(action);server.headers["Authorization"]="Bearer 00000000000000000000000000000000";expectError(401,"pair_required");
  request(action);nowUs+=3600000000LL;expectError(401,"pair_required");
  request(action);nowUs+=100LL*86400000000LL;expectError(401,"pair_required");
  request(action);server.headers["Origin"]="https://other.invalid";expectError(403,"same_origin_required");
  request(action);server.headers["Sec-Fetch-Site"]="cross-site";expectError(403,"same_origin_required");
  request(action);server.headers.erase("X-Boiler-Pairing");expectError(403,"same_origin_required");
  request(action);panel_update::available=false;expectError(503,"update_state_unavailable");
  request(action);panel_update::fixture.busy=true;expectError(409,"update_busy");
  request(action);panel_update::fixture.bootPending=true;expectError(409,"update_busy");
  request(action);lock->held=true;expectError(503,"configuration_unavailable");lock->held=false;
  request(action);powerOff();expectError(503,"configuration_unavailable");assert(begin());
 }
 assert(config_host::writes==0&&state().revision==0);
}
void badInputs(){
 fresh();
 for(const char* action:{"connection_pin_set","connection_pin_clear"}){
  for(const char* value:{"","-1","+52"," 52","52x","520","1.5"}){
   request(action);server.args["gpio"]=value;expectError(400,"invalid_pin_fields");
  }
  for(const char* value:{"0","3","14","35","39","54","55","99"}){
   request(action);server.args["gpio"]=value;expectError(400,"invalid_pin_assignment");
  }
  for(const char* value:{"","-1","+0","0x0","1.0","4294967296","99999999999999999"}){
   request(action);server.args["revision"]=value;expectError(400,"invalid_pin_fields");
  }
  request(action);server.args["revision"]=std::string("0\0ignored",9);expectError(400,"invalid_pin_fields");
  request(action);server.args.erase("gpio");expectError(400,"invalid_pin_fields");
  request(action);server.args.erase("revision");expectError(400,"invalid_pin_fields");
 }
 for(const char* field:{"channel","type","debounce"}){
  request();server.args.erase(field);expectError(400,"invalid_pin_fields");
  request();server.args[field]="-1";expectError(400,"invalid_pin_fields");
  request();server.args[field]="1x";expectError(400,"invalid_pin_fields");
 }
 for(const char* channel:{"7","9","255"}){request();server.args["channel"]=channel;expectError(400,"invalid_pin_fields");}
 for(const char* type:{"0","4","5","9"}){request();server.args["type"]=type;expectError(400,"invalid_pin_fields");}
 request();server.args["debounce"]="5001";expectError(400,"invalid_pin_fields");
 request();server.args["debounce"]="0";expectError(400,"invalid_pin_assignment");
 request();server.args["type"]="2";expectError(400,"invalid_pin_assignment");
 request();server.args["channel"]="4";expectError(400,"invalid_pin_assignment");
 assert(state().revision==0&&config_host::writes==0);
 request();server.args["channel"]="6";expectSaved(1);assert(state().item[6].gpio==52);
}
void repliesAndAtomicMutation(){
 fresh();request("connection_pin_set","1");expectError(409,"stale_revision");
 request();expectSaved(1);assert(state().item[0].gpio==52&&config_host::writes==1);
 request();expectError(409,"stale_revision");
 request("connection_pin_set","1");expectSaved(1);assert(config_host::writes==1);
 request("connection_pin_clear","0");expectError(409,"stale_revision");
 request("connection_pin_clear","1");expectSaved(2);assert(state().item[0].type==Type::None&&config_host::writes==2);
 request("connection_pin_clear","2");expectSaved(2);assert(config_host::writes==2);
 State x,y;twoAccepted(x,y);request("connection_pin_set","2");server.args["channel"]="1";
 const auto writes=config_host::writes;expectSaved(3);assert(config_host::writes==writes+1);
 assert(state().item[0].type==Type::None&&state().item[1].gpio==52);
 request("connection_pin_clear","2");expectError(409,"stale_revision");
 request("connection_pin_set","3");server.args["channel"]="5";server.args["type"]="3";server.args["debounce"]="5000";
 expectSaved(4);assert(state().item[1].type==Type::None&&state().item[5].gpio==52&&state().item[5].debounceMs==5000);
 request("connection_pin_set","4");server.args["channel"]="4";server.args["type"]="2";server.args["debounce"]="0";
 expectSaved(5);assert(state().item[5].type==Type::None&&state().item[4].type==Type::ReedNO);
 // Numeric uint32 maximum is valid and the success revision is taken under the mutex.
 fresh();current.revision=0xffffffffU;request("connection_pin_set","4294967295");expectSaved(0);
}
void storageErrors(){
 for(const char* action:{"connection_pin_set","connection_pin_clear"}){
  State x,y;twoAccepted(x,y);request(action,"2");server.args["channel"]="1";
  config_host::failWrite=true;expectError(503,"configuration_write_failed",false);unchanged(y);
  const auto before=config_host::nvs;
  request(action,"2");server.args["channel"]="1";
  config_host::afterWrite=[](){config_host::failRead=true;};
  expectError(503,"configuration_write_failed",false);unchanged(y);assert(config_host::nvs==before);
  reboot();unchanged(y);
 }
}
int main(){
 guards();badInputs();repliesAndAtomicMutation();storageErrors();powerOff();
 puts("PASS: actual pin HTTP/auth/Origin/64-bit expiry/OTA guards with real config/NVS; strict numeric/body validation, 400/409/503 errors, exact saved/revision replies, no-op, atomic replacement, stale and offline/unavailable rejection without unrelated commands");
}
'''
for key, value in {
    "SESSION_SOURCE": session, "QUOTE_SOURCE": quote, "DIGITS_SOURCE": digits,
    "RESPONSE_SOURCE": responses, "AUTH_SOURCE": auth, "PAIR_SOURCE": pair,
    "COMMAND_SOURCE": commands,
}.items():
    template = template.replace(key, value)
print(run_host(template), end="")
