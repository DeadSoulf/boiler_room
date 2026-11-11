"""Run the embedded heating GET and POST field parser with host transport fakes."""
import json
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
src = (root / "src/panel_web.cpp").read_text()
digits = src[src.index("bool allDigits("):src.index("#if BP_WEB_ENABLED", src.index("bool allDigits("))]
quoted = src[src.index("void jsonQuote("):src.index("const char* fuelName(")]
handlers = src[src.index("struct HeatingField {"):src.index("void worker(void*) {")]
command = src[src.index("void commandRequest() {"):src.index("  panel::Command cmd;", src.index("void commandRequest() {"))]
assert command.index("!authorized()") < command.index("!pairingOriginAllowed()") < command.index("if(kind==\"heating_settings\")")
guard = command.index("// Read/status and pairing")
assert guard < command.index("if(update.busy||update.bootPending)", guard) < command.index("if(kind==\"heating_settings\")")
assert 'server.on("/api/heating",HTTP_GET,heatingRequest)' in src
driver = r'''
#include "panel_heating_settings.h"
#include "panel_heating_runtime.h"
#include <string>
#include <type_traits>
#include <utility>
#include <cstdio>
#include <map>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <cassert>
using std::isfinite;
class String:public std::string{
public:
 using std::string::string;
 String()=default;
 String(const std::string& s):std::string(s){}
 template<class T,typename std::enable_if<std::is_arithmetic<T>::value,int>::type=0>
 String(T value):std::string(std::to_string(value)){}
 String(float value,unsigned precision){
   char buf[48];snprintf(buf,sizeof(buf),"%.*f",int(precision),double(value));assign(buf);
 }
 bool isEmpty()const{return empty();}
};
struct Server{
 std::map<std::string,String> args;
 bool hasArg(const char* k)const{return args.count(k)>0;}
 String arg(const char* k){return args.count(k)?args.at(k):String();}
}server;
bool paired=true,runtimeReadable=true,settingsReadable=true;
bool authorized(){return paired;}
int httpCode; String sent;
void jsonResponse(int code,const String& json){httpCode=code;sent=json;}
void errorResponse(int code,const char* reason){httpCode=code;sent=reason;}
namespace panel_heating_settings{
State stored;
bool snapshot(State& out){if(!settingsReadable)return false;out=stored;return true;}
Result save(uint32_t revision,const State& s){
 if(stored.revision!=revision)return Result::Conflict;
 stored=s;stored.revision=revision+1;return Result::Saved;
}
}
namespace panel_heating_runtime{
Snapshot fixture;
bool snapshot(Snapshot& out){if(!runtimeReadable)return false;out=fixture;return true;}
}
DIGITS
QUOTED
HANDLERS
int main(){
 using namespace panel_heating_settings;
 auto& fixture=panel_heating_runtime::fixture;
 paired=false;heatingRequest();assert(httpCode==401);
 paired=true;settingsReadable=false;heatingRequest();assert(httpCode==200);std::cout<<sent<<"\n";
 settingsReadable=true;runtimeReadable=false;heatingRequest();assert(httpCode==200);std::cout<<sent<<"\n";
 server.args={{"revision","0"},{"season","2"},{"reheat","2"},{"reheat_step_s","300"},
              {"away_diesel","0"},{"rooms","1,2,0,0,0,0,0,0,1,0,0,0,0,14"}};
 for(const auto& p:heatingFields)server.args[p.key]="";
 server.args["room_on_c"]="19";server.args["room_radiator_off_c"]="22";server.args["room_off_c"]="23";
 server.args["supply_target_c"]="60.5";server.args["return_target_c"]="48";
 auto valid=server.args;
 for(const auto& p: {std::pair<const char*,const char*>{"rooms","1,2"},{"rooms","0,0,0,0,0,0,0,0,0,0,0,0,0,15"},
     {"rooms","0,0,0,0,0,0,0,0,0,0,0,0,0,0,"},{"room_on_c","nan"},{"room_on_c","1e3"},
     {"room_on_c","12.34"},{"room_on_c"," 19"},{"away_diesel","yes"},{"reheat","3"},{"season","25"},
     {"revision","4294967296"},{"revision","-1"},{"reheat_step_s","9999999"}}){
   server.args=valid;server.args[p.first]=p.second;saveHeatingSettings();assert(httpCode==400&&stored.revision==0);
 }
 server.args=valid;server.args.erase("tank_on_c");saveHeatingSettings();assert(httpCode==400&&stored.revision==0);
 server.args=valid;saveHeatingSettings();assert(httpCode==200&&stored.revision==1);
 assert(stored.config.roomForLoop[13]==14&&stored.config.supplyTargetC==60.5f&&stored.config.reheatStepMs==300000);
 saveHeatingSettings();assert(httpCode==409&&stored.revision==1);
 runtimeReadable=true;fixture.settingsAvailable=true;fixture.settingsRevision=0;
 heatingRequest();assert(httpCode==200);std::cout<<sent<<"\n";
 fixture.settingsRevision=1;fixture.input.roomC[0]=18.5f;fixture.input.roomC[13]=NAN;
 fixture.output.stages=2;
 heatingRequest();assert(httpCode==200);std::cout<<sent<<"\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp)
    cpp = p / "test.cpp"
    cpp.write_text(driver.replace("DIGITS", digits).replace("QUOTED", quoted).replace("HANDLERS", handlers))
    exe = p / "test"
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-I", str(root / "src"),
                    str(cpp), "-o", str(exe)], check=True)
    answers = [json.loads(row) for row in subprocess.check_output([str(exe)], text=True).splitlines()]
assert answers[0] == {"settings": None, "calculation": None}
assert answers[1]["settings"]["revision"] == 0 and answers[1]["calculation"] is None
assert answers[2]["settings"]["revision"] == 1 and answers[2]["calculation"] is None
assert answers[3]["settings"]["rooms"][13] == 14 and answers[3]["settings"]["tank_on_c"] is None
assert answers[3]["calculation"]["stages"] == 2
assert answers[3]["calculation"]["room_c"][0] == 18.5 and answers[3]["calculation"]["room_c"][13] is None
print("PASS: authenticated heating response, CAS POST parser, malformed and partial edits, nullable telemetry and coherent revision")
