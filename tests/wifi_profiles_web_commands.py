"""Compile and run the actual Wi-Fi HTTP parsing, queue handoff and status writer.
Auth/Origin/OTA guards (shared by all commands) run in update_web_commands.py.
"""
import json
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
s = (root / "src/panel_web.cpp").read_text()
cmd = s.index("  panel::Command cmd;", s.index("void commandRequest()"))
wifi = s[cmd:s.index('  else if(kind=="zone")', cmd)]
end = s.index("  RadioState radio;", cmd)
wifi += s[end:s.index("void logoutRequest()", end)]
start = s.index(r'  j+=",\"wifi\":')
status = s[start:s.index(r'  j+=",\"ble\":', start)]
quote = s[s.index("void jsonQuote("):s.index("const char* fuelName(")]
host = r'''
#include "panel_types.h"
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <type_traits>
using namespace panel;
class String:public std::string{
public: using std::string::string;
 String()=default; String(const std::string& s):std::string(s){}
 template<class T, typename std::enable_if<std::is_arithmetic<T>::value,int>::type=0>
 String(T v):std::string(std::to_string(v)){}
 bool isEmpty()const{return empty();}
};
struct {
 std::map<std::string,String> fields;
 bool hasArg(const char* k){return fields.count(k);}
 String arg(const char* k){return hasArg(k)?fields.at(k):String{};}
}server;
RadioState actual; Command seen; unsigned submits=0; int code;
namespace panel_radio{
 bool snapshot(RadioState& r){r=actual;return true;}
 bool submit(const Command& c){++submits;seen=c;return true;}
}
void errorResponse(int c,const char*){code=c;}
void jsonResponse(int c,const char*){code=c;}
QUOTE
void command(const String& kind){
WIFI
void printStatus(bool admin){
 const auto& radio=actual;String j="{\"ok\":true";STATUS
 j+='}';std::cout<<j<<"\n";
}
int main(){
 actual.ready=true;actual.savedCount=2;strcpy(actual.savedNetworks[0].ssid,"Test");
 strcpy(actual.savedNetworks[1].ssid,"Absent");actual.savedNetworks[1].open=true;
 actual.apCount=1;strcpy(actual.aps[0].ssid,"Test");actual.aps[0].saved=true;
 strcpy(actual.ssid,"Test");printStatus(false);printStatus(true);
 auto call=[](const char* action,int c){
  const unsigned before=submits;code=0;command(action);assert(code==c);
  assert(submits==before+unsigned(c==202));
 };
 server.fields={{"ssid","Test"},{"open","0"},{"saved","1"}};
 call("wifi_connect",202);assert(seen.useSaved&&!seen.open&&!seen.password[0]&&!strcmp(seen.ssid,"Test"));
 server.fields["password"]="";call("wifi_connect",400);
 server.fields.erase("password");server.fields["open"]="1";call("wifi_connect",409);
 server.fields["ssid"]="Missing";call("wifi_connect",409);
 server.fields["ssid"]=std::string("Test\0suffix",11);call("wifi_connect",400);
 server.fields={{"ssid","New"},{"open","0"},{"password","fixture-only"}};
 call("wifi_connect",202);assert(!seen.useSaved&&!strcmp(seen.password,"fixture-only"));
 server.fields["password"]="";call("wifi_connect",400);
 server.fields={{"ssid","Absent"},{"open","1"}};call("wifi_forget",202);
 assert(seen.job==Job::Forget&&seen.open&&!strcmp(seen.ssid,"Absent"));
 server.fields.clear();call("wifi_forget",409);actual.savedCount=1;call("wifi_forget",202);
 assert(!seen.open&&!strcmp(seen.ssid,"Test"));
}
'''
for marker, piece in (("QUOTE", quote), ("WIFI", wifi), ("STATUS", status)):
    host = host.replace(marker, piece)
with tempfile.TemporaryDirectory() as tmp:
    driver = Path(tmp) / "wifi.cpp"
    binary = Path(tmp) / "wifi"
    driver.write_text(host)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-I", str(root / "src"),
                    str(driver), "-o", str(binary)], check=True)
    guest, admin = [json.loads(line)["wifi"] for line in subprocess.check_output([binary], text=True).splitlines()]
assert guest["ssid"] == "" and guest["aps"] == guest["savedNetworks"] == []
assert admin["aps"][0]["saved"] and admin["savedNetworks"] == [
    {"ssid": "Test", "open": False}, {"ssid": "Absent", "open": True}]
print("PASS: actual Wi-Fi JSON privacy, strict saved login, scoped and legacy Forget")
