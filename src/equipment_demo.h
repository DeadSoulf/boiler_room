#pragma once
// Local visualization only. No transport, GPIO, sensor readings or actuator state.
// A confirmed mode is a simulated permission; physical operation remains unknown.
#include <stdint.h>

namespace panel { namespace equipment_demo {
enum class Id : uint8_t { Dhw, Electric, Circuit1, Circuit2, Circuit3, Diesel, FuelHeat, Count };
enum class Mode : uint8_t { Off, Enabled, Unknown };
enum class Status : uint8_t { None, Pending, Accepted, Rejected, Timeout };
enum class Outcome : uint8_t { Accepted, Rejected, Timeout };
constexpr unsigned COUNT = static_cast<unsigned>(Id::Count);
constexpr unsigned JOURNAL_SIZE = 16;
constexpr uint32_t RESPONSE_MS = 1000;

struct Command {
  uint64_t token=0;
  bool requested=false; // true: enable, false: disable.
  Status status=Status::None;
  uint32_t requestedAt=0,resolvedAt=0;
  Outcome outcome=Outcome::Accepted; // Captured when requested, not when resolved.
};
struct Node {
  Mode mode=Mode::Off;
  Command command;
};
struct Event {
  Id device=Id::Dhw;
  Status status=Status::None;
  bool requested=false;
  Mode mode=Mode::Off;
  uint64_t token=0;
  uint32_t at=0;
};

class State {
 public:
  Node devices[COUNT]={};
  Outcome nextOutcome=Outcome::Accepted;
  uint32_t revision=0;
  unsigned eventCount=0;

  const Node* device(Id id) const {
    const unsigned i=static_cast<unsigned>(id);
    return i<COUNT?&devices[i]:nullptr;
  }
  const Event* event(unsigned newestIndex) const {
    if(newestIndex>=eventCount)return nullptr;
    return &events_[(nextEvent_+JOURNAL_SIZE-1-newestIndex)%JOURNAL_SIZE];
  }
  bool canRequest(Id id,bool enabled) const {
    const Node* d=device(id);
    if(!d || !validOutcome(nextOutcome))return false;
    if(d->command.status==Status::Pending)return d->command.requested && !enabled;
    if(d->mode==Mode::Unknown)return !enabled;
    return enabled?d->mode==Mode::Off:d->mode==Mode::Enabled;
  }
  bool request(Id id,bool enabled,uint32_t now) {
    if(!canRequest(id,enabled) || !validTime(now) || nextToken_==UINT64_MAX)return false;
    Node& d=devices[static_cast<unsigned>(id)];
    // An OFF replacing an unconfirmed ON cannot claim that ON never took effect.
    if(d.command.status==Status::Pending)d.mode=Mode::Unknown;
    d.command={++nextToken_,enabled,Status::Pending,now,0,nextOutcome};
    rememberTime(now);++revision;append(id,now);
    return true;
  }
  bool resolve(Id id,uint64_t token,Outcome outcome,uint32_t now) {
    const unsigned i=static_cast<unsigned>(id);
    if(i>=COUNT || !validOutcome(outcome) || !validTime(now))return false;
    Node& d=devices[i];
    if(d.command.status!=Status::Pending || !token || d.command.token!=token
       || !forward(now,d.command.requestedAt))return false;
    if(outcome==Outcome::Accepted)d.mode=d.command.requested?Mode::Enabled:Mode::Off;
    else if(outcome==Outcome::Timeout)d.mode=Mode::Unknown;
    // Rejection preserves the last known mode, including Unknown after replacement.
    d.command.status=outcome==Outcome::Accepted?Status::Accepted:
                     outcome==Outcome::Rejected?Status::Rejected:Status::Timeout;
    d.command.resolvedAt=now;
    rememberTime(now);++revision;append(id,now);
    return true;
  }
  bool tick(uint32_t now) {
    if(!validTime(now))return false;
    rememberTime(now);
    bool changed=false;
    for(unsigned i=0;i<COUNT;i++){
      const Command& command=devices[i].command;
      if(command.status==Status::Pending && forward(now,command.requestedAt)
         && uint32_t(now-command.requestedAt)>=RESPONSE_MS){
        changed=resolve(static_cast<Id>(i),command.token,command.outcome,now) || changed;
      }
    }
    return changed;
  }
  void reset() {
    for(auto& d:devices)d=Node{};
    for(auto& e:events_)e=Event{};
    nextOutcome=Outcome::Accepted;eventCount=nextEvent_=0;++revision;
    // Keep monotonic tokens and clock across a UI reset. Old replies stay invalid.
  }

 private:
  Event events_[JOURNAL_SIZE]={};
  unsigned nextEvent_=0;
  uint64_t nextToken_=0;
  uint32_t lastNow_=0;
  bool clockSeen_=false;
  static bool validOutcome(Outcome outcome) {
    return outcome==Outcome::Accepted || outcome==Outcome::Rejected || outcome==Outcome::Timeout;
  }
  // millis() may wrap. Callers must poll within 2^31 ms (~24.8 days).
  static bool forward(uint32_t now,uint32_t previous) {
    return uint32_t(now-previous)<=0x7fffffffU;
  }
  bool validTime(uint32_t now) const {return !clockSeen_ || forward(now,lastNow_);}
  void rememberTime(uint32_t now) {lastNow_=now;clockSeen_=true;}
  void append(Id id,uint32_t now) {
    const Node& d=devices[static_cast<unsigned>(id)];
    events_[nextEvent_]={id,d.command.status,d.command.requested,d.mode,d.command.token,now};
    nextEvent_=(nextEvent_+1)%JOURNAL_SIZE;
    if(eventCount<JOURNAL_SIZE)++eventCount;
  }
};
} } // namespace panel::equipment_demo
