"""Compile the actual HTTP updater guard/handlers and status serialization.

Only transport/RTOS/backend side effects are faked. The source slices are taken
from panel_web.cpp so these tests exercise the firmware's actual decisions.
No network connection or hardware access is made.
"""
from pathlib import Path
import json
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/panel_web.cpp").read_text(encoding="utf-8")
backend = (root / "src/panel_update.cpp").read_text(encoding="utf-8")
session = source[source.index("panel_pairing::State pairing;"):source.index("void jsonQuote(")]
pair = source[source.index("void pairRequest() {"):source.index("void statusRequest() {")]
logout = source[source.index("void logoutRequest() {"):source.index("void notFound() {")]
digits = source[source.index("bool allDigits("):source.index("uint32_t asUint(")]
auth = source[source.index("bool authorized() {"):source.index("void pairStartRequest() {")]
command_start = source.index("void commandRequest() {")
commands = source[command_start:source.index('  if(kind=="ha_discover"){', command_start)]
commands += '\n  ++otherMutations;jsonResponse(202,"{}");\n}\n'
quote = source[source.index("void jsonQuote("):source.index("const char* fuelName(")]
status_start = source.index("  panel_update::Snapshot update;", source.index("void statusRequest() {"))
status = source[status_start:source.index('  j+=",\\"service\\":', status_start)]
phase = backend[backend.index("const char* phaseName(Phase p){"):backend.index("} // namespace panel_update", backend.index("const char* phaseName(Phase p){"))]
template = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <map>
#include <string>
#include <type_traits>
#include <vector>
#include "panel_update.h"
#include "panel_pairing.h"
class String:public std::string{
public:
 using std::string::string;
 String()=default;String(const std::string& s):std::string(s){}
 template<class T,typename std::enable_if<std::is_arithmetic<T>::value,int>::type=0>
 String(T n):std::string(std::to_string(n)){}
 bool startsWith(const char* s)const{return rfind(s,0)==0;}
 bool isEmpty()const{return empty();}
};
struct Server{
 std::map<std::string,String> args,headers;
 String arg(const char* n){return args[n];}String header(const char* n){return headers[n];}
}server;
struct IP {String toString()const{return "192.0.2.10";}};
struct Wifi{IP localIP()const{return {};}}WiFi;
bool guard=true,lockAllowed=true;constexpr int pdTRUE=1;
int pdMS_TO_TICKS(int n){return n;}int xSemaphoreTake(bool,int){return lockAllowed?pdTRUE:0;}
void xSemaphoreGive(bool){}
// Independent 64-bit simulated uptime; millis() intentionally wraps like Arduino.
int64_t nowUs=1000000;
uint32_t millis(){return uint32_t(nowUs/1000);}
int64_t esp_timer_get_time(){return nowUs;}
void esp_fill_random(void* out,size_t size){memset(out,0xff,size);}
SESSION_SOURCE
int response=0,otherMutations=0;std::string errorCode;
void errorResponse(int status,const char* error){response=status;errorCode=error;}
void jsonResponse(int status,const String&){response=status;errorCode.clear();}
namespace panel_update{
 Snapshot fixture;bool stateAvailable=true,accept=true,installRace=false;
 unsigned called=0,accepted=0;std::string requested;
 bool snapshot(Snapshot& out){out=fixture;return stateAvailable;}
 bool acceptRequest(){++called;if(accept)++accepted;return accept;}
 bool requestCheck(){return acceptRequest();}
 bool requestCancel(){return acceptRequest();}
 bool requestInstall(const char* version){
  ++called;requested=version;
  if(installRace)strcpy(fixture.latestVersion,"v1.2.0");
  const bool ok=accept&&requested==fixture.latestVersion;
  if(ok)++accepted;
  return ok;
 }
 bool requestRollback(const char* version){requested=version;return acceptRequest();}
 PHASE_SOURCE
}
QUOTE_SOURCE
AUTH_SOURCE
DIGITS_SOURCE
PAIR_SOURCE
LOGOUT_SOURCE
COMMAND_SOURCE
String serialized(){
 String j="{\"ok\":true";
 STATUS_SOURCE
 j+='}';return j;
}
void reset(const char* action,int64_t issuedUs=1000000){
 server.args.clear();server.headers.clear();
 server.args["action"]=action;
 server.headers["Authorization"]="Bearer ffffffffffffffffffffffffffffffff";
 server.headers["X-Boiler-Pairing"]="1";
 server.headers["Origin"]="http://192.0.2.10";
 server.headers["Sec-Fetch-Site"]="same-origin";
 response=0;errorCode.clear();otherMutations=0;guard=lockAllowed=true;
 nowUs=issuedUs;pairing={};clearSessionLocked();
 // Create sessions through the real pairing handler, not a copied deadline formula.
 const char* ticket="0123456789abcdef0123456789abcdef";
 assert(pairing.request(ticket,"123456",millis())==panel_pairing::Request::Started);
 server.args["request_id"]=ticket;server.args["code"]="123456";
 pairRequest();assert(response==200);assert(sessionToken[0]);
 response=0;errorCode.clear();
 using namespace panel_update;
 fixture={};fixture.started=fixture.supported=fixture.available=fixture.rollbackAvailable=true;
 strcpy(fixture.latestVersion,"v1.1.0");strcpy(fixture.previousVersion,"v0.9.0");
 stateAvailable=accept=true;installRace=false;called=accepted=0;requested.clear();
}
void expectStatus(int code){
 commandRequest();
 if(response!=code)fprintf(stderr,"uptime=%lld us: expected HTTP %d, got %d\n",
                          static_cast<long long>(nowUs),code,response);
 assert(response==code);
}
int main(){
 using namespace panel_update;
 // Actual token and Origin validation, including expired/contended sessions.
 for(const char* action:{"update_check","update_install","update_cancel","update_rollback"}){
  reset(action);server.headers.erase("Authorization");expectStatus(401);assert(!called);
  reset(action);server.headers["Authorization"]="Bearer 00000000000000000000000000000000";expectStatus(401);assert(!called);
  reset(action);server.headers["Origin"]="https://unrelated.invalid";expectStatus(403);assert(!called);
  reset(action);server.headers["Sec-Fetch-Site"]="cross-site";expectStatus(403);assert(!called);
  reset(action);server.headers.erase("X-Boiler-Pairing");expectStatus(403);assert(!called);
 }
 // Exact one-hour boundary and no intervening requests over multiple millis wraps.
 const int64_t issuedUs=1000000, hourUs=3600000000LL, dayUs=86400000000LL;
 reset("update_check");nowUs=issuedUs+hourUs-1;expectStatus(202);assert(accepted==1);
 reset("update_check");nowUs=issuedUs+hourUs;expectStatus(401);assert(!called);
 for(const int64_t elapsed:std::initializer_list<int64_t>{hourUs+1, hourUs+(0x80000001LL*1000),
                            25*dayUs,50*dayUs,100*dayUs}){
  reset("update_check");nowUs=issuedUs+elapsed;expectStatus(401);assert(!called);
 }
 // Fresh sessions still last a full hour when issued after long uptime or near wrap.
 for(const int64_t start:std::initializer_list<int64_t>{
       25*dayUs,50*dayUs,100*dayUs,0xffffffffLL*1000-1000000}){
  reset("update_check",start);expectStatus(202);assert(accepted==1);
  nowUs=start+hourUs-1;expectStatus(202);assert(accepted==2);
  nowUs=start+hourUs;expectStatus(401);assert(called==2);
 }
 // An already observed expiration must not later revive either.
 reset("update_check");nowUs=issuedUs+hourUs;expectStatus(401);
 nowUs=issuedUs+hourUs+(0x80000001LL*1000);expectStatus(401);assert(!called);
 // Logout removes the bearer permanently, without affecting fresh pairing.
 reset("update_check");logoutRequest();assert(response==200&&!sessionToken[0]);
 expectStatus(401);assert(!called);
 nowUs=issuedUs+100*dayUs;expectStatus(401);assert(!called);
 reset("update_check");expectStatus(202);assert(accepted==1);
 reset("update_check");lockAllowed=false;expectStatus(401);
 reset("update_check");stateAvailable=false;expectStatus(503);assert(!called);
 reset("update_check");fixture.supported=false;expectStatus(503);assert(!called);
 reset("update_check");expectStatus(202);assert(accepted==1);
 reset("update_check");accept=false;expectStatus(409);assert(accepted==0);
 // Exact version + independent confirmation are mandatory, never inferred.
 reset("update_install");expectStatus(400);assert(!called);
 reset("update_install");server.args["version"]="v1.1.0";expectStatus(400);assert(!called);
 reset("update_install");server.args["version"]="v1.1.0";server.args["confirm"]="yes";expectStatus(400);
 reset("update_install");server.args["version"]=server.args["confirm"]="v9.9.9";expectStatus(409);assert(!called);
 reset("update_install");fixture.available=false;server.args["version"]=server.args["confirm"]="v1.1.0";expectStatus(409);assert(!called);
 reset("update_install");std::string embedded="v1.1.0";embedded.push_back('\0');embedded+="suffix";
 server.args["version"]=server.args["confirm"]=embedded;expectStatus(400);assert(!called);
 reset("update_install");server.args["version"]=server.args["confirm"]=std::string(24,'v');expectStatus(400);
 reset("update_install");server.args["version"]=server.args["confirm"]="v1.1.0";expectStatus(202);assert(accepted==1&&requested=="v1.1.0");
 reset("update_install");server.args["version"]=server.args["confirm"]="v1.1.0";installRace=true;expectStatus(409);assert(called==1&&accepted==0&&requested=="v1.1.0");
 reset("update_rollback");server.args["version"]=server.args["confirm"]="v0.9.0";expectStatus(202);assert(accepted==1&&requested=="v0.9.0");
 reset("update_rollback");fixture.rollbackAvailable=false;server.args["version"]=server.args["confirm"]="v0.9.0";expectStatus(409);assert(!called);
 reset("update_rollback");server.args["version"]=server.args["confirm"]="v1.1.0";expectStatus(409);assert(!called);
 // All other mutating command branches are unreachable during OTA/self-test.
 for(const char* action:{"wifi_scan","wifi_disconnect","wifi_forget","wifi_connect","ble_scan","clock","screen","ha_save","ha_discover","sd_format_prepare","connection_set","connection_pin_set","connection_pin_clear","connection_rollback","update_install","update_rollback","update_check"}){
  reset(action);fixture.busy=true;expectStatus(409);assert(!otherMutations&&!called);
  reset(action);fixture.bootPending=true;expectStatus(409);assert(!otherMutations&&!called);
 }
 reset("wifi_scan");expectStatus(202);assert(otherMutations==1);
 reset("update_cancel");expectStatus(409);assert(!called);
 reset("update_cancel");fixture.busy=true;expectStatus(409);assert(!called);
 reset("update_cancel");fixture.busy=fixture.cancelAllowed=true;expectStatus(202);assert(accepted==1);
 reset("update_cancel");fixture.busy=fixture.cancelAllowed=true;accept=false;expectStatus(409);assert(!accepted);
 reset("update_check");
 fixture.phase=Phase::RolledBack;fixture.done=3700;fixture.total=10000;fixture.percent=37;fixture.lastCheck=123;
 fixture.bootPending=fixture.automaticRollback=true;fixture.errorCode=19;
 strcpy(fixture.message,"Сбой \"файла\"\nПовторите");
 const auto json=serialized();puts(json.c_str());
}
'''
for key, value in {"SESSION_SOURCE": session, "DIGITS_SOURCE": digits, "PAIR_SOURCE": pair,
                   "LOGOUT_SOURCE": logout, "PHASE_SOURCE": phase, "QUOTE_SOURCE": quote, "AUTH_SOURCE": auth,
                   "COMMAND_SOURCE": commands, "STATUS_SOURCE": status}.items():
    template = template.replace(key, value)
with tempfile.TemporaryDirectory(prefix="boiler-update-web-") as temporary:
    directory = Path(temporary)
    cpp = directory / "handlers.cpp"
    binary = directory / "handlers"
    cpp.write_text(template, encoding="utf-8")
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-I", str(root / "src"),
                    str(cpp), "-o", str(binary)], check=True)
    output = subprocess.check_output([str(binary)], text=True)
data = json.loads(output)["update"]
assert set(data) == {
    "started", "supported", "busy", "available", "cancel_allowed", "rollback_available",
    "boot_pending", "automatic_rollback", "phase", "done", "total", "percent", "last_check",
    "latest_version", "previous_version", "message", "error_code",
}
assert data["phase"] == "rolled_back"
assert data["message"] == 'Сбой "файла"\nПовторите'
assert data["done"] == 3700 and data["total"] == 10000 and data["percent"] == 37
assert data["latest_version"] == "v1.1.0" and data["previous_version"] == "v0.9.0"
assert data["automatic_rollback"] and data["boot_pending"] and data["error_code"] == 19
print("PASS: actual pairing/auth/logout with expiry boundary and 25/50/100-day idle sessions, HTTP Origin, precise confirmation and version, backend race rejection, busy/cancel/rollback guards, complete escaped update status")
