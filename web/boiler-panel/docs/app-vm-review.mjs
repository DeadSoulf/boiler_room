// Reproduction of the independent, inert-DOM review executed in memory.
// This checks JavaScript execution and state transitions, not browser geometry
// or physical touch events. It makes no network requests and writes no files.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import {createHash} from 'node:crypto';
import * as model from '../dist/model.js';
import * as navigation from '../dist/navigation.js';
import {fileURLToPath} from 'node:url';

const project = fileURLToPath(new URL('..', import.meta.url));
console.log(JSON.stringify({
  review: 'Independent app.js execution with an inert DOM',
  at: new Date().toISOString(),
  node: process.version,
  project,
  sha256: Object.fromEntries(['app.js','model.js','navigation.js','index.html','styles.css'].map(name => [name, createHash('sha256').update(fs.readFileSync(`${project}/dist/${name}`)).digest('hex')])),
}, null, 2));

class Classes {
  values = new Set();
  contains(v){return this.values.has(v)}
  toggle(v,flag){const yes = flag ?? !this.contains(v); if(yes)this.values.add(v);else this.values.delete(v);return yes}
}
class Element {
  constructor(id=''){this.id=id;this.dataset={};this.innerHTML='';this.textContent='';this.hidden=false;this.disabled=false;this.open=false;this.classList=new Classes();this.listeners={};this.attributes={};this.clientWidth=420;this.scrollTop=0;this.pointerIds=new Set();}
  addEventListener(type,fn){(this.listeners[type]??=[]).push(fn)}
  querySelectorAll(){return []}
  querySelector(){return null}
  setAttribute(name,value){this.attributes[name]=value}
  focus(){}
  showModal(){this.open=true}
  close(){this.open=false}
  getBoundingClientRect(){return {left:0,top:0,right:500,bottom:500}}
  hasPointerCapture(id){return this.pointerIds.has(id)}
  setPointerCapture(id){this.pointerIds.add(id)}
  append(){}
  remove(){}
  click(){}
}
const elements=new Map();
const document=new Element('document');
document.getElementById=id=>{if(!elements.has(id))elements.set(id,new Element(id));return elements.get(id)};
document.body=new Element('body');
document.createElement=tag=>new Element(tag);
let now=0, timerId=0;
const timers=new Map();
const location={hash:''};
const window=new Element('window');window.scrollTo=()=>{};
const context=vm.createContext({...model,...navigation,document,window,location,history:{pushState(a,b,hash){location.hash=hash}},performance:{now:()=>now},ResizeObserver:class{observe(){}},setTimeout:(fn,delay)=>{timers.set(++timerId,{fn,delay});return timerId},clearTimeout:id=>timers.delete(id),setInterval:()=>0,Blob,URL,console});
const source=fs.readFileSync(`${project}/dist/app.js`,'utf8').replace(/^import .*;\n/gm,'');
vm.runInContext(source,context);
const run=code=>vm.runInContext(code,context);
const main=elements.get('mainContent');
const routes=run('Object.keys(titles)');
let matrix=0, charts=0;
for(const scenario of model.SCENARIOS){
  for(const page of routes){
    run(`state=createState(${JSON.stringify(scenario.id)}); route=${JSON.stringify(page)}; render();`);
    assert.ok(main.innerHTML.length>50);
    assert.equal(elements.get('homeClock').hidden,page!=='overview');
    assert.equal(elements.get('homeButton').hidden,page==='overview');
    if(['overview','fuel'].includes(page)){
      assert.match(main.innerHTML,/data-heating="on"/);assert.match(main.innerHTML,/data-heating="off"/);
      assert.match(main.innerHTML,/Нагреватель/);
    }
    if(['low','stale','inconsistent','disconnected'].includes(scenario.id))assert.match(main.innerHTML,/Действующее предупреждение/);
    if(['wifi','datetime','display'].includes(page))assert.match(main.innerHTML,/fieldset[^>]*disabled/);
    matrix++;
  }
  for(const sensor of model.SENSOR_DEFS){
    for(const period of ['1h','6h','24h']){
      const slot=new Element();slot.dataset={sensor:sensor.id,period,large:'true'};context.slot=slot;run('drawChart(slot)');assert.match(slot.innerHTML,/chart/);charts++;
    }
  }
}
function click(dataset){
  const button=new Element('testButton');button.dataset=dataset;
  const event={detail:0,target:{closest:s=>s==='button'?button:null},preventDefault(){},stopImmediatePropagation(){}};
  for(const fn of document.listeners.click)fn(event);
}
for(const [outcome,mode] of [['accepted','enabled'],['rejected','off'],['timeout','unknown']]){
  run(`state=createState('normal');route='fuel';commandOutcome=${JSON.stringify(outcome)};render();`);
  click({heating:'on'});
  assert.equal(run('state.heating.command.status'),'pending');
  assert.equal(run('state.heating.mode'),'off');
  assert.match(main.innerHTML,/Ожидается подтверждение/);
  timers.get(run('commandTimer')).fn();
  assert.equal(run('state.heating.command.status'),outcome);
  assert.equal(run('state.heating.mode'),mode);
  assert.equal(run('state.heating.actual'),'unknown');
}
run("state=createState('normal');route='fuel';commandOutcome='accepted';render();");
click({heating:'on'});const staleTimer=timers.get(run('commandTimer')).fn;
click({heating:'off'});assert.equal(run('state.heating.mode'),'unknown');staleTimer();assert.equal(run('state.heating.mode'),'unknown');
timers.get(run('commandTimer')).fn();assert.equal(run('state.heating.mode'),'off');
run("state=createState('normal');route='fuel';render();");click({heating:'on'});const old=timers.get(run('commandTimer')).fn;click({action:'reset'});old();assert.equal(run('state.heating.command'),null);
run("state=createState('low');route='journal';render();");const eventId=run('state.events[0].id');click({ack:eventId});assert.match(main.innerHTML,/Отмечено как прочитанное/);assert.match(main.innerHTML,/Действующее предупреждение/);
run("state=createState('normal');route='overview';render();");
const target={closest:()=>null};
for(const fn of main.listeners.pointerdown)fn({isPrimary:true,button:0,pointerId:1,clientX:220,clientY:20,target});
now=300;
for(const fn of main.listeners.pointerup)fn({pointerId:1,clientX:80,clientY:30,preventDefault(){}});
assert.equal(run('route'),'controls');
let prevented=false,stopped=false;
for(const fn of document.listeners.click.slice(0,1))fn({detail:1,preventDefault(){prevented=true},stopImmediatePropagation(){stopped=true}});
assert.equal(prevented,true);assert.equal(stopped,true);
console.log(JSON.stringify({renderCases:matrix,chartCases:charts,commandOutcomes:3,pendingOnReplacedByOff:'pass',lateResponseAfterReset:'pass',ackPreservesAlert:'pass',swipeNavigationAndClickSuppression:'pass',geometry:'not tested'},null,2));
