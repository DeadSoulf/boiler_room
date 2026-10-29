/**
 * Independent app integration checks with an inert DOM and controlled timers.
 * This exercises render and event handlers; it does not verify browser layout,
 * CSS, touch hardware, external publication, or any physical equipment.
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import * as model from '../dist/model.js';
import * as navigation from '../dist/navigation.js';
import * as equipment from '../dist/equipment.js';

const appSource = fs.readFileSync(new URL('../dist/app.js', import.meta.url), 'utf8')
  .replace(/^import .*;\n/gm, '');

class Classes {
  values = new Set();
  contains(value) { return this.values.has(value); }
  toggle(value, force) {
    const enabled = force ?? !this.contains(value);
    if (enabled) this.values.add(value); else this.values.delete(value);
    return enabled;
  }
}
class Element {
  constructor(id = '') {
    Object.assign(this, {id, dataset: {}, innerHTML: '', textContent: '', hidden: false,
      disabled: false, open: false, listeners: {}, attributes: {}, clientWidth: 420,
      scrollTop: 0, classList: new Classes(), pointerIds: new Set()});
  }
  addEventListener(type, callback) { (this.listeners[type] ??= []).push(callback); }
  querySelectorAll() { return []; }
  querySelector() { return null; }
  setAttribute(name, value) { this.attributes[name] = value; }
  focus() {}
  showModal() { this.open = true; }
  close() { this.open = false; }
  getBoundingClientRect() { return {left: 0, top: 0, right: 500, bottom: 500}; }
  hasPointerCapture(id) { return this.pointerIds.has(id); }
  setPointerCapture(id) { this.pointerIds.add(id); }
  append() {}
  remove() {}
  click() {}
}

function harness() {
  const elements = new Map(), timers = new Map();
  const document = new Element('document'), window = new Element('window');
  document.getElementById = id => {
    if (!elements.has(id)) elements.set(id, new Element(id));
    return elements.get(id);
  };
  document.body = new Element('body');
  document.createElement = tag => new Element(tag);
  window.scrollTo = () => {};
  let monotonicTime = 0, nextTimer = 0;
  const location = {hash: ''};
  const context = vm.createContext({
    ...model, ...navigation, ...equipment, document, window, location,
    history: {pushState(_state, _title, hash) { location.hash = hash; }},
    performance: {now: () => monotonicTime},
    ResizeObserver: class { observe() {} },
    setTimeout(callback, delay) { timers.set(++nextTimer, {callback, delay}); return nextTimer; },
    clearTimeout(id) { timers.delete(id); }, setInterval() { return 0; }, Blob, URL,
  });
  const run = code => vm.runInContext(code, context);
  run(appSource);
  const main = elements.get('mainContent'), dialog = elements.get('scenarioDialog');
  const read = code => JSON.parse(JSON.stringify(run(code)));
  const dispatch = (target, type, event) => {
    let stopped = false;
    event.preventDefault ??= () => {};
    event.stopImmediatePropagation ??= () => { stopped = true; };
    for (const callback of target.listeners[type] || []) {
      callback(event);
      if (stopped) break;
    }
  };
  const click = (dataset, options = {}) => {
    const button = new Element(options.id || 'testButton');
    button.dataset = dataset; button.disabled = !!options.disabled;
    dispatch(document, 'click', {detail: 0, target: {closest: selector => selector === 'button' ? button : null}});
  };
  const selectScenario = scenario => dispatch(dialog, 'change', {target: {name: 'scenario', value: scenario}});
  const selectOutcome = outcome => dispatch(dialog, 'change', {target: {id: 'commandResponse', value: outcome}});
  const timerFor = id => timers.get(run(`equipmentTimers.get(${JSON.stringify(id)})`));
  return {run, read, elements, main, context, document, window, dialog, timers, location,
    dispatch, click, selectScenario, selectOutcome, timerFor, setTime: value => { monotonicTime = value; }};
}

test('all 16 routes render in seven scenarios, preserving four primary pages and fuel features', () => {
  const h = harness(), routes = h.read('Object.keys(titles)');
  assert.equal(routes.length, 16);
  assert.deepEqual(navigation.PRIMARY_PAGES.map(page => page.label), ['Главная', 'Управление', 'Настройки', 'Сервис']);
  assert.deepEqual(model.SENSOR_DEFS.map(sensor => sensor.label), ['В котельной', 'На улице', 'Теплоноситель', 'Топливо']);
  let count = 0;
  for (const {id} of model.SCENARIOS) for (const route of routes) {
    h.run(`state=createState(${JSON.stringify(id)}); equipmentState=createEquipmentState(state.scenario,state.now); route=${JSON.stringify(route)}; render();`);
    assert.ok(h.main.innerHTML.length > 100, `${route}/${id}`);
    assert.equal(h.elements.get('homeClock').hidden, route !== 'overview');
    assert.equal(h.elements.get('homeButton').hidden, route === 'overview');
    assert.equal((h.elements.get('desktopNav').innerHTML.match(/class="nav-button"/g) || []).length, 4);
    if (['overview', 'fuel'].includes(route)) {
      assert.match(h.main.innerHTML, /data-heating="on"/);
      assert.match(h.main.innerHTML, /data-heating="off"/);
      assert.match(h.main.innerHTML, /до 80 °C/);
      assert.match(h.main.innerHTML, /Верхний/);
      assert.match(h.main.innerHTML, /Нижний/);
    }
    if (['low', 'inconsistent', 'stale', 'disconnected'].includes(id)) assert.match(h.main.innerHTML, /Действующее предупреждение/);
    if (['dhw', 'electric', 'diesel', 'equipment-journal'].includes(route)) {
      assert.match(h.elements.get('pageNavigation').innerHTML, /data-route="controls"/);
      assert.match(h.elements.get('pageNavigation').innerHTML, /К управлению/);
      assert.equal(h.main.dataset.primary, 'false');
    }
    count++;
  }
  assert.equal(count, 112);
});

test('all existing temperature charts retain their scenario, sensor, and period combinations', () => {
  const h = harness(); let count = 0;
  for (const {id} of model.SCENARIOS) {
    h.run(`state=createState(${JSON.stringify(id)});`);
    for (const sensor of model.SENSOR_DEFS) for (const period of ['1h', '6h', '24h']) {
      const slot = new Element(); slot.dataset = {sensor: sensor.id, period, large: 'true'};
      h.context.slot = slot; h.run('drawChart(slot)');
      assert.match(slot.innerHTML, /chart/);
      if (id === 'disconnected') assert.match(slot.innerHTML, /История пока недоступна/);
      count++;
    }
  }
  assert.equal(count, 84);
});

test('equipment pages expose the requested devices, three distinct circuits, and unknown feedback', () => {
  const h = harness();
  h.click({route: 'controls'});
  for (const route of ['dhw', 'electric', 'diesel', 'fuel']) assert.match(h.main.innerHTML, new RegExp(`data-route="${route}"`));
  h.click({route: 'electric'});
  for (const id of ['electric', 'circuit1', 'circuit2', 'circuit3']) {
    assert.match(h.main.innerHTML, new RegExp(`data-equipment="${id}" data-enabled="true"`));
    assert.match(h.main.innerHTML, new RegExp(`data-equipment="${id}" data-enabled="false"`));
  }
  assert.equal((h.main.innerHTML.match(/class="panel circuit-card"/g) || []).length, 3);
  assert.equal((h.main.innerHTML.match(/Фактическая работа/g) || []).length, 4);
  h.click({route: 'dhw'});
  assert.match(h.main.innerHTML, /Температура в бойлере · демо/);
  assert.match(h.main.innerHTML, /Источник нагрева<\/span><strong>Не назначен/);
  h.selectScenario('stale');
  assert.match(h.main.innerHTML, /equipment-temperature">—/);
  assert.match(h.main.innerHTML, /Данные устарели/);
  h.selectScenario('disconnected');
  assert.match(h.main.innerHTML, /equipment-temperature">—/);
  assert.match(h.main.innerHTML, /data-equipment="dhw" data-enabled="true"[^>]*disabled/);
  assert.match(h.main.innerHTML, /data-equipment="dhw" data-enabled="false"[^>]*disabled/);
});

test('clicks and reply timers keep pending, accepted, rejected, and timeout separate for every device', () => {
  for (const {id} of equipment.EQUIPMENT_DEFS) for (const [outcome, mode] of [['accepted', 'enabled'], ['rejected', 'off'], ['timeout', 'unknown']]) {
    const h = harness();
    h.click({route: id.startsWith('circuit') ? 'electric' : id});
    h.selectOutcome(outcome);
    h.click({equipment: id, enabled: 'true'});
    assert.equal(h.run(`equipmentState.devices.${id}.mode`), 'off');
    assert.equal(h.run(`equipmentState.devices.${id}.command.status`), 'pending');
    assert.match(h.main.innerHTML, /Ожидается подтверждение/);
    assert.equal(h.timerFor(id).delay, 1000);
    h.timerFor(id).callback();
    assert.equal(h.run(`equipmentState.devices.${id}.mode`), mode);
    assert.equal(h.run(`equipmentState.devices.${id}.command.status`), outcome);
    assert.equal(h.run(`equipmentState.devices.${id}.actual`), 'unknown');
    assert.match(h.main.innerHTML, /Фактическая работа<\/span><span class="muted">Нет данных/);
  }
});

test('six simultaneous equipment commands and fuel command retain independent timers and replies', () => {
  const h = harness(), replies = new Map();
  for (const {id} of equipment.EQUIPMENT_DEFS) {
    h.selectOutcome(id === 'circuit2' ? 'rejected' : 'accepted');
    h.click({equipment: id, enabled: 'true'});
    replies.set(id, h.timerFor(id));
  }
  h.selectOutcome('timeout'); h.click({heating: 'on'});
  assert.equal(h.run('equipmentTimers.size'), 6);
  const fuelReply = h.timers.get(h.run('commandTimer'));
  for (const [id, reply] of [...replies].reverse()) {
    assert.ok(h.timerFor(id)); reply.callback();
    assert.equal(h.run(`equipmentState.devices.${id}.mode`), id === 'circuit2' ? 'off' : 'enabled');
  }
  assert.equal(h.run('state.heating.command.status'), 'pending');
  fuelReply.callback();
  assert.equal(h.run('state.heating.mode'), 'unknown');
  assert.equal(h.run('equipmentTimers.size'), 0);
  assert.ok(h.read('Object.values(equipmentState.devices).every(device=>device.actual === "unknown")'));
});

test('OFF replacing pending ON ignores old and duplicate callbacks while preserving the new timer', () => {
  const h = harness(); h.click({route: 'diesel'});
  h.click({equipment: 'diesel', enabled: 'true'});
  const onReply = h.timerFor('diesel').callback;
  h.click({equipment: 'diesel', enabled: 'false'});
  const offReply = h.timerFor('diesel');
  assert.equal(h.run('equipmentState.devices.diesel.mode'), 'unknown');
  onReply();
  assert.equal(h.timerFor('diesel'), offReply);
  assert.equal(h.run('equipmentState.devices.diesel.mode'), 'unknown');
  offReply.callback();
  assert.equal(h.run('equipmentState.devices.diesel.mode'), 'off');
  const events = h.run('equipmentState.events.length');
  offReply.callback(); onReply();
  assert.equal(h.run('equipmentState.events.length'), events);
});

test('rejected and timed-out OFF never establish a false stopped equipment state', () => {
  for (const [outcome, mode] of [['rejected', 'enabled'], ['timeout', 'unknown']]) {
    const h = harness(); h.click({route: 'electric'});
    h.click({equipment: 'circuit1', enabled: 'true'}); h.timerFor('circuit1').callback();
    h.selectOutcome(outcome); h.click({equipment: 'circuit1', enabled: 'false'});
    assert.equal(h.run('equipmentState.devices.circuit1.mode'), 'enabled');
    h.timerFor('circuit1').callback();
    assert.equal(h.run('equipmentState.devices.circuit1.mode'), mode);
    assert.equal(h.run('equipmentState.devices.circuit1.actual'), 'unknown');
    assert.equal(h.run('getEquipmentAvailability(equipmentState,"circuit1").canDisable'), true);
    assert.match(h.run('deviceControls("circuit1")'), outcome === 'rejected'
      ? /Подтверждённый режим · демо<\/span><span class="badge success">Разрешён/
      : /Подтверждённый режим · демо<\/span><span class="badge muted">Неизвестен/);
    assert.match(h.main.innerHTML, /Фактическая работа<\/span><span class="muted">Нет данных/);
  }
});

test('reset and scenario change cancel all timers and ignore replies captured before the change', () => {
  for (const change of ['reset', 'scenario']) {
    const h = harness(), oldReplies = [];
    for (const {id} of equipment.EQUIPMENT_DEFS) {
      h.click({equipment: id, enabled: 'true'}); oldReplies.push(h.timerFor(id).callback);
    }
    h.click({heating: 'on'}); oldReplies.push(h.timers.get(h.run('commandTimer')).callback);
    if (change === 'reset') h.click({action: 'reset'}); else h.selectScenario('disconnected');
    assert.equal(h.run('equipmentTimers.size'), 0);
    const before = h.read('[state,equipmentState]');
    for (const reply of oldReplies) reply();
    assert.deepEqual(h.read('[state,equipmentState]'), before);
    assert.ok(h.read('Object.values(equipmentState.devices).every(device=>device.command===null)'));
  }
});

test('equipment warning persists across all pages, routes to the right parent, and survives acknowledgement', () => {
  const h = harness(); h.selectOutcome('rejected');
  h.click({equipment: 'circuit3', enabled: 'true'}); h.timerFor('circuit3').callback();
  const eventId = h.run('equipmentState.events[0].id');
  for (const route of h.read('Object.keys(titles)')) {
    h.run(`route=${JSON.stringify(route)}; render();`);
    assert.match(h.main.innerHTML, /Контур 3: команда отклонена/);
    assert.match(h.main.innerHTML, /data-route="electric">Подробнее/);
  }
  h.click({route: 'equipment-journal'}); h.click({equipmentAck: eventId});
  assert.match(h.main.innerHTML, /Отмечено как прочитанное/);
  assert.match(h.main.innerHTML, /Действующее предупреждение/);
  assert.equal(h.run('getEquipmentAlerts(equipmentState).length'), 1);
});

test('equipment actions never alter the preserved fuel model or its separate event journal', () => {
  const h = harness(); h.selectScenario('low');
  const initial = h.read('state');
  h.click({equipment: 'diesel', enabled: 'true'}); h.timerFor('diesel').callback();
  assert.deepEqual(h.read('state'), initial);
  h.click({route: 'journal'});
  assert.doesNotMatch(h.run('eventList(Infinity,true)'), /Дизельный отопитель/);
  h.click({ack: h.run('state.events[0].id')});
  assert.match(h.main.innerHTML, /Действующее предупреждение/);
  assert.equal(h.run('getLevel(state).id'), 'low');
});

test('fuel pending and reply behavior remains correct with the additional equipment state present', () => {
  for (const [outcome, mode] of [['accepted', 'enabled'], ['rejected', 'off'], ['timeout', 'unknown']]) {
    const h = harness(); h.click({route: 'fuel'}); h.selectOutcome(outcome);
    h.click({heating: 'on'});
    assert.equal(h.run('state.heating.mode'), 'off');
    assert.equal(h.run('state.heating.command.status'), 'pending');
    h.timers.get(h.run('commandTimer')).callback();
    assert.equal(h.run('state.heating.mode'), mode);
    assert.equal(h.run('state.heating.actual'), 'unknown');
    assert.ok(h.read('Object.values(equipmentState.devices).every(device=>device.mode==="off")'));
  }
});

test('primary swipe suppresses its click, while equipment detail swipes cannot navigate or send commands', () => {
  const h = harness(), target = {closest: () => null};
  const gesture = () => {
    h.dispatch(h.main, 'pointerdown', {isPrimary: true, button: 0, pointerId: 1, clientX: 220, clientY: 20, target});
    h.setTime(300);
    h.dispatch(h.main, 'pointerup', {pointerId: 1, clientX: 80, clientY: 30});
  };
  gesture(); assert.equal(h.run('route'), 'controls');
  let prevented = false, stopped = false;
  h.document.listeners.click[0]({detail: 1, preventDefault() { prevented = true; }, stopImmediatePropagation() { stopped = true; }});
  assert.equal(prevented, true); assert.equal(stopped, true);
  for (const route of ['dhw', 'electric', 'diesel', 'equipment-journal']) {
    h.click({route}); const before = h.read('equipmentState');
    h.setTime(0); gesture();
    assert.equal(h.run('route'), route);
    assert.deepEqual(h.read('equipmentState'), before);
  }
});
