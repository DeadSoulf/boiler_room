"""Actual embedded HTML/JS in Chromium. HTTP fixtures only, never the panel."""
from pathlib import Path
from http.server import ThreadingHTTPServer,BaseHTTPRequestHandler
from threading import Thread
from urllib.parse import parse_qs
import copy,json,time,os,re,sys
from playwright.sync_api import sync_playwright,expect
r=Path(__file__).resolve().parents[1]
version=(r/'VERSION').read_text(encoding='ascii').strip()
source=(r/'src/panel_web_page.h').read_text(encoding='utf-8')
html=source[source.index('<!doctype html>'):source.index('</html>')+7].encode()
out=r/'artifacts/web-review';out.mkdir(parents=True,exist_ok=True)
fixture={
 'version':version,'clock':'04.11.2026 12:40:00','uptime_s':3600,'physical_control':False,'data_origin':'fixture',
 'wifi':{'connected':True,'ready':True,'busy':False,'supported':True,'has_saved':True,'ssid':'Fixture-WiFi','ip':'192.0.2.10','rssi':-53,'aps':[]},
 'ble':{'supported':True,'message':'','devices':[]},'screen':{'timeout_s':60,'touch_ready':True,'message':''},
 'time':{'zone':0,'zone_name':'UTC','message':''},'service':{'free_heap':180000,'first_frame_ms':480,'display_ms':320,'lvgl_ms':2,'touch_ms':80},
 'host_version':'2.9.6','c6_version':'2.9.6',
 'temperatures':[{'valid':True,'ever':True,'value':v,'minimum':v,'maximum':v,'trend':None,'age_s':0,'history':[]} for v in (22.4,-8.2,61.7,34.8,43.2,17.1)],
 'temperature_targets':[{'configured':False,'revision':0,'value_c':None} for _ in range(6)],
 'update':{'started':True,'supported':True,'busy':False,'boot_pending':False},
 'fuel':{'lower':{'valid':True,'reached':True,'age_s':0},'upper':{'valid':True,'reached':True,'age_s':0}},'fuel_level_text':'Полная',
 'storage':{'started':True,'mounted':True,'logging':True,'card_bytes':64000000000,'total_bytes':62602396160,'used_bytes':6000,'free_bytes':62602390160,'format_attempted':False,'format_state':0},
 'connections':{'revision':0,'drivers_active':False,'items':[],'message':''},
 'equipment':[{'name':n,'connected':False} for n in ('Бойлер косвенного нагрева','Электрокотёл','Контур 1','Контур 2','Контур 3','Дизельный отопитель','Подогрев топлива')],
 'home_assistant':{'ready':True,'supported':True,'enabled':False,'connected':False,'revision':0,'id':'bp_fixture','port':1883,'host':'','username':'','password_set':False,'discovery_prefix':'homeassistant','discovery_messages':0,'read_only':True}
}
state={'ticket':'','active':False,'token':'','starts':0,'verifies':0,'cancels':0,'offline':False,'ttl':120,'writes':[]}
class Handler(BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def send(self,status,payload,mime='application/json'):
  b=payload if isinstance(payload,bytes) else json.dumps(payload,ensure_ascii=False).encode()
  self.send_response(status);self.send_header('Content-Type',mime+'; charset=utf-8');self.send_header('Content-Length',str(len(b)));self.end_headers();self.wfile.write(b)
 def do_GET(self):
  if self.path=='/':return self.send(200,html,'text/html')
  if self.path=='/api/status':
   if state['offline']:return self.send(503,{'error':'fixture_offline'})
   d=copy.deepcopy(fixture);d['authorized']=bool(state['token']) and self.headers.get('Authorization')=='Bearer '+state['token'];d['pair_available']=state['active'];d['pair_remaining_s']=state['ttl'] if state['active'] else 0
   return self.send(200,d)
  self.send(404,{})
 def do_POST(self):
  body=parse_qs(self.rfile.read(int(self.headers.get('Content-Length','0'))).decode(),keep_blank_values=True);get=lambda k:body.get(k,[''])[0]
  if self.path.startswith('/api/pair'):
   if self.headers.get('X-Boiler-Pairing')!='1':return self.send(403,{'error':'same_origin_required'})
  if self.path=='/api/pair/start':
   state['starts']+=1;state['ticket']=get('request_id');assert len(state['ticket'])==32
   state['active']=True;return self.send(202,{'ok':True,'display_only':True,'expires_in_s':state['ttl']})
  if self.path=='/api/pair/cancel':
   state['cancels']+=1
   if get('request_id')==state['ticket']:state['active']=False
   return self.send(200,{'ok':True})
  if self.path=='/api/pair':
   state['verifies']+=1
   if not state['active'] or get('request_id')!=state['ticket'] or get('code')!='123456':return self.send(403,{'error':'code_invalid_or_expired'})
   state['active']=False;state['token']='f'*32;return self.send(200,{'ok':True,'token':state['token'],'expires_in_s':3600})
  if self.path=='/api/logout':state['token']='';return self.send(200,{'ok':True})
  if self.path=='/api/command':
   if self.headers.get('Authorization')!='Bearer '+state['token'] or not state['token']:return self.send(401,{'error':'pair_required'})
   assert get('action')=='temperature_target'
   channel,value,revision=(int(get(k)) for k in ('channel','value_c','revision'))
   assert channel in range(6) and -55<=value<=125
   state['writes'].append((channel,value,revision))
   target=fixture['temperature_targets'][channel]
   if target['revision']!=revision:return self.send(409,{'error':'stale_revision'})
   target.update(configured=True,value_c=value,revision=revision+1)
   if state.pop('lose_ack',False):return self.send(503,{'error':'fixture_lost_ack'})
   return self.send(200,{'ok':True,'saved':True,'revision':target['revision']})
  self.send(404,{})
server=ThreadingHTTPServer(('127.0.0.1',0),Handler);Thread(target=server.serve_forever,daemon=True).start()
base=f'http://127.0.0.1:{server.server_port}'
try:
 with sync_playwright() as p:
  executable=os.environ.get('BP_BROWSER')
  if not executable and os.name=='nt':executable=r'C:\Program Files\Google\Chrome\Application\chrome.exe'
  browser=p.chromium.launch(headless=True,executable_path=executable)
  context=browser.new_context(viewport={'width':1280,'height':900},device_scale_factor=1,has_touch=True,reduced_motion='reduce')
  page=context.new_page();errors=[];page.on('pageerror',lambda err:errors.append(str(err)))
  page.goto(base);expect(page.locator('#version')).to_have_text(version);expect(page.locator('#fuel-canister')).to_have_attribute('data-state','full')
  assert len(page.locator('.topnav button').all())==4
  expect(page.locator('#home .temp-name').first).to_have_text('В доме')
  assert page.locator('#home svg.temp-icon').count()==3
  expect(page.locator('#home > .grid > .card')).to_have_count(4)
  dialog=page.locator('#temperature-target');slider=page.get_by_role('slider',name='Заданная температура')
  close=dialog.locator('#target-close');save=dialog.locator('#target-save')
  def check_target(index,title):
   expect(dialog).to_be_visible();expect(dialog.locator('#target-title')).to_have_text(title)
   expect(dialog.locator('#target-actual')).to_have_text(f"{fixture['temperatures'][index]['value']:.1f} °C")
   expect(dialog.locator('#target-value')).to_have_text('Не задана')
   expect(slider).to_be_visible();expect(slider).to_be_disabled();expect(save).to_be_disabled()
   assert not dialog.locator('svg').count()
  for kind,index,name in [('house',0,'В доме'),('outdoor',1,'На улице')]:
   icon=page.locator(f'svg[data-icon="{kind}"]');expect(icon).to_be_visible()
   assert icon.get_attribute('aria-hidden')=='true'
   box=icon.bounding_box();page.mouse.click(box['x']+box['width']/2,box['y']+box['height']/2)
   check_target(index,name)
   close.click();expect(page.locator(f'#home [data-temp="{index}"]')).to_be_focused()
  coolant=page.get_by_role('region',name='Теплоноситель')
  expect(coolant.get_by_role('heading',name='Теплоноситель')).to_have_count(1)
  expect(coolant.locator('svg[data-icon="radiator"]')).to_have_count(1)
  supply=coolant.get_by_role('button',name=re.compile('Подача'))
  returning=coolant.get_by_role('button',name=re.compile('Обратка'))
  supply.focus();page.keyboard.press('Tab');expect(returning).to_be_focused()
  for button,name,index,key in ((supply,'Подача',2,'Enter'),(returning,'Обратка',4,'Space')):
   button.focus();page.keyboard.press(key)
   check_target(index,name)
   page.keyboard.press('Escape');expect(button).to_be_focused()
  fixture['temperatures'][4]['valid']=False
  expect(coolant.locator('#tstate4')).to_have_text('Устарело',timeout=5500)
  expect(coolant.locator('#t4')).to_have_text('--.- °C')
  expect(coolant.locator('#t2')).to_have_text('61.7 °C')
  fixture['temperatures'][4]['valid']=True
  expect(coolant.locator('#t4')).to_have_text('43.2 °C',timeout=5500)
  fuel=page.get_by_role('region',name='Топливо',exact=True)
  tank=fuel.get_by_role('button',name=re.compile('В баке'))
  outlet=fuel.get_by_role('button',name=re.compile('На выходе'))
  tank.focus();page.keyboard.press('Tab');expect(outlet).to_be_focused()
  for button,title,index in ((tank,'Топливо в баке',3),(outlet,'Топливо на выходе',5)):
   button.press('Enter')
   check_target(index,title)
   close.click();expect(button).to_be_focused()
  fixture['temperatures'][5]['valid']=False
  expect(fuel.locator('#tstate5')).to_have_text('Устарело',timeout=5500)
  expect(fuel.locator('#t5')).to_have_text('--.- °C');expect(fuel.locator('#t3')).to_have_text('34.8 °C')
  fixture['temperatures'][5]['valid']=True
  expect(fuel.locator('#t5')).to_have_text('17.1 °C',timeout=5500)
  page.screenshot(path=str(out/'desktop-home.png'),full_page=True)
  fuel.get_by_role('button',name=re.compile('Открыть раздел топлива')).click()
  expect(page.locator('#fuel')).to_be_visible()
  expect(page.locator('#fuel-temp')).to_have_text('34.8 °C');expect(page.locator('#fuel-outlet-temp')).to_have_text('17.1 °C')
  for index,title in ((3,'Топливо в баке'),(5,'Топливо на выходе')):
   trigger=page.locator(f'#fuel [data-temp="{index}"]')
   trigger.click();check_target(index,title)
   close.click();expect(page.locator('#fuel')).to_be_visible();expect(trigger).to_be_focused()
  page.click('.topnav [data-page="home"]')
  page.click('#pairbutton');expect(page.locator('#pairbox')).to_be_visible();expect(page.locator('#pair-submit')).to_be_enabled()
  assert state['starts']==1 and state['active'] and page.input_value('#paircode')==''
  assert '123456' not in page.content() # Test challenge never supplied to browser by server.
  page.screenshot(path=str(out/'pairing-dialog.png'),full_page=True)
  page.fill('#paircode','000000');page.click('#pair-submit');expect(page.locator('#pair-error')).to_contain_text('Неверный');assert state['active']
  page.fill('#paircode','123456');page.click('#pair-submit');expect(page.locator('#pairbox')).not_to_be_visible();expect(page.locator('#logout')).to_be_visible();assert not state['active']
  # A drag changes only the draft; other channels, operator navigation and polling preserve independent values.
  assert not state['writes']
  for channel,button,key,expected in ((0,page.locator('#home [data-temp="0"]'),'ArrowLeft',21),
                                      (1,page.locator('#home [data-temp="1"]'),'Home',-55),
                                      (2,supply,'ArrowRight',63),(4,returning,'End',125),
                                      (3,tank,'ArrowLeft',34),(5,outlet,'ArrowRight',18)):
   count=len(state['writes'])
   button.click();expect(slider).to_be_focused();expect(save).to_be_disabled()
   slider.press(key);expect(dialog.locator('#target-value')).to_have_text(f'{expected} °C')
   assert len(state['writes'])==count
   save.click();expect(dialog.locator('#target-status')).to_contain_text('сохранена',timeout=9000)
   assert state['writes'][-1]==(channel,expected,0)
   close.click();expect(button).to_be_focused()
   button.click();expect(slider).to_have_value(str(expected));expect(save).to_be_disabled();close.click()
  snapshot=copy.deepcopy(fixture['temperature_targets']);count=len(state['writes'])
  page.set_viewport_size({'width':390,'height':844})
  tank.click();box=slider.bounding_box();page.touchscreen.tap(box['x']+box['width']*.53,box['y']+box['height']/2)
  expect(save).to_be_enabled();draft=slider.input_value()
  fixture['temperatures'][3]['value']=35.1
  expect(dialog.locator('#target-actual')).to_have_text('35.1 °C',timeout=5500)
  expect(slider).to_have_value(draft)
  assert page.evaluate('document.documentElement.scrollWidth<=innerWidth')
  dialog.screenshot(path=str(out/'temperature-slider-mobile.png'))
  close.click();expect(tank).to_be_focused();assert fixture['temperature_targets']==snapshot
  tank.click();expect(slider).to_have_value('34');slider.press('ArrowRight')
  fixture['temperature_targets'][3].update(value_c=40,revision=2)
  expect(dialog.locator('#target-status')).to_contain_text('изменилось',timeout=5500)
  expect(slider).to_have_value('35');expect(save).to_be_disabled()
  dialog.locator('#target-reload').click();expect(slider).to_have_value('40',timeout=5500)
  assert len(state['writes'])==count
  state['offline']=True;expect(slider).to_be_disabled(timeout=12000)
  expect(dialog.locator('#target-actual')).to_have_text('Нет связи')
  state['offline']=False;expect(slider).to_be_enabled(timeout=5500)
  fixture['update']['busy']=True;expect(slider).to_be_disabled(timeout=5500)
  fixture['update']['busy']=False;expect(slider).to_be_enabled(timeout=5500)
  state['lose_ack']=True;slider.press('ArrowRight');save.click()
  expect(dialog.locator('#target-status')).to_contain_text('не подтверждено',timeout=5500)
  expect(save).to_be_disabled();dialog.locator('#target-reload').click()
  expect(slider).to_have_value('41');expect(slider).to_be_enabled(timeout=5500)
  assert state['writes'][count:]==[(3,41,2)]
  close.click();page.set_viewport_size({'width':1280,'height':900})
  assert all(fixture['temperature_targets'][i]==snapshot[i] for i in (0,1,2,4,5))
  page.click('.topnav [data-page="settings"]');expect(page.locator('#settings-menu')).to_be_visible();page.screenshot(path=str(out/'desktop-settings.png'),full_page=True)
  for tab in ('wifi','ble','screen','clock'):
   page.click(f'#settings-menu [data-tab="{tab}"]');expect(page.locator('#tab-'+tab)).to_be_visible();page.click('#settings-back [data-action="settings-root"]')
  for sub in ('ha','connections','service'):
   page.click(f'#settings-menu [data-page="{sub}"]');expect(page.locator('#'+sub)).to_be_visible()
   assert page.evaluate('document.documentElement.scrollWidth<=innerWidth')
   if sub=='ha':assert page.input_value('#ha-password')=='';page.screenshot(path=str(out/'ha-settings.png'),full_page=True)
   page.click('#'+sub+' [data-action="settings-root"]')
  page.click('.topnav [data-page="home"]')
  for lower,upper,kind in [(True,False,'half'),(False,False,'empty'),(False,True,'invalid'),(True,True,'full')]:
   fixture['fuel']['lower']['reached']=lower;fixture['fuel']['upper']['reached']=upper
   expect(page.locator('#fuel-canister')).to_have_attribute('data-state',kind,timeout=5500)
   if kind=='empty':expect(page.locator('#canister-symbol')).to_have_text('!');page.screenshot(path=str(out/'fuel-empty.png'),full_page=True)
  fixture['fuel']['lower']['age_s']=30
  expect(page.locator('#fuel-canister')).to_have_attribute('data-state','stale',timeout=5500)
  fixture['fuel']['lower']['age_s']=0
  state['offline']=True;expect(page.locator('#tstate0')).to_have_text('Нет связи',timeout=12000);expect(page.locator('#fuel-canister')).to_have_attribute('data-state','unknown')
  state['offline']=False;expect(page.locator('#fuel-canister')).to_have_attribute('data-state','full',timeout=5500)
  page.click('#logout');expect(page.locator('#pairbutton')).to_be_visible();page.click('#pairbutton');expect(page.locator('#pair-submit')).to_be_enabled();page.click('#pairbox [data-action="hide-pair"]');expect(page.locator('#pairbox')).not_to_be_visible();page.wait_for_timeout(150);assert not state['active'] and state['cancels']>=1
  state['ttl']=1;page.click('#pairbutton');expect(page.locator('#pair-expiry')).to_contain_text('Срок кода истёк',timeout=3000);expect(page.locator('#pair-submit')).to_be_disabled();page.keyboard.press('Escape');expect(page.locator('#pairbox')).not_to_be_visible();state['ttl']=120
  for width in (320,390,768):
   page.set_viewport_size({'width':width,'height':844});page.click('.topnav [data-page="home"]')
   if not page.evaluate('document.documentElement.scrollWidth<=innerWidth'):
    page.screenshot(path=str(out/'overflow-home.png'),full_page=True)
    print('OVERFLOW',width,page.evaluate("Array.from(document.querySelectorAll('body *')).filter(e=>e.getBoundingClientRect().right>innerWidth+1).slice(0,12).map(e=>({tag:e.tagName,id:e.id,class:e.className,right:e.getBoundingClientRect().right}))"))
   assert page.evaluate('document.documentElement.scrollWidth<=innerWidth'),('overflow home',width)
   if width==390:page.screenshot(path=str(out/'mobile-home.png'),full_page=True)
   page.click('.topnav [data-page="settings"]');assert page.evaluate('document.documentElement.scrollWidth<=innerWidth'),('overflow settings',width)
   page.click('#settings-menu [data-page="connections"]');assert page.evaluate('document.documentElement.scrollWidth<=innerWidth'),('overflow sensors',width)
   page.click('#connections [data-action="settings-root"]');page.click('#settings-menu [data-page="ha"]');assert page.evaluate('document.documentElement.scrollWidth<=innerWidth'),('overflow HA',width)
  assert len(state['writes'])==7,state['writes']
  assert not errors,errors
  result={'browser':'Chromium','desktop_and_mobile_widths':[1280,768,390,320],'page_errors':errors,'pair_starts':state['starts'],'pair_verifies':state['verifies'],'pair_cancels':state['cancels'],'settings_writes':state['writes'],'physical_panel_contacted':False,'data':'isolated synthetic fixtures, not live measurements','result':'PASS'}
  (out/'browser-result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result))
  browser.close()
finally:server.shutdown()
