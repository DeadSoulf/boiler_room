#include "equipment_demo.h"
#include <stdio.h>
#include <stdlib.h>
#include <type_traits>
using namespace panel::equipment_demo;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)

static void defaultsAndRoundTrip(){
  State s;
  CHECK(s.eventCount==0 && !s.event(0));
  for(unsigned i=0;i<COUNT;i++){
    const Id id=static_cast<Id>(i);
    CHECK(s.device(id) && s.device(id)->mode==Mode::Off);
    CHECK(s.device(id)->command.status==Status::None);
    CHECK(s.canRequest(id,true) && !s.canRequest(id,false));
  }
  CHECK(s.request(Id::Dhw,true,100));
  const auto first=s.device(Id::Dhw)->command.token;
  CHECK(first!=0 && s.device(Id::Dhw)->command.status==Status::Pending);
  CHECK(s.device(Id::Dhw)->mode==Mode::Off);
  CHECK(!s.canRequest(Id::Dhw,true) && s.canRequest(Id::Dhw,false));
  const auto revision=s.revision;
  CHECK(!s.request(Id::Dhw,true,200) && s.revision==revision);
  CHECK(!s.tick(1099) && s.device(Id::Dhw)->mode==Mode::Off);
  CHECK(s.tick(1100));
  CHECK(s.device(Id::Dhw)->mode==Mode::Enabled);
  CHECK(s.device(Id::Dhw)->command.status==Status::Accepted);
  CHECK(s.device(Id::Dhw)->command.resolvedAt==1100);
  CHECK(!s.tick(1101) && s.eventCount==2);
  CHECK(!s.resolve(Id::Dhw,first,Outcome::Rejected,1102));
  CHECK(s.request(Id::Dhw,false,1200));
  CHECK(s.device(Id::Dhw)->mode==Mode::Enabled);
  CHECK(!s.canRequest(Id::Dhw,true) && !s.canRequest(Id::Dhw,false));
  CHECK(s.tick(2200) && s.device(Id::Dhw)->mode==Mode::Off);
  CHECK(s.eventCount==4);
}

static void outcomesAndIsolation(){
  const Outcome outcomes[]={Outcome::Accepted,Outcome::Rejected,Outcome::Timeout};
  for(unsigned i=0;i<COUNT;i++)for(Outcome outcome:outcomes){
    State s;const Id id=static_cast<Id>(i);s.nextOutcome=outcome;
    CHECK(s.request(id,true,5));
    // UI changing its next-response selection cannot change an in-flight reply.
    s.nextOutcome=outcome==Outcome::Accepted?Outcome::Rejected:Outcome::Accepted;
    CHECK(s.tick(1005));
    const auto* d=s.device(id);
    CHECK(d->mode==(outcome==Outcome::Accepted?Mode::Enabled:outcome==Outcome::Rejected?Mode::Off:Mode::Unknown));
    CHECK(d->command.status==(outcome==Outcome::Accepted?Status::Accepted:outcome==Outcome::Rejected?Status::Rejected:Status::Timeout));
    CHECK(d->command.outcome==outcome);
    for(unsigned j=0;j<COUNT;j++)if(j!=i){
      CHECK(s.devices[j].mode==Mode::Off && s.devices[j].command.status==Status::None);
    }
  }
  State s;
  CHECK(s.request(Id::Diesel,true,100));CHECK(s.tick(1100));
  s.nextOutcome=Outcome::Rejected;
  CHECK(s.request(Id::Diesel,false,1200));CHECK(s.tick(2200));
  CHECK(s.device(Id::Diesel)->mode==Mode::Enabled);
  CHECK(s.device(Id::Diesel)->command.status==Status::Rejected);
  s.nextOutcome=Outcome::Timeout;
  CHECK(s.request(Id::Diesel,false,2300));CHECK(s.tick(3300));
  CHECK(s.device(Id::Diesel)->mode==Mode::Unknown);
  CHECK(!s.canRequest(Id::Diesel,true) && s.canRequest(Id::Diesel,false));
  CHECK(!s.request(Id::Diesel,true,3400));
  CHECK(s.request(Id::Diesel,false,3400));CHECK(s.tick(4400));
  CHECK(s.device(Id::Diesel)->mode==Mode::Unknown);
  s.nextOutcome=Outcome::Accepted;
  CHECK(s.request(Id::Diesel,false,4500));CHECK(s.tick(5500));
  CHECK(s.device(Id::Diesel)->mode==Mode::Off);
}

static void parallelRequests(){
  State s;uint64_t tokens[COUNT];
  for(unsigned i=0;i<COUNT;i++){
    s.nextOutcome=static_cast<Outcome>(i%3);
    CHECK(s.request(static_cast<Id>(i),true,100));
    tokens[i]=s.devices[i].command.token;
    for(unsigned j=0;j<i;j++)CHECK(tokens[j]!=tokens[i]);
  }
  CHECK(!s.resolve(Id::Electric,tokens[0],Outcome::Accepted,150));
  CHECK(s.tick(1100));
  for(unsigned i=0;i<COUNT;i++){
    CHECK(s.devices[i].mode==(i%3==0?Mode::Enabled:i%3==1?Mode::Off:Mode::Unknown));
  }
  CHECK(s.eventCount==COUNT*2);
}

static void replacementAndReset(){
  State s;CHECK(s.request(Id::FuelHeat,true,100));
  const auto old=s.device(Id::FuelHeat)->command.token;
  s.nextOutcome=Outcome::Rejected;
  CHECK(s.request(Id::FuelHeat,false,200));
  const auto replacement=s.device(Id::FuelHeat)->command.token;
  CHECK(replacement!=old && s.device(Id::FuelHeat)->mode==Mode::Unknown);
  CHECK(!s.resolve(Id::FuelHeat,old,Outcome::Accepted,300));
  CHECK(!s.tick(1100)); // Replacement has its own deadline.
  CHECK(s.tick(1200));
  CHECK(s.device(Id::FuelHeat)->mode==Mode::Unknown);
  CHECK(s.device(Id::FuelHeat)->command.status==Status::Rejected);
  CHECK(!s.resolve(Id::FuelHeat,replacement,Outcome::Accepted,1201));
  s.nextOutcome=Outcome::Accepted;
  CHECK(s.request(Id::FuelHeat,false,1300));CHECK(s.tick(2300));
  CHECK(s.device(Id::FuelHeat)->mode==Mode::Off);
  CHECK(s.request(Id::Circuit2,true,2400));
  const auto beforeReset=s.device(Id::Circuit2)->command.token;
  s.reset();CHECK(s.eventCount==0 && !s.event(0));
  for(const auto& d:s.devices)CHECK(d.mode==Mode::Off && d.command.status==Status::None);
  CHECK(s.request(Id::Circuit2,true,2500));
  CHECK(s.device(Id::Circuit2)->command.token!=beforeReset);
  CHECK(!s.resolve(Id::Circuit2,beforeReset,Outcome::Accepted,2600));
  CHECK(!s.tick(3499));CHECK(s.tick(3500));
  CHECK(s.device(Id::Circuit2)->mode==Mode::Enabled);
}

static void timeAndInvalidInput(){
  State wrap;const uint32_t start=UINT32_MAX-499U;
  CHECK(wrap.request(Id::Electric,true,start));
  CHECK(!wrap.tick(uint32_t(start+999U)));
  CHECK(wrap.tick(uint32_t(start+1000U)));
  CHECK(wrap.device(Id::Electric)->mode==Mode::Enabled);
  CHECK(wrap.device(Id::Electric)->command.resolvedAt==500U);
  State s;CHECK(s.request(Id::Dhw,true,100));
  const auto token=s.device(Id::Dhw)->command.token;
  CHECK(!s.tick(99));
  CHECK(!s.resolve(Id::Dhw,token,Outcome::Accepted,99));
  CHECK(!s.request(Id::Diesel,true,99));
  CHECK(s.device(Id::Dhw)->mode==Mode::Off && s.eventCount==1);
  const Id invalid=static_cast<Id>(255);
  CHECK(!s.device(invalid) && !s.device(Id::Count));
  CHECK(!s.canRequest(invalid,true) && !s.request(invalid,true,100));
  CHECK(!s.resolve(invalid,token,Outcome::Accepted,100));
  CHECK(!s.resolve(Id::Dhw,0,Outcome::Accepted,100));
  CHECK(!s.resolve(Id::Dhw,token,static_cast<Outcome>(255),100));
  s.nextOutcome=static_cast<Outcome>(255);
  CHECK(!s.canRequest(Id::Diesel,true) && !s.request(Id::Diesel,true,100));
  // A malformed future-selection value does not corrupt an already captured reply.
  CHECK(s.tick(1100) && s.device(Id::Dhw)->mode==Mode::Enabled);
}

static void boundedJournal(){
  State s;uint32_t now=0;
  for(unsigned i=0;i<30;i++){
    CHECK(s.request(Id::Circuit1,true,now));now+=1000;CHECK(s.tick(now));
    CHECK(s.request(Id::Circuit1,false,now));now+=1000;CHECK(s.tick(now));
  }
  CHECK(s.eventCount==JOURNAL_SIZE && !s.event(JOURNAL_SIZE));
  CHECK(s.event(0)->at==now && s.event(0)->status==Status::Accepted && !s.event(0)->requested);
  CHECK(s.event(1)->status==Status::Pending && !s.event(1)->requested);
  CHECK(s.event(0)->token==s.event(1)->token);
  for(unsigned i=0;i<JOURNAL_SIZE;i++){
    CHECK(s.event(i)->device==Id::Circuit1);
    CHECK(s.event(i)->status==(i%2?Status::Pending:Status::Accepted));
    if(i)CHECK(s.event(i-1)->at>=s.event(i)->at);
  }
}

int main(){
  static_assert(std::is_trivially_copyable<State>::value,"Demo state must be fixed storage");
  static_assert(sizeof(State)<2048,"Bounded demo storage must fit comfortably on the panel");
  defaultsAndRoundTrip();outcomesAndIsolation();parallelRequests();replacementAndReset();timeAndInvalidInput();boundedJournal();
  printf("PASS: %u assertions (7 local demo nodes, outcomes, parallel requests, late replies, reset, wraparound, journal); State=%zu bytes\n",checks,sizeof(State));
}
