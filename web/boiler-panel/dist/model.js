/**
 * Browser-local demonstration model. No device transport or hardware control.
 * The ON prerequisites below are provisional UI demo rules, not an equipment
 * protection system. «До 80 °C» is descriptive: no regulator, trip limit or
 * hysteresis is inferred from it. Permission and actual heater state differ.
 */
export const FRESHNESS_MS = 30_000;
export const MAX_EVENTS = 100;

export const SENSOR_DEFS = Object.freeze([
  Object.freeze({ id: 'room', label: 'В котельной', unit: '°C' }),
  Object.freeze({ id: 'outdoor', label: 'На улице', unit: '°C' }),
  Object.freeze({ id: 'coolant', label: 'Теплоноситель', unit: '°C' }),
  Object.freeze({ id: 'fuel', label: 'Топливо', unit: '°C' }),
]);

export const SCENARIOS = Object.freeze([
  Object.freeze({ id: 'normal', label: 'Рабочий диапазон', description: 'Нижний уровень достигнут. Подогрев отключён.' }),
  Object.freeze({ id: 'heating', label: 'Подогрев разрешён', description: 'Пример включённого режима. Фактическая работа нагревателя не подтверждена.' }),
  Object.freeze({ id: 'upper', label: 'Верхний уровень', description: 'Оба нормализованных порога достигнуты.' }),
  Object.freeze({ id: 'low', label: 'Мало топлива', description: 'Ни один из двух порогов не достигнут.' }),
  Object.freeze({ id: 'inconsistent', label: 'Проверьте датчики', description: 'Верхний порог достигнут, нижний — нет. Сигналы не согласуются.' }),
  Object.freeze({ id: 'stale', label: 'Данные устарели', description: 'Последние показания получены 75 секунд назад.' }),
  Object.freeze({ id: 'disconnected', label: 'Нет подключения', description: 'Показания и канал управления недоступны.' }),
]);

const KNOWN_SCENARIOS = new Set(SCENARIOS.map(({ id }) => id));
const KNOWN_SENSORS = new Set(SENSOR_DEFS.map(({ id }) => id));
const finiteTime = (value) => typeof value === 'number' && Number.isFinite(value) && value >= 0;
const clock = (value) => {
  if (!finiteTime(value)) throw new TypeError('Time must be a finite non-negative timestamp.');
  return value;
};
const snapshotNow = (state, now) => clock(now ?? state?.now ?? Date.now());

export function getReadingStatus(reading, now, key = 'value') {
  const valueKnown = key === 'reached'
    ? typeof reading?.reached === 'boolean'
    : typeof reading?.value === 'number' && Number.isFinite(reading.value);
  if (!reading || reading.valid !== true || !valueKnown || !finiteTime(now)
      || !finiteTime(reading.updatedAt) || reading.updatedAt > now) return 'no-data';
  return now - reading.updatedAt > FRESHNESS_MS ? 'stale' : 'fresh';
}

function event(state, { time = state.now, kind = 'info', title, detail = '' }) {
  const sequence = Number.isSafeInteger(state.nextEventId) ? state.nextEventId : 1;
  const entry = { id: `event-${sequence}`, time, kind, title, detail, acknowledged: false };
  return {
    ...state,
    nextEventId: sequence + 1,
    events: [entry, ...(state.events || [])].slice(0, MAX_EVENTS),
  };
}

function scenarioSnapshot(scenario, now) {
  const disconnected = scenario === 'disconnected';
  const updatedAt = disconnected ? null : Math.max(0, now - (scenario === 'stale' ? 75_000 : 0));
  const values = { room: 24.6, outdoor: 7.2, coolant: 68.4, fuel: scenario === 'heating' ? 62.5 : 36.0 };
  const telemetry = Object.fromEntries(SENSOR_DEFS.map(({ id }) => [id, {
    value: disconnected ? null : values[id], valid: !disconnected, updatedAt,
  }]));
  return {
    scenario,
    telemetry,
    lower: { reached: disconnected ? null : !['low', 'inconsistent'].includes(scenario), valid: !disconnected, updatedAt },
    upper: { reached: disconnected ? null : ['upper', 'inconsistent'].includes(scenario), valid: !disconnected, updatedAt },
    connection: { connected: !disconnected, commandAvailable: !disconnected },
    heating: { mode: disconnected ? 'unknown' : scenario === 'heating' ? 'enabled' : 'off', actual: 'unknown' },
    now,
  };
}

export function createState(scenario = 'normal', now = Date.now()) {
  clock(now);
  const chosen = KNOWN_SCENARIOS.has(scenario) ? scenario : 'normal';
  let state = {
    ...scenarioSnapshot(chosen, now),
    startedAt: Math.max(0, now - 5 * 60_000),
    events: [],
    nextEventId: 1,
  };
  state = event(state, {
    time: state.startedAt,
    title: 'Демонстрация запущена',
    detail: 'Показания — примеры. Оборудование не подключено.',
  });
  return event(state, {
    title: SCENARIOS.find(({ id }) => id === chosen).label,
    detail: 'Загружен демонстрационный сценарий.',
    kind: ['low', 'stale'].includes(chosen) ? 'warning' : chosen === 'inconsistent' ? 'danger' : 'info',
  });
}

export function selectScenario(state, scenario, now = Date.now()) {
  clock(now);
  if (!KNOWN_SCENARIOS.has(scenario)) return state;
  return event({ ...state, ...scenarioSnapshot(scenario, now) }, {
    title: SCENARIOS.find(({ id }) => id === scenario).label,
    detail: 'Выбран демонстрационный сценарий.',
    kind: ['low', 'stale'].includes(scenario) ? 'warning' : scenario === 'inconsistent' ? 'danger' : 'info',
  });
}

export function getLevel(state) {
  const lowerStatus = getReadingStatus(state?.lower, state?.now, 'reached');
  const upperStatus = getReadingStatus(state?.upper, state?.now, 'reached');
  if ([lowerStatus, upperStatus].includes('no-data')) return {
    id: 'no-data', label: 'Нет данных', description: 'Нужны два достоверных показания.', tone: 'muted', fresh: false,
  };
  if ([lowerStatus, upperStatus].includes('stale')) return {
    id: 'stale', label: 'Данные устарели', description: 'Нет актуального снимка двух уровней.', tone: 'warning', fresh: false,
  };
  if (state.upper.reached && !state.lower.reached) return {
    id: 'inconsistent', label: 'Проверьте датчики', description: 'Верхний достигнут, нижний — нет.', tone: 'danger', fresh: true,
  };
  if (state.upper.reached) return {
    id: 'upper', label: 'Верхний достигнут', description: 'Оба порога уровня подтверждены.', tone: 'info', fresh: true,
  };
  if (state.lower.reached) return {
    id: 'between', label: 'Рабочий диапазон', description: 'Нижний достигнут · верхний ещё нет.', tone: 'success', fresh: true,
  };
  return {
    id: 'low', label: 'Ниже нижнего', description: 'Нижний порог не достигнут.', tone: 'warning', fresh: true,
  };
}

export function getHeatingAvailability(state) {
  const unavailable = (reason) => ({ canEnable: false, canDisable: false, reason });
  if (state?.connection?.connected !== true || state?.connection?.commandAvailable !== true) {
    return unavailable('Управление не подключено');
  }
  if (!['off', 'enabled'].includes(state?.heating?.mode)) return unavailable('Режим подогрева неизвестен');
  // Loss of level or temperature validity must never hide an available OFF action.
  if (state.heating.mode === 'enabled') return { canEnable: false, canDisable: true, reason: '' };
  if (getReadingStatus(state?.telemetry?.fuel, state?.now) !== 'fresh') {
    return unavailable('Нет актуальной температуры топлива');
  }
  const level = getLevel(state);
  if (level.id === 'no-data') return unavailable('Нет достоверных показаний уровня');
  if (level.id === 'stale') return unavailable('Показания уровня устарели');
  if (level.id === 'inconsistent') return unavailable('Показания датчиков не согласуются');
  if (level.id === 'low') return unavailable('Демо: включение недоступно при низком уровне');
  return { canEnable: true, canDisable: false, reason: '' };
}

export function setHeating(state, enabled, now) {
  const timestamp = snapshotNow(state, now);
  if (typeof enabled !== 'boolean') return { state, ok: false, message: 'Некорректная команда подогрева' };
  const next = { ...state, now: timestamp };
  const availability = getHeatingAvailability(next);
  const allowed = enabled ? availability.canEnable : availability.canDisable;
  if (!allowed) return {
    state: next, ok: false,
    message: availability.reason || (enabled ? 'Подогрев уже разрешён' : 'Подогрев уже отключён'),
  };
  const message = enabled ? 'Режим подогрева разрешён' : 'Режим подогрева отключён';
  return {
    state: event({ ...next, heating: { mode: enabled ? 'enabled' : 'off', actual: 'unknown' } }, {
      kind: enabled ? 'success' : 'info',
      title: message,
      detail: 'Демонстрационная команда. Состояние физического нагревателя неизвестно.',
    }),
    ok: true,
    message: `${message} · демо`,
  };
}

export function ackEvent(state, id) {
  if (!state.events.some((entry) => entry.id === id && !entry.acknowledged)) return state;
  return {
    ...state,
    events: state.events.map((entry) => entry.id === id ? { ...entry, acknowledged: true } : entry),
  };
}

export function getAlerts(state) {
  if (state?.connection?.connected !== true) return [{
    id: 'connection', kind: 'warning', title: 'Нет подключения',
    detail: 'Показания и управление недоступны в выбранном сценарии.',
  }];
  const alerts = [];
  const level = getLevel(state);
  const texts = {
    low: ['Мало топлива', 'Нижний уровень не достигнут.'],
    inconsistent: ['Проверьте датчики', 'Нормализованные состояния верхнего и нижнего уровней не согласуются. Это не диагноз обрыва.'],
    stale: ['Данные уровня устарели', 'Оценка уровня приостановлена до получения свежих показаний.'],
    'no-data': ['Нет данных уровня', 'Для оценки нужны два достоверных показания.'],
  };
  if (texts[level.id]) alerts.push({ id: `level-${level.id}`, kind: level.id === 'inconsistent' ? 'danger' : 'warning', title: texts[level.id][0], detail: texts[level.id][1] });
  const missing = SENSOR_DEFS.filter(({ id }) => getReadingStatus(state?.telemetry?.[id], state?.now) !== 'fresh');
  if (missing.length) alerts.push({
    id: 'temperatures', kind: 'warning', title: 'Нет актуальных температур',
    detail: missing.map(({ label }) => label).join(' · '),
  });
  if (state?.connection?.commandAvailable !== true) alerts.push({
    id: 'command', kind: 'warning', title: 'Управление недоступно',
    detail: 'Канал команд отключён в выбранном сценарии.',
  });
  return alerts;
}

export function getTemperatureHistory(state, sensorId, period = '1h') {
  if (!KNOWN_SENSORS.has(sensorId)) return [];
  const reading = state?.telemetry?.[sensorId];
  if (getReadingStatus(reading, state?.now) === 'no-data') return [];
  const duration = ({ '1h': 3_600_000, '6h': 21_600_000, '24h': 86_400_000 })[period] ?? 3_600_000;
  // History ends at the last sample. Stale readings never gain fresh endpoints.
  const end = reading.updatedAt;
  const start = Math.max(0, end - duration);
  const amplitudes = { room: 0.25, outdoor: 0.6, coolant: 1.2, fuel: 1.0 };
  const trend = { room: 0.4, outdoor: -0.8, coolant: 3.6, fuel: state.scenario === 'heating' ? 18 : 3 };
  return Array.from({ length: 61 }, (_, index) => {
    const fraction = index / 60;
    const wave = Math.sin(fraction * Math.PI * 4) * Math.sin(fraction * Math.PI) * amplitudes[sensorId];
    return {
      time: Math.round(start + (end - start) * fraction),
      value: index === 60 ? reading.value : Math.round((reading.value - trend[sensorId] * (1 - fraction) + wave) * 10) / 10,
    };
  });
}

export function formatAge(milliseconds) {
  if (typeof milliseconds !== 'number' || !Number.isFinite(milliseconds) || milliseconds < 0) return 'нет данных';
  if (milliseconds < 1_000) return 'только что';
  const seconds = Math.floor(milliseconds / 1_000);
  if (seconds < 60) return `${seconds} с назад`;
  const minutes = Math.floor(seconds / 60);
  if (minutes < 60) return `${minutes} мин назад`;
  const hours = Math.floor(minutes / 60);
  if (hours < 24) return `${hours} ч назад`;
  return `${Math.floor(hours / 24)} дн назад`;
}
