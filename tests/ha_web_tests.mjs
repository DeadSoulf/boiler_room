// Exercise the actual HA form functions, with isolated DOM/network stand-ins.
// These tests do not claim a browser layout or a live MQTT connection.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const page=fs.readFileSync(new URL('../src/panel_web_page.h',import.meta.url),'utf8');
const start=page.indexOf('function renderHA(){'),stop=page.indexOf('function updateView(){',start);
assert(start>0&&stop>start);
const ids=['ha-enabled','ha-host','ha-port','ha-user','ha-password','ha-clear-password','ha-prefix','ha-lan'];
const nodes=Object.fromEntries(ids.map(id=>[id,{value:'',checked:false,disabled:false}]));
nodes['ha-form']={querySelectorAll:()=>ids.map(id=>nodes[id])};
const sent=[],messages=[];
const context={
 current:{authorized:true,home_assistant:{ready:true,enabled:true,connected:true,revision:3,host:'192.0.2.1',port:1883,username:'fixture',password_set:true,discovery_prefix:'homeassistant'}},
 haFormRevision:null,haDirty:false,
 $:id=>nodes[id],text:()=>{},show:()=>{},startPairing:()=>messages.push(['pairing requested']),notice:(...args)=>messages.push(args),
 sendPost:async(path,data,auth)=>{sent.push({path,data:{...data},auth});return {ok:true};},poll:async()=>{}
};
vm.createContext(context);vm.runInContext(page.slice(start,stop),context);
context.renderHA();assert.equal(context.haFormRevision,3);assert.equal(nodes['ha-host'].value,'192.0.2.1');assert.equal(nodes['ha-password'].value,'');
nodes['ha-host'].value='editing.local';context.haDirty=true;context.current.home_assistant.host='changed.local';
context.renderHA();assert.equal(nodes['ha-host'].value,'editing.local');
context.haDirty=false;context.haFormRevision=null;context.current.home_assistant.host='192.0.2.1';context.renderHA();
const event={preventDefault(){}};
await context.saveHA(event);assert.equal(sent.length,0); // explicit LAN consent required
nodes['ha-lan'].checked=true;nodes['ha-password'].value='fixture-secret';
await context.saveHA(event);assert.equal(sent.length,1);assert.equal(sent[0].path,'/api/command');assert(sent[0].auth);
assert.equal(sent[0].data.action,'ha_save');assert.equal(sent[0].data.password_mode,'set');assert.equal(sent[0].data.revision,'3');
assert.equal(sent[0].data.password,'fixture-secret');assert.equal(nodes['ha-password'].value,'');
context.renderHA();await context.saveHA(event);assert.equal(sent[1].data.password_mode,'keep');
context.renderHA();nodes['ha-clear-password'].checked=true;nodes['ha-password'].value='contradiction';
await context.saveHA(event);assert.equal(sent.length,2);
nodes['ha-password'].value='';await context.saveHA(event);assert.equal(sent[2].data.password_mode,'clear');
context.renderHA();nodes['ha-enabled'].checked=false;nodes['ha-lan'].checked=false;
await context.saveHA(event);assert.equal(sent[3].data.enabled,'0');
context.renderHA();nodes['ha-password'].value='do-not-expose';context.current.authorized=false;context.renderHA();
assert.equal(nodes['ha-password'].value,'');assert.equal(nodes['ha-host'].value,'');assert.equal(nodes['ha-user'].value,'');
assert(nodes['ha-password'].disabled);const before=sent.length;await context.saveHA(event);assert.equal(sent.length,before);
assert(page.includes('addEventListener("submit",saveHA)'));
console.log('PASS: HA form preserves edits, requires pairing/LAN consent, uses revision and explicit password modes, clears secrets on submit/session expiry');
