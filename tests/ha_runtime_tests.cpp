// Compiles the actual MQTT/NVS adapter against explicitly isolated host fakes.
#include "../src/panel_ha.cpp"
#include <cassert>
#include <iostream>
namespace panel_radio {bool snapshot(panel::RadioState& s){s.connected=true;s.rssi=-60;return true;}}
namespace panel_storage {bool snapshot(Snapshot& s){s.mounted=true;s.freeBytes=1234;return true;}}
using namespace panel_ha;
int main(){
 assert(begin());Snapshot s;assert(snapshot(s)&&s.ready&&!s.enabled&&!s.connected);assert(writes==0&&creates==0);assert(!requestDiscovery());
 Config c;c.enabled=1;strcpy(c.host,"192.0.2.1");strcpy(c.username,"test-account");strcpy(c.password,"test-secret-NOT-REAL");
 assert(configure(c,PasswordMode::Set,0)==SaveResult::Saved);assert(writes==1);
 assert(snapshot(s)&&s.passwordSet&&s.enabled&&s.revision==1);assert(!strstr(s.message,c.password));
 assert(configure(c,PasswordMode::Set,0)==SaveResult::Conflict);
 assert(configure(c,PasswordMode::Set,1)==SaveResult::Saved&&writes==1);
 Config bad=c;strcpy(bad.host,"mqtt://wrong");assert(configure(bad,PasswordMode::Set,1)==SaveResult::Invalid&&writes==1);
 strcpy(bad.host,"192.0.2.2");bad.password[0]=0;assert(configure(bad,PasswordMode::Keep,1)==SaveResult::Invalid);
 writeOK=false;assert(configure(bad,PasswordMode::Clear,1)==SaveResult::StorageError);assert(snapshot(s)&&s.revision==1&&!strcmp(s.host,c.host));writeOK=true;
 Persist old;assert(readSlot(activeSlot,old)&&sameConfig(old.config,c));wipe(&old,sizeof(old));
 assert(configure(bad,PasswordMode::Clear,1)==SaveResult::Saved);assert(snapshot(s)&&s.revision==2&&!s.passwordSet);
 assert(readSlot(activeSlot^1U,old)&&sameConfig(old.config,c));wipe(&old,sizeof(old));
 nvs[activeSlot?"bp_ha_b":"bp_ha_a"][0]^=1;Persist corrupt;assert(!readSlot(activeSlot,corrupt));wipe(&corrupt,sizeof(corrupt));
 assert(startClient(c));assert(mqttOptions.network.disable_auto_reconnect&&mqttOptions.outbox.limit<=32768);
 assert(mqttOptions.session.last_will.qos==1&&mqttOptions.session.last_will.retain==1&&!strcmp(mqttOptions.session.last_will.msg,"offline"));
 assert(!strcmp(mqttOptions.credentials.authentication.password,c.password));
 Telemetry t;panel::RadioState radio;radio.connected=true;assert(!sendState(t,radio));
 mqttEvent(nullptr,nullptr,MQTT_EVENT_CONNECTED,nullptr);assert(connected.load()&&announce.load()&&subscribeBirth.load());
 announce.store(false);char message[]="online";char other[]="untrusted/set";esp_mqtt_event_t event;
 event.data=message;event.data_len=event.total_data_len=6;event.topic=other;event.topic_len=int(strlen(other));
 mqttEvent(nullptr,nullptr,MQTT_EVENT_DATA,&event);assert(!announce.load());
 event.topic=birthTopic;event.topic_len=int(strlen(birthTopic));event.current_data_offset=1;
 mqttEvent(nullptr,nullptr,MQTT_EVENT_DATA,&event);assert(!announce.load());
 event.current_data_offset=0;mqttEvent(nullptr,nullptr,MQTT_EVENT_DATA,&event);assert(announce.load());
 assert(sendState(t,radio));assert(sent.back().qos==0&&sent.back().retain==0&&!sent.back().queued);assert(!strstr(sent.back().payload.c_str(),c.password));
 panel::Measurement values[4];panel::FuelLevel fuel;values[0].put(23.5f,1000);publish(values,fuel,1000);
 Telemetry captured;assert(xQueueReceive(readings,&captured,0)==pdTRUE&&captured.input[0].valid&&!captured.input[1].valid);
 sent.clear();stopClient(c,true);assert(!client&&!connected.load()&&destroys==1);
 assert(sent.size()==ENTITY_COUNT+1&&sent[0].payload=="offline");
 for(unsigned i=1;i<sent.size();i++)assert(sent[i].payload.empty()&&sent[i].retain==1&&sent[i].topic.find("homeassistant/")==0);
 vSemaphoreDelete(guard);guard=nullptr;vQueueDelete(readings);readings=nullptr;task=nullptr;
 assert(begin());assert(snapshot(s)&&s.revision==1&&s.passwordSet&&!strcmp(s.host,c.host)); // corrupt newest slot rejected
 std::cout<<"PASS: actual adapter/NVS with host fakes; disabled boot, save/readback/fallback, no secret forwarding, exact birth-only callback, no retained readings, cleanup\n";
}
