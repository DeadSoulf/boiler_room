import test from 'node:test';
import assert from 'node:assert/strict';
import {
  FRESHNESS_MS, MAX_EVENTS, SENSOR_DEFS, SCENARIOS, createState, selectScenario,
  getLevel, getReadingStatus, getHeatingAvailability, setHeating, ackEvent,
  getAlerts, getTemperatureHistory, formatAge,
} from '../dist/model.js';

const NOW = Date.UTC(2026, 9, 29, 9, 0, 0);
const copy = (value) => structuredClone(value);

test('four normalized combinations describe a range, never a percentage', () => {
  const cases = [
    [false, false, 'low'], [true, false, 'between'],
    [true, true, 'upper'], [false, true, 'inconsistent'],
  ];
  for (const [lower, upper, expected] of cases) {
    const state = createState('normal', NOW);
    state.lower.reached = lower;
    state.upper.reached = upper;
    const result = getLevel(state);
    assert.equal(result.id, expected);
    assert.equal(result.fresh, true);
    assert.doesNotMatch(result.label + result.description, /%|литр/i);
  }
});

test('unknown inputs take precedence over stale or contradictory inputs', () => {
  const state = createState('inconsistent', NOW);
  state.lower.updatedAt = NOW - FRESHNESS_MS - 1;
  assert.equal(getLevel(state).id, 'stale');
  state.upper.valid = false;
  assert.equal(getLevel(state).id, 'no-data');
  state.upper.valid = true;
  state.upper.reached = null;
  assert.equal(getLevel(state).id, 'no-data');
});

test('freshness boundary is inclusive; future, absent, and nonfinite readings are invalid', () => {
  const reading = { value: 36, valid: true, updatedAt: NOW - FRESHNESS_MS };
  assert.equal(getReadingStatus(reading, NOW), 'fresh');
  assert.equal(getReadingStatus(reading, NOW + 1), 'stale');
  assert.equal(getReadingStatus({ ...reading, updatedAt: NOW + 1 }, NOW), 'no-data');
  assert.equal(getReadingStatus({ ...reading, value: NaN }, NOW), 'no-data');
  assert.equal(getReadingStatus({ ...reading, updatedAt: null }, NOW), 'no-data');
  assert.equal(getReadingStatus(undefined, NOW), 'no-data');
  assert.equal(getLevel({}).id, 'no-data');
});

test('all exposed scenarios are coherent and all disconnected readings are unknown', () => {
  assert.equal(SENSOR_DEFS.length, 4);
  for (const { id } of SCENARIOS) {
    const state = createState(id, NOW);
    assert.equal(state.scenario, id);
    assert.equal(state.heating.actual, 'unknown');
    assert.ok(getLevel(state).label);
    assert.ok(state.events.every((entry) => entry.time <= NOW));
  }
  const disconnected = createState('disconnected', NOW);
  assert.equal(getLevel(disconnected).id, 'no-data');
  for (const { id } of SENSOR_DEFS) assert.equal(disconnected.telemetry[id].value, null);
  assert.equal(disconnected.heating.mode, 'unknown');
  assert.equal(getHeatingAvailability(disconnected).canEnable, false);
  assert.equal(getHeatingAvailability(disconnected).canDisable, false);
});

test('demo enable requires fresh fuel temperature and usable consistent levels', () => {
  for (const scenario of ['normal', 'upper']) assert.equal(getHeatingAvailability(createState(scenario, NOW)).canEnable, true);
  for (const scenario of ['low', 'inconsistent', 'stale', 'disconnected']) {
    const state = createState(scenario, NOW);
    assert.equal(getHeatingAvailability(state).canEnable, false);
    const result = setHeating(state, true, NOW);
    assert.equal(result.ok, false);
    assert.equal(result.state.heating.mode, state.heating.mode);
    assert.equal(result.state.events.length, state.events.length);
  }
  const noTemperature = createState('normal', NOW);
  noTemperature.telemetry.fuel.value = null;
  assert.equal(getHeatingAvailability(noTemperature).canEnable, false);
  const noMode = createState('normal', NOW);
  noMode.heating.mode = 'unknown';
  assert.equal(getHeatingAvailability(noMode).canEnable, false);
});

test('available disable survives invalid levels and temperature, but not a missing command channel', () => {
  for (const scenario of ['low', 'inconsistent', 'stale']) {
    const state = createState(scenario, NOW);
    state.heating.mode = 'enabled';
    state.telemetry.fuel.valid = false;
    assert.equal(getHeatingAvailability(state).canDisable, true);
    assert.equal(setHeating(state, false, NOW).state.heating.mode, 'off');
  }
  const state = createState('heating', NOW);
  state.connection.commandAvailable = false;
  assert.equal(getHeatingAvailability(state).canDisable, false);
  assert.equal(setHeating(state, false, NOW).ok, false);
});

test('ON and OFF are immutable demo permissions, never heater feedback or auto-regulation', () => {
  const original = createState('normal', NOW);
  const saved = copy(original);
  const on = setHeating(original, true, NOW);
  assert.equal(on.ok, true);
  assert.equal(on.state.heating.mode, 'enabled');
  assert.equal(on.state.heating.actual, 'unknown');
  assert.deepEqual(original, saved);
  assert.deepEqual(on.state.telemetry, saved.telemetry);
  assert.equal(on.state.events.length, original.events.length + 1);
  assert.match(on.state.events[0].detail, /Демонстрационная/);
  assert.equal(setHeating(on.state, true, NOW).ok, false);
  const off = setHeating(on.state, false, NOW);
  assert.equal(off.ok, true);
  assert.equal(off.state.heating.mode, 'off');
  assert.equal(off.state.heating.actual, 'unknown');
  const above80 = createState('normal', NOW);
  above80.telemetry.fuel.value = 81;
  assert.equal(getHeatingAvailability(above80).canEnable, true, '80 is descriptive, no hardware control threshold is invented');
  assert.equal(setHeating(original, 'true', NOW).ok, false);
});

test('commands re-evaluate freshness at an explicitly supplied new time', () => {
  const original = createState('normal', NOW);
  const result = setHeating(original, true, NOW + FRESHNESS_MS + 1);
  assert.equal(result.ok, false);
  assert.equal(getLevel(result.state).id, 'stale');
  assert.equal(original.now, NOW);
});

test('scenario selection preserves the session journal without mutating old states', () => {
  const original = createState('normal', NOW);
  const snapshot = copy(original);
  const next = selectScenario(original, 'low', NOW + 1_000);
  assert.equal(getLevel(next).id, 'low');
  assert.equal(next.startedAt, original.startedAt);
  assert.equal(next.events.length, original.events.length + 1);
  assert.deepEqual(original, snapshot);
  assert.equal(selectScenario(original, 'invalid', NOW), original);
});

test('acknowledgement affects only the selected event and does not clear a live condition', () => {
  const original = createState('low', NOW);
  const acknowledged = ackEvent(original, original.events[0].id);
  assert.equal(acknowledged.events[0].acknowledged, true);
  assert.equal(original.events[0].acknowledged, false);
  assert.equal(acknowledged.events[1].acknowledged, false);
  assert.equal(getAlerts(acknowledged)[0].id, 'level-low');
  assert.equal(ackEvent(original, 'absent'), original);
});

test('bounded journal uses unique stable IDs after truncation', () => {
  let state = createState('normal', NOW);
  for (let i = 0; i < MAX_EVENTS + 20; i++) state = selectScenario(state, i % 2 ? 'normal' : 'upper', NOW + i);
  assert.equal(state.events.length, MAX_EVENTS);
  assert.equal(new Set(state.events.map((entry) => entry.id)).size, MAX_EVENTS);
  assert.ok(state.events.every((entry, index, entries) => index === 0 || entry.time <= entries[index - 1].time));
});

test('histories are deterministic finite samples with valid times and exact current endpoint', () => {
  for (const scenario of ['normal', 'heating']) {
    const state = createState(scenario, NOW);
    for (const { id } of SENSOR_DEFS) for (const period of ['1h', '6h', '24h']) {
      const points = getTemperatureHistory(state, id, period);
      assert.equal(points.length, 61);
      assert.deepEqual(points, getTemperatureHistory(state, id, period));
      assert.equal(points.at(-1).value, state.telemetry[id].value);
      assert.equal(points.at(-1).time, NOW);
      assert.ok(points.every((point, index) => Number.isFinite(point.value) && point.time >= 0 && point.time <= NOW && (index === 0 || point.time > points[index - 1].time)));
    }
  }
});

test('stale history ends in the past, disconnected and invalid history is empty', () => {
  const stale = createState('stale', NOW);
  assert.equal(getTemperatureHistory(stale, 'fuel').at(-1).time, NOW - 75_000);
  assert.deepEqual(getTemperatureHistory(createState('disconnected', NOW), 'fuel'), []);
  assert.deepEqual(getTemperatureHistory(stale, 'unknown'), []);
  stale.telemetry.fuel.valid = false;
  assert.deepEqual(getTemperatureHistory(stale, 'fuel'), []);
});

test('alerts retain condition semantics and age labels handle missing inputs', () => {
  assert.deepEqual(getAlerts(createState('normal', NOW)), []);
  assert.equal(getAlerts(createState('inconsistent', NOW))[0].kind, 'danger');
  assert.match(getAlerts(createState('inconsistent', NOW))[0].detail, /не диагноз/);
  assert.equal(getAlerts(createState('stale', NOW)).length, 2);
  assert.equal(getAlerts(createState('disconnected', NOW)).length, 1);
  assert.equal(formatAge(0), 'только что');
  assert.equal(formatAge(75_000), '1 мин назад');
  assert.equal(formatAge(3_600_000), '1 ч назад');
  assert.equal(formatAge(null), 'нет данных');
  assert.equal(formatAge(-1), 'нет данных');
  assert.throws(() => createState('normal', NaN), TypeError);
});
