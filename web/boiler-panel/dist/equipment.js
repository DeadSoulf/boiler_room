/**
 * Browser-local equipment demonstration, without transport or physical feedback.
 * The three circuits are provisional independent heating branches. No hydraulic
 * connection, priority, power stage, diesel type or protection algorithm is set.
 * A confirmed mode represents a simulated permission, never equipment operation.
 */
import {SCENARIOS, MAX_EVENTS, getReadingStatus} from './model.js';

export const EQUIPMENT_DEFS = Object.freeze([
  Object.freeze({id: 'dhw', label: 'Бойлер косвенного нагрева', kind: 'dhw', group: null}),
  Object.freeze({id: 'electric', label: 'Электрокотёл', kind: 'electric', group: null}),
  Object.freeze({id: 'circuit1', label: 'Контур 1', kind: 'circuit', group: 'electric'}),
  Object.freeze({id: 'circuit2', label: 'Контур 2', kind: 'circuit', group: 'electric'}),
  Object.freeze({id: 'circuit3', label: 'Контур 3', kind: 'circuit', group: 'electric'}),
  Object.freeze({id: 'diesel', label: 'Дизельный отопитель', kind: 'diesel', group: null}),
]);

const DEFINITIONS = new Map(EQUIPMENT_DEFS.map(item => [item.id, item]));
const SCENARIO_IDS = new Set(SCENARIOS.map(item => item.id));
const MODES = new Set(['off', 'enabled', 'unknown']);
let nextSession = 1;
const finiteTime = value => typeof value === 'number' && Number.isFinite(value) && value >= 0;
const timestampFor = (state, now) => now ?? state?.now;
const validTime = (state, now) => finiteTime(now) && finiteTime(state?.now) && now >= state.now;
const knownDevice = (state, id) => DEFINITIONS.has(id)
  && Object.hasOwn(state?.devices || {}, id) && MODES.has(state.devices[id]?.mode);
const validJournal = state => typeof state?.sessionId === 'string' && state.sessionId.length > 0
  && Number.isSafeInteger(state.nextEventId) && state.nextEventId > 0
  && state.nextEventId < Number.MAX_SAFE_INTEGER && Array.isArray(state.events);

function appendEvent(state, {deviceId = null, kind = 'info', title, detail}) {
  const entry = {
    id: `equipment-event:${state.sessionId}:${state.nextEventId}`,
    deviceId, time: state.now, kind, title, detail, acknowledged: false,
  };
  return {...state, nextEventId: state.nextEventId + 1, events: [entry, ...state.events].slice(0, MAX_EVENTS)};
}

export function createEquipmentState(scenario = 'normal', now = Date.now()) {
  if (!finiteTime(now)) throw new TypeError('Time must be a finite non-negative timestamp.');
  const chosen = SCENARIO_IDS.has(scenario) ? scenario : 'normal';
  const disconnected = chosen === 'disconnected';
  const state = {
    scenario: chosen,
    now,
    sessionId: `equipment-${nextSession++}`,
    sequence: 1,
    nextEventId: 1,
    connection: {commandAvailable: !disconnected},
    devices: Object.fromEntries(EQUIPMENT_DEFS.map(({id}) => [id, {
      mode: disconnected ? 'unknown' : 'off', actual: 'unknown', command: null,
    }])),
    readings: {dhw: {
      value: disconnected ? null : 46.0,
      valid: !disconnected,
      updatedAt: disconnected ? null : Math.max(0, now - (chosen === 'stale' ? 75_000 : 0)),
    }},
    events: [],
  };
  return appendEvent(state, {
    title: 'Оборудование: демонстрация',
    detail: 'Показания и ответы — примеры. Фактическое состояние оборудования неизвестно.',
  });
}

export function selectEquipmentScenario(state, scenario, now = Date.now()) {
  if (!SCENARIO_IDS.has(scenario) || !validTime(state, now) || !validJournal(state)) return state;
  const next = createEquipmentState(scenario, now);
  return appendEvent({...next, events: state.events, nextEventId: state.nextEventId}, {
    title: 'Оборудование: сценарий обновлён',
    detail: 'Выбран демонстрационный сценарий. Ожидавшие команды отменены в макете.',
  });
}

export function getEquipmentAvailability(state, id) {
  const unavailable = reason => ({canEnable: false, canDisable: false, reason});
  if (!knownDevice(state, id)) return unavailable('Нет данных об устройстве');
  if (state.connection?.commandAvailable !== true) return unavailable('Канал управления не подключён');
  const device = state.devices[id];
  if (device.command?.status === 'pending') {
    return device.command.requested === 'enabled'
      ? {canEnable: false, canDisable: true, reason: 'Ожидается подтверждение включения'}
      : unavailable('Ожидается подтверждение выключения');
  }
  if (device.mode === 'unknown') return {
    canEnable: false, canDisable: true, reason: 'Режим неизвестен — можно запросить выключение',
  };
  // These permissions are UI examples. They do not implement equipment interlocks.
  return {canEnable: device.mode === 'off', canDisable: device.mode === 'enabled', reason: ''};
}

export function requestEquipment(state, id, enabled, now) {
  const timestamp = timestampFor(state, now);
  if (!knownDevice(state, id) || typeof enabled !== 'boolean' || !validTime(state, timestamp)
      || !Number.isSafeInteger(state.sequence) || state.sequence < 1
      || state.sequence >= Number.MAX_SAFE_INTEGER || !validJournal(state)) {
    return {state, ok: false, message: 'Некорректный запрос оборудования'};
  }
  const availability = getEquipmentAvailability(state, id);
  if (!(enabled ? availability.canEnable : availability.canDisable)) return {
    state, ok: false,
    message: availability.reason || (enabled ? 'Режим уже разрешён' : 'Режим уже отключён'),
  };
  const device = state.devices[id];
  const requested = enabled ? 'enabled' : 'off';
  const requestId = `${state.sessionId}:${id}:${state.sequence}`;
  const supersedesOn = !enabled && device.command?.status === 'pending' && device.command.requested === 'enabled';
  const message = enabled ? 'Ожидается подтверждение включения' : 'Ожидается подтверждение выключения';
  const next = {
    ...state, now: timestamp, sequence: state.sequence + 1,
    devices: {...state.devices, [id]: {
      ...device,
      mode: supersedesOn ? 'unknown' : device.mode,
      actual: 'unknown',
      command: {id: requestId, requested, status: 'pending', requestedAt: timestamp, resolvedAt: null, message},
    }},
  };
  return {
    state: appendEvent(next, {
      deviceId: id,
      title: `${DEFINITIONS.get(id).label}: запрос ${enabled ? 'включения' : 'выключения'}`,
      detail: 'Демонстрационная команда. Фактическое состояние оборудования неизвестно.',
    }),
    ok: true, requestId, message: `${message} · демо`,
  };
}

export function resolveEquipment(state, requestId, outcome, now) {
  const definition = typeof requestId === 'string' && EQUIPMENT_DEFS.find(({id}) => knownDevice(state, id)
    && state.devices[id].command?.id === requestId && state.devices[id].command?.status === 'pending');
  if (!definition) return {
    state, ok: false, ignored: true, message: 'Ответ не относится к ожидающей команде',
  };
  const timestamp = timestampFor(state, now);
  const device = state.devices[definition.id];
  if (!['accepted', 'rejected', 'timeout'].includes(outcome) || !validTime(state, timestamp)
      || !finiteTime(device.command.requestedAt) || timestamp < device.command.requestedAt
      || !['enabled', 'off'].includes(device.command.requested) || !validJournal(state)) return {
    state, ok: false, ignored: false, message: 'Некорректный ответ оборудования',
  };
  const accepted = outcome === 'accepted';
  const enabling = device.command.requested === 'enabled';
  const message = accepted
    ? `Режим ${enabling ? 'разрешён' : 'отключён'} · подтверждение в демо`
    : outcome === 'rejected'
      ? `Запрос ${enabling ? 'включения' : 'выключения'} отклонён · демо`
      : `${enabling ? 'Включение' : 'Выключение'} не подтверждено. Результат неизвестен · демо`;
  const mode = accepted ? device.command.requested : outcome === 'timeout' ? 'unknown' : device.mode;
  const next = {
    ...state, now: timestamp,
    devices: {...state.devices, [definition.id]: {
      ...device, mode, actual: 'unknown',
      command: {...device.command, status: outcome, resolvedAt: timestamp, message},
    }},
  };
  return {
    state: appendEvent(next, {
      deviceId: definition.id,
      kind: accepted ? 'info' : 'warning',
      title: `${definition.label}: ${accepted ? 'режим подтверждён' : outcome === 'rejected' ? 'команда отклонена' : 'нет подтверждения'}`,
      detail: `${message}. Фактическое состояние оборудования неизвестно.`,
    }),
    ok: accepted, ignored: false, message,
  };
}

export function getEquipmentAlerts(state) {
  if (!state?.devices) return [];
  const alerts = [];
  if (state.connection?.commandAvailable !== true) alerts.push({
    id: 'equipment-connection', deviceId: null, kind: 'warning',
    title: 'Оборудование: нет подключения', detail: 'Команды недоступны. Состояния устройств неизвестны.',
  });
  else {
    const readingStatus = getReadingStatus(state.readings?.dhw, state.now);
    if (readingStatus !== 'fresh') alerts.push({
      id: `equipment-dhw-${readingStatus}`, deviceId: 'dhw', kind: 'warning',
      title: readingStatus === 'stale' ? 'Бойлер: температура устарела' : 'Бойлер: нет температуры',
      detail: 'Актуальная температура воды неизвестна.',
    });
  }
  for (const {id, label} of EQUIPMENT_DEFS) {
    const command = state.devices[id]?.command;
    if (['rejected', 'timeout'].includes(command?.status)) alerts.push({
      id: `equipment-${id}-${command.status}`, deviceId: id, kind: 'warning',
      title: `${label}: ${command.status === 'timeout' ? 'нет подтверждения' : 'команда отклонена'}`,
      detail: command.message,
    });
  }
  return alerts;
}

export function ackEquipmentEvent(state, id) {
  if (!Array.isArray(state?.events) || !state.events.some(entry => entry.id === id && !entry.acknowledged)) return state;
  return {...state, events: state.events.map(entry => entry.id === id ? {...entry, acknowledged: true} : entry)};
}
