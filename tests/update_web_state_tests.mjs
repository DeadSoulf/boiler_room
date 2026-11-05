// Execute the actual updater functions with deterministic DOM/network stand-ins.
// Browser layout and real pointer coverage live in update_web_browser.py.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const source=fs.readFileSync(new URL('../src/panel_web_page.h',import.meta.url),'utf8');
const start=source.indexOf('function saveUpdatePending(){'),end=source.indexOf('function handleAction(b){',start);
assert(start>0&&end>start);
const nodes=new Map(),storage=new Map(),sent=[],messages=[];
function node(id){
 if(!nodes.has(id))nodes.set(id,{textContent:'',disabled:false,value:0,dataset:{},hidden:false,classes:new Set(),
  classList:{toggle(name,on){const n=nodes.get(id);if(on)n.classes.add(name);else n.classes.delete(name);}}});
 return nodes.get(id);
}
const ctx={
 Date,Number,String,Math,JSON,current:null,token:'fixture-session',serverOnline:true,page:'update',
 updateRequestInFlight:false,pendingUpdate:null,updateResult:'',updateResultError:false,
 $:node,text:(id,value)=>{node(id).textContent=String(value??'—')},
 show:(id,on)=>{node(id).hidden=!on},numeric:n=>Number.isFinite(Number(n))?Number(n):0,
 notice:(...value)=>messages.push(value),startPairing:()=>messages.push(['pair']),
 confirm:()=>true,
 sessionStorage:{setItem:(k,v)=>storage.set(k,v),removeItem:k=>storage.delete(k),getItem:k=>storage.get(k)||null},
};
vm.createContext(ctx);vm.runInContext(source.slice(start,end),ctx);
ctx.updateView=()=>ctx.renderUpdate();
ctx.poll=async()=>ctx.reconcileUpdate(ctx.current);
function reset(){
 ctx.current={version:'v1.0.0',authorized:true,update:{started:true,supported:true,busy:false,
  available:true,cancel_allowed:false,rollback_available:true,boot_pending:false,automatic_rollback:true,
  phase:'available',done:0,total:0,percent:0,latest_version:'v1.1.0',previous_version:'v0.9.0',message:'',error_code:0}};
 ctx.token='fixture-session';ctx.serverOnline=true;ctx.pendingUpdate=null;ctx.updateResult='';
 ctx.updateResultError=false;ctx.updateRequestInFlight=false;ctx.confirm=()=>true;
 ctx.sendPost=async(path,data,secret)=>{
  sent.push({path,data:{...data},secret});
  const u=ctx.current.update;
  if(data.action==='update_cancel'){u.busy=false;u.phase='cancelled';u.cancel_allowed=false;return {ok:true};}
  if(data.action==='update_check'){u.busy=true;u.phase='checking';return {ok:true};}
  const target=data.action==='update_rollback'?u.previous_version:u.latest_version;
  if(data.version!==target){const error=Error('version changed');error.httpStatus=409;throw error;}
  u.busy=true;u.phase=data.action==='update_rollback'?'rebooting':'downloading';u.cancel_allowed=data.action==='update_install';
  return {ok:true};
 };
 sent.length=messages.length=0;storage.clear();ctx.renderUpdate();
}
reset();
ctx.reconcileUpdate(ctx.current);assert.equal(ctx.updateResult,''); // Capability is not an occurred rollback.
ctx.current.update.phase='current';ctx.reconcileUpdate(ctx.current);assert.equal(ctx.updateResult,'');
ctx.current.update.phase='success';ctx.reconcileUpdate(ctx.current);assert(!ctx.updateResultError);assert(ctx.updateResult.includes('Установлена v1.0.0'));
reset();
ctx.token='';await ctx.requestUpdate('update_install','v1.1.0');assert.equal(sent.length,0);assert.equal(messages.at(-1)[0],'pair');
reset();ctx.confirm=()=>false;await ctx.requestUpdate('update_install','v1.1.0');
assert.equal(sent.length,0);assert.equal(ctx.pendingUpdate,null);
reset();ctx.serverOnline=false;await ctx.requestUpdate('update_install','v1.1.0');assert.equal(sent.length,0);
reset();ctx.current.update.supported=false;await ctx.requestUpdate('update_install','v1.1.0');assert.equal(sent.length,0);

// The confirmation captures the displayed release, even if new status arrives.
reset();ctx.confirm=()=>{ctx.current.update.latest_version='v1.2.0';return true};
await ctx.requestUpdate('update_install','v1.1.0');
assert.deepEqual(sent[0].data,{action:'update_install',version:'v1.1.0',confirm:'v1.1.0'});
assert.equal(ctx.pendingUpdate,null);assert(ctx.updateResult.includes('Запрос не принят'));assert(ctx.updateResultError);
assert(sent[0].secret);
reset();await ctx.requestUpdate('update_install','v1.1.0');assert.equal(sent.length,1);
assert.equal(JSON.parse(storage.get('boiler_panel_update')).target,'v1.1.0');
Object.assign(ctx.current.update,{done:3700,total:10000,percent:37});
ctx.renderUpdate();assert(node('update-progress-text').textContent.includes('37%'));
assert(node('update-check').disabled&&node('update-install').disabled&&!node('update-cancel').disabled);
await ctx.requestUpdate('update_install','v1.1.0');assert.equal(sent.length,1);
ctx.current.update.cancel_allowed=false;ctx.renderUpdate();assert(node('update-cancel').disabled);
await ctx.requestUpdate('update_cancel');assert.equal(sent.length,1);
ctx.current.update.cancel_allowed=true;await ctx.requestUpdate('update_cancel');
assert.equal(sent[1].data.action,'update_cancel');assert.equal(ctx.pendingUpdate,null);
assert(ctx.updateResult.includes('отменено'));assert(!node('update-install').disabled);

reset();await ctx.requestUpdate('update_check');assert.equal(sent[0].data.action,'update_check');
assert.equal(ctx.pendingUpdate,null);assert(node('update-check').disabled);
reset();await ctx.requestUpdate('update_rollback','v0.9.0');
assert.deepEqual(sent[0].data,{action:'update_rollback',version:'v0.9.0',confirm:'v0.9.0'});
assert.equal(ctx.pendingUpdate.target,'v0.9.0');assert(ctx.pendingUpdate.rebootSeen);
ctx.pendingUpdate.connectionLost=true;ctx.current.version='v0.9.0';
ctx.current.update={}; // Older retained firmware may predate the OTA status fields.
ctx.reconcileUpdate(ctx.current);ctx.renderUpdate();
assert(ctx.updateResult.includes('Возврат завершён'));assert.equal(ctx.pendingUpdate,null);

reset();await ctx.requestUpdate('update_install','v1.1.0');
ctx.pendingUpdate.connectionLost=true;ctx.current.version='v1.1.0';
Object.assign(ctx.current.update,{boot_pending:true,phase:'installing'});
ctx.reconcileUpdate(ctx.current);ctx.renderUpdate();assert(ctx.pendingUpdate);assert(node('update-result').textContent.includes('проверка запуска'));
Object.assign(ctx.current.update,{boot_pending:false,busy:false,phase:'success'});
ctx.reconcileUpdate(ctx.current);ctx.renderUpdate();
assert.equal(ctx.pendingUpdate,null);assert(ctx.updateResult.includes('Связь восстановлена. Установлена v1.1.0'));
assert.equal(sent.length,1);assert(!storage.has('boiler_panel_update'));

// A late POST timeout must not erase a final outcome already observed by polling.
reset();let rejectPost;
ctx.sendPost=async(path,data,secret)=>{sent.push({path,data:{...data},secret});return new Promise((_,reject)=>{rejectPost=reject;});};
const request=ctx.requestUpdate('update_install','v1.1.0');
assert(ctx.pendingUpdate&&ctx.updateRequestInFlight);
ctx.current.version='v1.1.0';ctx.current.update.phase='success';
ctx.reconcileUpdate(ctx.current);assert.equal(ctx.pendingUpdate,null);
rejectPost(Error('connection lost after reboot'));await request;
assert(ctx.updateResult.includes('Установлена v1.1.0'));assert(!ctx.updateResultError);

// A new accepted request can briefly retain a prior terminal phase while busy.
reset();ctx.pendingUpdate={target:'v1.1.0',from:'v1.0.0',action:'update_install',at:Date.now()};
Object.assign(ctx.current.update,{busy:true,phase:'error'});ctx.reconcileUpdate(ctx.current);assert(ctx.pendingUpdate);
Object.assign(ctx.current.update,{busy:false,phase:'rolled_back',automatic_rollback:true});
ctx.reconcileUpdate(ctx.current);assert.equal(ctx.pendingUpdate,null);assert(ctx.updateResultError);
assert(ctx.updateResult.includes('выполнен возврат'));
reset();ctx.pendingUpdate={target:'v1.1.0',from:'v1.0.0',action:'update_install',at:Date.now()-31000};
ctx.reconcileUpdate(ctx.current);assert.equal(ctx.pendingUpdate,null);assert(ctx.updateResult.includes('не подтверждено'));
assert.equal(sent.length,0);
console.log('PASS: actual updater functions; pairing/availability, pinned confirmations, stale version rejection, progress/buttons, cancellation, rollback, pending boot/reconnect, late POST race and no automatic resubmission');
