#include "panel_pairing.h"
#include <cassert>
#include <cstdio>
#include <climits>
using namespace panel_pairing;
int main(){
 const char* a="0123456789abcdef0123456789abcdef";
 const char* b="fedcba9876543210fedcba9876543210";
 State s;char pin[7];uint32_t left=0,rev=0;
 assert(!s.display(pin,left,rev,0)&&!pin[0]&&left==0);
 assert(s.request("bad","123456",0)==Request::Invalid);
 assert(s.request(a,"12x456",0)==Request::Invalid);
 assert(s.request(a,"123456",0)==Request::Started);
 assert(s.display(pin,left,rev,0)&&!strcmp(pin,"123456")&&left==120&&rev==1);
 assert(s.request(a,"654321",5000)==Request::Existing);
 assert(s.display(pin,left,rev,5000)&&!strcmp(pin,"123456")&&left==115);
 assert(s.request(b,"654321",5000)==Request::Busy);
 assert(!s.cancel(b,5000));
 for(int i=0;i<20;i++)assert(s.verify(b,"123456",5000)==Verify::Rejected);
 assert(s.active(5000));
 assert(s.verify(a,"000000",5000)==Verify::Rejected&&s.active(5000));
 assert(s.verify(a,"123456",5000)==Verify::Accepted);
 assert(!s.display(pin,left,rev,5001)&&!pin[0]);
 assert(s.verify(a,"123456",5001)==Verify::Rejected);
 assert(s.request(a,"333333",29999)==Request::Cooldown);
 assert(s.request(a,"333333",30000)==Request::Started);
 assert(s.cancel(a,30001)&&!s.active(30001));
 assert(!s.cancel(a,30002));
 assert(s.request(a,"444444",60000)==Request::Started);
 for(int i=0;i<4;i++)assert(s.verify(a,"000000",60001+i)==Verify::Rejected);
 assert(s.verify(a,"000000",60005)==Verify::Locked);
 assert(!s.active(60005)&&s.retryAfter(60005)==120);
 assert(s.request(a,"555555",180004)==Request::Cooldown);
 assert(s.request(a,"555555",180005)==Request::Started);
 assert(s.active(300004));assert(!s.active(300005));
 assert(s.request(b,"777777",300006)==Request::Started);
 s.revoke();assert(!s.display(pin,left,rev,300007));
 State wrap;const uint32_t start=UINT32_MAX-1000;
 assert(wrap.request(a,"123456",start)==Request::Started);
 assert(wrap.display(pin,left,rev,500)&&left==119);
 assert(wrap.active(start+119999));assert(!wrap.active(start+120000));
 assert(wrap.request(a,"123456",start+120001)==Request::Started);
 assert(wrap.cancel(a,start+120002));
 assert(!localRequest("","","http://192.0.2.1","http://192.0.2.1:80",""));
 assert(localRequest("1","http://192.0.2.1","http://192.0.2.1","http://192.0.2.1:80","same-origin"));
 assert(localRequest("1","http://192.0.2.1:80","http://192.0.2.1","http://192.0.2.1:80",""));
 assert(!localRequest("1","https://evil.invalid","http://192.0.2.1","http://192.0.2.1:80",""));
 assert(!localRequest("1","null","http://192.0.2.1","http://192.0.2.1:80",""));
 assert(!localRequest("1","http://192.0.2.1","http://192.0.2.1","http://192.0.2.1:80","cross-site"));
 assert(!ticket32("0000000000000000000000000000000Z")&&!digits6("1234567"));
 puts("PASS: pairing issue/reuse/busy/cancel/success/expiry, five-attempt lockout, cooldown, rollover and request origin policy");
}
