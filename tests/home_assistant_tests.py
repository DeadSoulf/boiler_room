#!/usr/bin/env python3
"""MQTT HA protocol/runtime host checks plus integration guards. No hardware claim."""
from pathlib import Path
from html.parser import HTMLParser
import json,os,shutil,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
read=lambda p:(r/p).read_text(encoding='utf-8')
with tempfile.TemporaryDirectory() as td:
 compiler=shutil.which(os.environ.get('CXX','g++'))
 if not compiler:
  if os.environ.get('CI'):raise RuntimeError('Native C++ compiler required in CI')
  print('SKIP: host C++ unavailable locally; run this suite on Linux/GitHub CI')
 else:
  def run(name,includes):
   exe=Path(td)/name
   subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror','-pedantic',*[x for d in includes for x in ('-I',str(r/d))],str(r/('tests/'+name+'.cpp')),'-o',str(exe)],check=True)
   return subprocess.check_output([str(exe)],text=True,encoding='utf-8')
  out=run('ha_protocol_tests',['src'])
  configs=[];states={}
  for line in out.splitlines():
   if line.startswith('CONFIG '):
    _,topic,payload=line.split(' ',2);obj=json.loads(payload);configs.append(obj)
    assert topic.startswith('homeassistant/') and topic.endswith('/config')
    assert obj['availability_mode']=='all' and obj['expire_after']==15
    assert len(obj['availability'])==2 and 'command_topic' not in obj
   elif not line.startswith('PASS:'):
    name,payload=line.split(' ',1);states[name]=json.loads(payload)
  assert len(configs)==14 and len({c['unique_id'] for c in configs})==14
  assert all(c['device']['identifiers']==['bp_0123456789ab'] for c in configs)
  assert states['EMPTY']['room'] is None and not states['EMPTY']['room_valid']
  assert states['LIVE']['room']==-12.25 and states['LIVE']['fuel_lower']=='ON'
  assert states['LIVE']['coolant']==58.0 and states['LIVE']['coolant_return']==43.5
  assert states['LIVE']['fuel_temp']==28.5 and states['LIVE']['fuel_outlet']==37.0
  assert states['EMPTY']['fuel_outlet'] is None and not states['EMPTY']['fuel_outlet_valid']
  assert states['EMPTY']['coolant_return'] is None and not states['EMPTY']['coolant_return_valid']
  assert states['LIVE']['sd_free']==62602392064 and states['LIVE']['fuel_level']=='between'
  assert states['INCONSISTENT']['fuel_level']=='inconsistent'
  assert states['STALE']['room'] is None and not states['STALE']['fuel_level_valid']
  print(out.splitlines()[-1]);print(run('ha_runtime_tests',['tests/ha_stubs','src']).strip())
 web,main,page,cpp,hdr=[read(p) for p in ['src/panel_web.cpp','src/main.cpp','src/panel_web_page.h','src/panel_ha.cpp','src/panel_ha.h']]
 assert web.index('if(!authorized())')<web.index('if(kind=="ha_save")')
 assert web.index('if(!authorized())')<web.index('if(kind=="ha_discover")')
 assert 'jsonQuote(j,admin?ha.host:"")' in web and 'jsonQuote(j,admin?ha.username:"")' in web
 assert 'ha.password)' not in web and 'char password[' not in hdr
 assert 'expectedRevision!=revision' in cpp and 'mode==PasswordMode::Keep&&saved.password[0]' in cpp
 assert 'bp_ha_a' in cpp and 'bp_ha_b' in cpp and 'check.revision==nextRevision' in cpp
 assert 'xQueueCreate(1,sizeof(Telemetry))' in cpp and 'xQueueOverwrite(readings,&t)' in cpp
 assert 'panel_ha::publish(measurements,fuelLevel,millis())' in main
 assert 'panel_ha::begin()' in main
 assert 'esp_mqtt_client_subscribe_single(client,birthTopic,1)' in cpp
 assert 'esp_mqtt_client_publish(client,stateTopic,payload,0,0,0)' in cpp
 for text in (cpp,hdr):
  for forbidden in ('digitalWrite(', 'gpio_set_level(', 'panel_radio::submit(', 'panel_config::set(', 'erase-flash'):
   assert forbidden not in text,forbidden
 assert 'options.outbox.limit=24576' in cpp and 'disable_auto_reconnect=true' in cpp
 class IDs(HTMLParser):
  def __init__(self):super().__init__();self.ids=set()
  def handle_starttag(self,tag,attrs):
   for k,v in attrs:
    if k=='id':assert v not in self.ids,v;self.ids.add(v)
 parser=IDs();parser.feed(page)
 for name in ('ha','ha-form','ha-host','ha-port','ha-password','ha-prefix','ha-lan'):
  assert name in parser.ids,name
 script=page.split('<script>',1)[1].split('</script>',1)[0];path=Path(td)/'web.js';path.write_text(script,encoding='utf-8')
 subprocess.run(['node','--check',str(path)],check=True)
 assert 'addEventListener("submit",saveHA)' in script
 assert 'values.password=""' in script and 'haFormRevision' in script
subprocess.run(['node',str(r/'tests/ha_web_tests.mjs')],check=True)
print('PASS: pairing guards, secret redaction, config validation, bounded worker, HTML IDs/JavaScript; no actuator path')
