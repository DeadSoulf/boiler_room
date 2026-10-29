import test from 'node:test';
import assert from 'node:assert/strict';
import {SCENARIOS, MAX_EVENTS, FRESHNESS_MS, getReadingStatus} from '../dist/model.js';
import {
  EQUIPMENT_DEFS, createEquipmentState, selectEquipmentScenario,
  getEquipmentAvailability, requestEquipment, resolveEquipment,
  getEquipmentAlerts, ackEquipmentEvent,
} from '../dist/equipment.js';

const NOW = Date.UTC(2026, 9, 29, 9, 0, 0);

test('equipment contains the requested independent devices and exactly three electric circuits', () => {
  const state = createEquipmentState('normal', NOW);
  assert.deepEqual(EQUIPMENT_DEFS.map(item => item.id), ['dhw', 'electric', 'circuit1', 'circuit2', 'circuit3', 'diesel']);
  assert.deepEqual(EQUIPMENT_DEFS.filter(item => item.group === 'electric').map(item => item.label), ['Контур 1', 'Контур 2', 'Контур 3']);
  assert.ok(Object.values(state.devices).every(device => device.mode === 'off' && device.actual === 'unknown' && device.command === null));
  assert.deepEqual(Object.keys(state.readings), ['dhw']);
  assert.equal(state.readings.dhw.value, 46);
  assert.equal(getReadingStatus(state.readings.dhw, NOW), 'fresh');
  assert.deepEqual(getEquipmentAlerts(state), []);
});

test('all seven existing scenarios are accepted without borrowing fuel state for equipment', () => {
  for (const {id} of SCENARIOS) {
    const state = createEquipmentState(id, NOW);
    assert.equal(state.scenario, id);
    assert.ok(Object.values(state.devices).every(device => device.actual === 'unknown'));
    assert.ok(Object.values(state.devices).every(device => device.mode === (id === 'disconnected' ? 'unknown' : 'off')));
  }
  assert.equal(createEquipmentState('invalid', NOW).scenario, 'normal');
  assert.throws(() => createEquipmentState('normal', NaN), TypeError);
});

test('missing and stale DHW temperatures stay distinguishable and never become zero', () => {
  const stale = createEquipmentState('stale', NOW);
  assert.equal(getReadingStatus(stale.readings.dhw, NOW), 'stale');
  assert.equal(stale.readings.dhw.updatedAt, NOW - 75_000);
  assert.equal(getEquipmentAlerts(stale)[0].deviceId, 'dhw');
  const missing = createEquipmentState('disconnected', NOW);
  assert.equal(missing.readings.dhw.value, null);
  assert.equal(getReadingStatus(missing.readings.dhw, NOW), 'no-data');
  for (const {id} of EQUIPMENT_DEFS) {
    assert.equal(getEquipmentAvailability(missing, id).canEnable, false);
    assert.equal(getEquipmentAvailability(missing, id).canDisable, false);
    assert.equal(requestEquipment(missing, id, true, NOW).state, missing);
  }
  const normal = createEquipmentState('normal', NOW);
  normal.readings.dhw.value = NaN;
  assert.match(getEquipmentAlerts(normal)[0].title, /нет температуры/);
  normal.readings.dhw = {value: 46, valid: true, updatedAt: NOW - FRESHNESS_MS};
  assert.deepEqual(getEquipmentAlerts(normal), []);
  normal.now += 1;
  assert.match(getEquipmentAlerts(normal)[0].title, /устарела/);
});

test('ON and OFF change only the selected simulated permission after acknowledgement', () => {
  for (const {id} of EQUIPMENT_DEFS) {
    const original = createEquipmentState('normal', NOW);
    const snapshot = structuredClone(original);
    const requested = requestEquipment(original, id, true, NOW);
    assert.equal(requested.ok, true);
    assert.equal(requested.state.devices[id].mode, 'off');
    assert.equal(requested.state.devices[id].command.status, 'pending');
    assert.equal(requested.state.devices[id].command.resolvedAt, null);
    const enabled = resolveEquipment(requested.state, requested.requestId, 'accepted', NOW);
    assert.equal(enabled.ok, true);
    assert.equal(enabled.state.devices[id].mode, 'enabled');
    assert.equal(enabled.state.devices[id].actual, 'unknown');
    assert.deepEqual(enabled.state.readings, original.readings);
    for (const other of EQUIPMENT_DEFS.filter(item => item.id !== id)) assert.deepEqual(enabled.state.devices[other.id], original.devices[other.id]);
    const off = requestEquipment(enabled.state, id, false, NOW);
    assert.equal(off.state.devices[id].mode, 'enabled');
    const disabled = resolveEquipment(off.state, off.requestId, 'accepted', NOW);
    assert.equal(disabled.state.devices[id].mode, 'off');
    assert.equal(disabled.state.devices[id].actual, 'unknown');
    assert.deepEqual(original, snapshot);
  }
});

test('concurrent requests and out-of-order replies remain isolated across devices', () => {
  let state = createEquipmentState('normal', NOW);
  const ids = [];
  for (const {id} of EQUIPMENT_DEFS) {
    const result = requestEquipment(state, id, true, NOW);
    state = result.state;
    ids.push([id, result.requestId]);
    assert.equal(result.ok, true);
  }
  assert.equal(new Set(ids.map(item => item[1])).size, 6);
  for (const [id, requestId] of ids.reverse()) {
    state = resolveEquipment(state, requestId, 'accepted', NOW).state;
    assert.equal(state.devices[id].mode, 'enabled');
  }
  assert.ok(Object.values(state.devices).every(device => device.actual === 'unknown'));
});

test('pending ON can be replaced by OFF without confirming off or accepting the late ON reply', () => {
  for (const outcome of ['accepted', 'rejected', 'timeout']) {
    const on = requestEquipment(createEquipmentState('normal', NOW), 'diesel', true, NOW);
    assert.equal(getEquipmentAvailability(on.state, 'diesel').canDisable, true);
    const off = requestEquipment(on.state, 'diesel', false, NOW);
    assert.notEqual(off.requestId, on.requestId);
    assert.equal(off.state.devices.diesel.mode, 'unknown');
    assert.equal(getEquipmentAvailability(off.state, 'diesel').canDisable, false);
    assert.equal(getEquipmentAvailability(off.state, 'diesel').canEnable, false);
    const late = resolveEquipment(off.state, on.requestId, 'accepted', NOW);
    assert.equal(late.ignored, true);
    assert.equal(late.state, off.state);
    const settled = resolveEquipment(off.state, off.requestId, outcome, NOW);
    assert.equal(settled.state.devices.diesel.mode, outcome === 'accepted' ? 'off' : 'unknown');
    assert.equal(settled.state.devices.diesel.actual, 'unknown');
  }
});

test('duplicate pending requests do not add commands or journal records', () => {
  const on = requestEquipment(createEquipmentState('normal', NOW), 'circuit2', true, NOW);
  const repeatedOn = requestEquipment(on.state, 'circuit2', true, NOW);
  assert.equal(repeatedOn.ok, false);
  assert.equal(repeatedOn.state, on.state);
  const off = requestEquipment(on.state, 'circuit2', false, NOW);
  for (const enabled of [true, false]) {
    const repeated = requestEquipment(off.state, 'circuit2', enabled, NOW);
    assert.equal(repeated.ok, false);
    assert.equal(repeated.state, off.state);
  }
  assert.equal(getEquipmentAvailability(off.state, 'circuit3').canEnable, true);
});

test('rejected requests preserve confirmed permission and acknowledgement does not clear their warning', () => {
  for (const enabled of [true, false]) {
    let state = createEquipmentState('normal', NOW);
    if (!enabled) state = {...state, devices: {...state.devices, dhw: {...state.devices.dhw, mode: 'enabled'}}};
    const requested = requestEquipment(state, 'dhw', enabled, NOW);
    const rejected = resolveEquipment(requested.state, requested.requestId, 'rejected', NOW);
    assert.equal(rejected.ok, false);
    assert.equal(rejected.state.devices.dhw.mode, state.devices.dhw.mode);
    assert.equal(rejected.state.devices.dhw.actual, 'unknown');
    assert.equal(getEquipmentAlerts(rejected.state).some(alert => alert.id === 'equipment-dhw-rejected'), true);
    const acknowledged = ackEquipmentEvent(rejected.state, rejected.state.events[0].id);
    assert.equal(acknowledged.events[0].acknowledged, true);
    assert.equal(rejected.state.events[0].acknowledged, false);
    assert.deepEqual(getEquipmentAlerts(acknowledged), getEquipmentAlerts(rejected.state));
    assert.equal(requestEquipment(rejected.state, 'dhw', enabled, NOW).ok, true);
  }
});

test('timeout never confirms operation or stop; unknown mode still permits an available OFF request', () => {
  for (const enabled of [true, false]) {
    let state = createEquipmentState('normal', NOW);
    state.devices.electric.mode = enabled ? 'off' : 'enabled';
    const requested = requestEquipment(state, 'electric', enabled, NOW);
    const timeout = resolveEquipment(requested.state, requested.requestId, 'timeout', NOW);
    assert.equal(timeout.state.devices.electric.mode, 'unknown');
    assert.equal(timeout.state.devices.electric.actual, 'unknown');
    assert.equal(getEquipmentAvailability(timeout.state, 'electric').canEnable, false);
    assert.equal(getEquipmentAvailability(timeout.state, 'electric').canDisable, true);
    assert.match(getEquipmentAlerts(timeout.state)[0].title, /нет подтверждения/);
    timeout.state.readings.dhw.valid = false;
    const off = requestEquipment(timeout.state, 'electric', false, NOW);
    assert.equal(off.ok, true);
    assert.equal(resolveEquipment(off.state, off.requestId, 'accepted', NOW).state.devices.electric.mode, 'off');
    timeout.state.connection.commandAvailable = false;
    assert.equal(getEquipmentAvailability(timeout.state, 'electric').canDisable, false);
  }
});

test('settled, unknown and prior-session replies cannot change state or append events', () => {
  for (const outcome of ['accepted', 'rejected', 'timeout']) {
    const requested = requestEquipment(createEquipmentState('normal', NOW), 'circuit1', true, NOW);
    const settled = resolveEquipment(requested.state, requested.requestId, outcome, NOW);
    for (const requestId of [requested.requestId, 'missing', undefined]) {
      const late = resolveEquipment(settled.state, requestId, 'accepted', NOW);
      assert.equal(late.ignored, true);
      assert.equal(late.state, settled.state);
    }
    const reset = requestEquipment(createEquipmentState('normal', NOW), 'circuit1', true, NOW);
    assert.notEqual(reset.requestId, requested.requestId);
    assert.equal(resolveEquipment(reset.state, requested.requestId, 'accepted', NOW).state, reset.state);
    const switched = selectEquipmentScenario(requested.state, 'normal', NOW);
    assert.equal(switched.devices.circuit1.command, null);
    const afterSwitch = requestEquipment(switched, 'circuit1', true, NOW);
    assert.notEqual(afterSwitch.requestId, requested.requestId);
    assert.equal(resolveEquipment(afterSwitch.state, requested.requestId, 'accepted', NOW).ignored, true);
  }
});

test('invalid commands, devices, times, and replies leave state intact', () => {
  const state = createEquipmentState('normal', NOW);
  for (const id of ['absent', '__proto__', null]) {
    assert.equal(getEquipmentAvailability(state, id).canEnable, false);
    assert.equal(requestEquipment(state, id, true, NOW).state, state);
  }
  for (const enabled of ['true', null, 1]) assert.equal(requestEquipment(state, 'dhw', enabled, NOW).state, state);
  for (const stamp of [NaN, Infinity, -1, NOW - 1]) assert.equal(requestEquipment(state, 'dhw', true, stamp).state, state);
  assert.equal(requestEquipment(undefined, 'dhw', true, NOW).ok, false);
  const pending = requestEquipment(state, 'dhw', true, NOW);
  for (const outcome of ['success', null, true]) {
    const invalid = resolveEquipment(pending.state, pending.requestId, outcome, NOW);
    assert.equal(invalid.state, pending.state);
    assert.equal(invalid.ignored, false);
  }
  for (const stamp of [NaN, Infinity, -1, NOW - 1]) assert.equal(resolveEquipment(pending.state, pending.requestId, 'accepted', stamp).state, pending.state);
  assert.equal(selectEquipmentScenario(state, 'missing', NOW), state);
  assert.equal(selectEquipmentScenario(state, 'normal', NaN), state);
  assert.equal(ackEquipmentEvent(state, 'missing'), state);
});

test('scenario changes preserve the bounded journal with unique IDs, without inventing cross-device rules', () => {
  let state = createEquipmentState('normal', NOW);
  for (let i = 0; i < MAX_EVENTS + 15; i++) state = selectEquipmentScenario(state, i % 2 ? 'normal' : 'upper', NOW + i);
  assert.equal(state.events.length, MAX_EVENTS);
  assert.equal(new Set(state.events.map(entry => entry.id)).size, MAX_EVENTS);
  assert.ok(state.events.every((entry, index, entries) => index === 0 || entry.time <= entries[index - 1].time));
  assert.equal(getEquipmentAvailability(createEquipmentState('stale', NOW), 'circuit1').canEnable, true);
  assert.equal(getEquipmentAvailability(createEquipmentState('low', NOW), 'diesel').canEnable, true);
  assert.equal(getEquipmentAvailability(createEquipmentState('inconsistent', NOW), 'electric').canEnable, true);
});
