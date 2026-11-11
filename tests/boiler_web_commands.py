"""Exercise the real boiler settings HTTP parser, guards, status and replies."""
from pathlib import Path
import json
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
s = (root / "src/panel_web.cpp").read_text()
p = s.index("void commandRequest() {")
guard = s[p:s.index('  if(kind=="update_check"', p)]
p = s.index("// Read/status and pairing", p)
guard += s[s.index("  if(update.busy||update.bootPending)", p):s.index('  if(kind=="ha_discover")', p)]
p = s.index('  if(kind=="boiler_thresholds")', p)
guard += s[p:s.index('  if(kind=="sd_format_prepare")', p)]
p = s.index("  panel_boiler::Settings boiler;", s.index("void statusRequest()"))
status = s[p:s.index("  panel_storage::Snapshot storage;", p)]
digits = s[s.index("bool allDigits("):s.index("#if BP_WEB_ENABLED", s.index("bool allDigits("))]
driver = r'''
#include "panel_boiler.h"
#include <string>
#include <iostream>
#include <cassert>
#include <map>
#include <cstdlib>
#include <type_traits>
class String:public std::string{public:
 using std::string::string;String()=default;String(const std::string& v):std::string(v){}
 template<class T,typename std::enable_if<std::is_arithmetic<T>::value,int>::type=0>
 String(T v):std::string(std::to_string(v)){}
};
struct {std::map<std::string,String> args;String arg(const char* name){return args[name];}}server;
bool auth=true,origin=true;
bool authorized(){return auth;}bool pairingOriginAllowed(){return origin;}
int code;String reply;
void errorResponse(int c,const char* reason){code=c;reply=reason;}
void jsonResponse(int c,const String& value){code=c;reply=value;}
namespace panel_update{struct Snapshot{bool started=true,busy=false,bootPending=false;};Snapshot fixture;
 bool ready=true;bool snapshot(Snapshot& v){v=fixture;return ready;}}
namespace panel_boiler{
 Settings fixture;bool readable=true;unsigned calls=0;Result next=Result::Saved;
 bool snapshot(Settings& b){b=fixture;return readable;}
 Result save(uint32_t expected,int on,int off){
  ++calls;assert(on==10&&off==50);if(expected!=fixture.revision)return Result::Conflict;
  if(next==Result::Saved){fixture.configured=true;fixture.onC=on;fixture.offC=off;++fixture.revision;}return next;
 }
}
DIGITS
GUARD
 errorResponse(404,"unknown_action");}
void printStatus(){String j="{\"ok\":true";STATUS;std::cout<<j<<"}\n";}
int main(){
 using namespace panel_boiler;
 printStatus();readable=false;printStatus();readable=true;
 server.args={{"action","boiler_thresholds"},{"on_c","10"},{"off_c","50"},{"revision","0"}};
 auto check=[](int expected,unsigned count){code=0;commandRequest();assert(code==expected&&calls==count);};
 auth=false;check(401,0);auth=true;origin=false;check(403,0);origin=true;
 panel_update::ready=false;check(503,0);panel_update::ready=true;
 panel_update::fixture.started=false;check(503,0);panel_update::fixture.started=true;
 panel_update::fixture.busy=true;check(409,0);panel_update::fixture.busy=false;
 panel_update::fixture.bootPending=true;check(409,0);panel_update::fixture.bootPending=false;
 server.args["action"]="boiler_enable";check(404,0);server.args["action"]="boiler_thresholds";
 for(const char* bad:{"","-1","NaN","3.5","100","50","51"}){server.args["on_c"]=bad;check(400,0);}
 server.args["on_c"]=String("1\0",2);check(400,0);server.args["on_c"]="10";
 for(const char* bad:{"","-1","4294967296","1.0"}){server.args["revision"]=bad;check(400,0);}
 server.args["revision"]="17";check(409,1);server.args["revision"]="0";next=Result::Unavailable;check(503,2);
 next=Result::Saved;check(200,3);std::cout<<reply<<"\n";printStatus();
 server.args["revision"]="1";readable=false;check(409,4);assert(reply=="boiler_save_unconfirmed");
}
'''
for marker, code in (("DIGITS",digits),("GUARD",guard),("STATUS",status)):
    driver = driver.replace(marker, code)
with tempfile.TemporaryDirectory() as tmp:
    cpp, exe = Path(tmp)/"boiler.cpp", Path(tmp)/"boiler"
    cpp.write_text(driver)
    subprocess.run(["g++","-std=c++17","-Wall","-Wextra","-Werror","-I",str(root/"src"),str(cpp),"-o",str(exe)],check=True)
    empty, unknown, saved, configured = map(json.loads, subprocess.check_output([exe],text=True).splitlines())
assert empty["boiler_settings"] == dict(revision=0,configured=False,on_c=None,off_c=None)
assert unknown["boiler_settings"] is None
assert saved == dict(ok=True,saved=True,revision=1)
assert configured["boiler_settings"] == dict(revision=1,configured=True,on_c=10,off_c=50)
print("PASS: boiler settings status, guarded CAS write, rejected invalid/stale fields, no enable route")
