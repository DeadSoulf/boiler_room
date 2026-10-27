#include "panel_types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(){
 using namespace panel;unsigned checks=0;
 #define CHECK(x) do{assert(x);++checks;}while(0)
 CHECK(adjacent(0,-1)==3);CHECK(adjacent(3,1)==0);
 CHECK(passwordValid("",true));CHECK(!passwordValid("",false));CHECK(!passwordValid("1234567",false));CHECK(passwordValid("12345678",false));
 char hex[65];memset(hex,'a',64);hex[64]=0;CHECK(passwordValid(hex,false));hex[10]='z';CHECK(!passwordValid(hex,false));
 Measurement m;char b[24];temperature(b,sizeof(b),m,1);CHECK(!strcmp(b,"--.-"));
 m.put(-5.2f,100);temperature(b,sizeof(b),m,200);CHECK(!strcmp(b,"-5.2"));CHECK(m.fresh(29999));CHECK(!m.fresh(30100));
 m.put(12.5f,500);CHECK(m.minimum==-5.2f && m.maximum==12.5f);m.put(NAN,600);CHECK(!m.valid);
 m.put(1.0f,0xfffffff0U);CHECK(m.fresh(10));CHECK(!m.fresh(40000));
 m.put(-0.01f,5);temperature(b,sizeof(b),m,6);CHECK(!strcmp(b,"0.0"));
 Measurement history;CHECK(history.count==0 && history.freshness(0)==0);
 for(unsigned i=0;i<100;i++)history.put(float(i),i*1000U);
 CHECK(history.count==24 && history.at(0)==76 && history.at(23)==99);
 CHECK(history.timeAt(0)==76000 && history.delta()==1);
 CHECK(history.freshness(99000)==100 && history.freshness(114000)==50 && history.freshness(129000)==0);
 history.put(INFINITY,100000);CHECK(!history.valid && history.count==24);
 history.put(-10,101000);CHECK(history.minimum==-10 && history.maximum==99);
 history.put(20,0xfffffff0U);CHECK(history.freshness(10)==99);CHECK(!history.fresh(40000));
 setenv("TZ","UTC0",1);tzset();time_t t;
 CHECK(parseLocalTime("2026-10-27 18:40",t));CHECK(!parseLocalTime("2026-02-30 10:20",t));CHECK(!parseLocalTime("2026-10-27 24:00",t));
 CHECK(!parseLocalTime("2026-10-27 18:40x",t));CHECK(!parseLocalTime("1970-01-01 00:00",t));CHECK(parseLocalTime("2024-02-29 12:00",t));
 for(int initial=0;initial<4;initial++){int p=initial;for(int n=0;n<10000;n++){p=adjacent(p,n%2?1:-1);CHECK(p>=0 && p<4);}}
 printf("PASS: %u assertions (navigation, validity, stale readings, wraparound, password validation, date parsing)\n",checks);
}
