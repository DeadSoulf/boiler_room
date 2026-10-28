import {SENSOR_DEFS, SCENARIOS, FRESHNESS_MS, createState, selectScenario, getLevel, getHeatingAvailability, setHeating, getReadingStatus, getTemperatureHistory, getAlerts, ackEvent, formatAge} from './model.js';

const icons = {
  boiler:'<path d="M8 3h8v4H8zM6 8h12v12H6zM8 20v2m8-2v2"/><path d="M10 14c0-2 2-4 2-4s3 3 3 5a3 3 0 0 1-5-1Z"/>',
  grid:'<rect x="3" y="3" width="7" height="7" rx="2"/><rect x="14" y="3" width="7" height="7" rx="2"/><rect x="3" y="14" width="7" height="7" rx="2"/><rect x="14" y="14" width="7" height="7" rx="2"/>',
  fuel:'<path d="M6 3h10v18H4V5a2 2 0 0 1 2-2Z"/><path d="M7 6h6v5H7zm9 4h2l3 3v5a2 2 0 0 1-4 0v-3m2-11 3 3v4"/>',
  chart:'<path d="M3 3v18h18M6 15l4-5 4 3 6-8"/>',
  journal:'<path d="M5 3h14v18H5zM9 7h6M9 11h6M9 15h4"/>',
  sensors:'<rect x="7" y="7" width="10" height="10" rx="2"/><path d="M9 2v5m6-5v5M9 17v5m6-5v5M2 9h5m-5 6h5m10-6h5m-5 6h5"/><path d="M10 10h4v4h-4z"/>',
  service:'<path d="m14.5 6.5 3 3 4-4a6 6 0 0 1-7.9 7.9l-7.8 7.8a2.8 2.8 0 0 1-4-4l7.8-7.8A6 6 0 0 1 17.5 1.5Z"/>',
  display:'<rect x="5" y="2" width="14" height="20" rx="3"/><path d="M9 5h6m-4 14h2"/>',
  sliders:'<path d="M4 21v-7m0-4V3m8 18V11m0-4V3m8 18v-3m0-4V3M1 14h6m2-7h6m2 11h6"/>',
  close:'<path d="m6 6 12 12M6 18 18 6"/>',
  arrow:'<path d="M4 12h16m-6-6 6 6-6 6"/>',
  chevron:'<path d="m9 5 7 7-7 7"/>',
  thermometer:'<path d="M9 14.5V5a3 3 0 0 1 6 0v9.5a5 5 0 1 1-6 0ZM12 8v9"/>',
  room:'<path d="m3 11 9-8 9 8M6 9v12h12V9M10 21v-7h4v7"/>',
  outdoor:'<path d="M3 16a4 4 0 1 1 3-7 6 6 0 1 1 11 6H3ZM16 3V1m5 6 2-1M6 2l1 2"/>',
  coolant:'<path d="M12 2S4 11 4 16a8 8 0 0 0 16 0C20 11 12 2 12 2Z"/><path d="M8 16a4 4 0 0 0 4 4"/>',
  heat:'<path d="M9 3c-5 5 5 6 0 11m6-11c-5 5 5 6 0 11M4 18h16M7 21h10"/>',
  power:'<path d="M12 2v10m-6.4-7A9 9 0 1 0 18.4 5"/>',
  stop:'<rect x="5" y="5" width="14" height="14" rx="3"/>',
  check:'<path d="m5 12 4 4L19 6"/>',
  warning:'<path d="m12 3 10 18H2L12 3ZM12 9v5m0 3v.1"/>',
  info:'<circle cx="12" cy="12" r="9"/><path d="M12 11v6m0-10v.1"/>',
  wifi:'<path d="M2 8a16 16 0 0 1 20 0M5 12a11 11 0 0 1 14 0m-10 4a5 5 0 0 1 6 0m-3 4v.1"/>',
  disconnected:'<path d="m3 3 18 18M2 8a16 16 0 0 1 4-2m4-1a16 16 0 0 1 12 3M5 12a11 11 0 0 1 4-2m5 0a11 11 0 0 1 5 2m-10 4a5 5 0 0 1 6 0m-3 4v.1"/>',
  clock:'<circle cx="12" cy="12" r="9"/><path d="M12 6v6l4 2"/>',
  download:'<path d="M12 3v12m-5-5 5 5 5-5M4 16v5h16v-5"/>',
  reset:'<path d="M3 10a9 9 0 1 1 2 9M3 3v7h7"/>',
};
const icon = (name) => `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${icons[name] || icons.info}</svg>`;
const esc = (value) => String(value ?? '').replace(/[&<>"']/g, (char) => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[char]));
const number = (value, digits=1) => Number.isFinite(value) ? value.toLocaleString('ru-RU',{minimumFractionDigits:digits,maximumFractionDigits:digits}) : '—';
const time = (stamp) => new Intl.DateTimeFormat('ru-RU',{hour:'2-digit',minute:'2-digit',timeZone:'Europe/Moscow'}).format(stamp);
const sensorIcon = {room:'room',outdoor:'outdoor',coolant:'coolant',fuel:'fuel'};
const titles = {overview:'Обзор котельной',fuel:'Топливо и подогрев',history:'История температуры',journal:'Журнал событий',diagnostics:'Диагностика',service:'Сервис'};
const nav = [
  ['overview','Обзор','Обзор','grid'],['fuel','Топливо','Топливо','fuel'],['history','История','Графики','chart'],['journal','Журнал','Журнал','journal'],['diagnostics','Диагностика','Датчики','sensors'],['service','Сервис','Сервис','service'],
];
let state = createState();
let route = Object.hasOwn(titles,location.hash.slice(1)) ? location.hash.slice(1) : 'overview';
let selectedSensor = 'fuel';
let period = '1h';
let journalFilter = 'all';
let toastTimer;
let chartSequence=0;
const main = document.getElementById('mainContent');
const dialog = document.getElementById('scenarioDialog');

function hydrateIcons(root=document){root.querySelectorAll('[data-icon]').forEach(el=>{el.innerHTML=icon(el.dataset.icon);});}
function badge(label,tone='muted'){return `<span class="badge ${tone}">${esc(label)}</span>`;}
function readout(id){const reading=state.telemetry[id];return getReadingStatus(reading,state.now)==='fresh'?number(reading.value):'—';}
function modeLabel(){return {off:'Отключён',enabled:'Разрешён',unknown:'Нет данных'}[state.heating.mode];}
function readingLabel(reading,key='value'){return {fresh:'Свежие данные',stale:'Устарели','no-data':'Нет данных'}[getReadingStatus(reading,state.now,key)];}
function reedLabel(reading){const status=getReadingStatus(reading,state.now,'reached');return status==='fresh'?(reading.reached?'Достигнут':'Не достигнут'):(status==='stale'?'Устарели':'Нет данных');}
function reedTone(reading){return getReadingStatus(reading,state.now,'reached')==='fresh'&&reading.reached?'good-text':'muted';}
function tempUnit(id){return `${readout(id)}<span>°C</span>`;}
function chartSlot(sensor='fuel',large=false,chartPeriod=period){return `<div class="chart-slot" data-sensor="${sensor}" data-period="${chartPeriod}" data-large="${large}"></div>`;}

function sparkline(sensor){
  const data=getTemperatureHistory(state,sensor,'1h');
  if(!data.length||getReadingStatus(state.telemetry[sensor],state.now)!=='fresh')return '<span class="sparkline"></span>';
  const values=data.map(p=>p.value),min=Math.min(...values)-.3,max=Math.max(...values)+.3;
  const points=data.map((p,i)=>`${(i/(data.length-1)*80).toFixed(1)},${(21-(p.value-min)/(max-min)*19).toFixed(1)}`).join(' ');
  return `<svg class="sparkline" viewBox="0 0 82 23" aria-hidden="true"><polyline points="${points}" fill="none" stroke="currentColor" stroke-width="1.5"/></svg>`;
}
function temperatureCards(){return `<section class="overview-temps" aria-label="Температуры">${SENSOR_DEFS.map(({id,label})=>`<button class="temp-card" data-sensor-open="${id}" aria-label="${esc(label)}: ${readout(id)} градусов. Открыть ${id==='fuel'?'топливо':'историю'}"><div class="temp-card-header"><span class="sensor-name">${esc(label)}</span>${icon(sensorIcon[id])}</div><div class="temp-value">${tempUnit(id)}</div><div class="temp-card-footer"><span>${readingLabel(state.telemetry[id])}</span>${sparkline(id)}</div></button>`).join('')}</section>`;}

function tank(){
  const level=getLevel(state),colors={between:'#67dfc0',upper:'#89c9ff',low:'#f4c379',inconsistent:'#ff9c9c',stale:'#7890a8','no-data':'#7890a8'};
  const c=colors[level.id],zone={between:[57,65],upper:[28,28],low:[123,28]}[level.id];
  return `<div class="tank"><svg viewBox="0 0 120 180" role="img" aria-label="Схема порогов уровня: ${esc(level.label)}"><rect x="22" y="11" width="66" height="11" rx="4" fill="#1e354a" stroke="#3a536c"/><rect x="9" y="22" width="94" height="138" rx="18" fill="#101e2e" stroke="#3a536c" stroke-width="1.5"/>${zone?`<rect x="15" y="${zone[0]}" width="82" height="${zone[1]}" rx="${level.id==='between'?0:10}" fill="${c}" fill-opacity=".17"/>`:''}<path d="M15 57h82M15 122h82" stroke="#4a647c" stroke-width="1" stroke-dasharray="4 4"/><path d="M103 57h11m-11 65h11" stroke="#5f7c93" stroke-width="2"/><circle cx="114" cy="57" r="4" fill="${getReadingStatus(state.upper,state.now,'reached')==='fresh'&&state.upper.reached?'#89c9ff':'#5a7188'}"/><circle cx="114" cy="122" r="4" fill="${getReadingStatus(state.lower,state.now,'reached')==='fresh'&&state.lower.reached?'#67dfc0':'#5a7188'}"/><path d="M26 161v9m59-9v9" stroke="#3a536c" stroke-width="4" stroke-linecap="round"/>${!zone?`<text x="56" y="98" text-anchor="middle" fill="${c}" font-size="27" font-family="Arial">${level.id==='inconsistent'?'!':'—'}</text>`:''}</svg></div>`;
}
function fuelSummary(){
  const level=getLevel(state);
  return `<div class="fuel-summary">${tank()}<div class="level-copy"><h3 class="${({success:'good-text',info:'info-text',warning:'warn-text',danger:'danger-text'})[level.tone]||'muted'}">${esc(level.label)}</h3><p>${esc(level.description)}</p><div class="reed-row"><span class="reed-label"><span class="status-dot" style="background:${reedTone(state.upper)==='good-text'?'var(--blue)':'var(--faint)'}"></span>Верхний</span><span class="reed-state ${reedTone(state.upper)}">${reedLabel(state.upper)}</span></div><div class="reed-row"><span class="reed-label"><span class="status-dot" style="background:${reedTone(state.lower)==='good-text'?'var(--teal)':'var(--faint)'}"></span>Нижний</span><span class="reed-state ${reedTone(state.lower)}">${reedLabel(state.lower)}</span></div></div></div>`;
}
function heatControls(){
  const availability=getHeatingAvailability(state);
  return `<div class="compact-controls"><button class="button primary" data-heating="on" aria-label="Включить подогрев топлива" ${availability.canEnable?'':'disabled'}>${icon('power')}Включить</button><button class="button secondary" data-heating="off" aria-label="Выключить подогрев топлива" ${availability.canDisable?'':'disabled'}>${icon('stop')}Выключить</button></div><p class="button-note">${esc(availability.reason||'Изменение режима в демонстрации.')}</p>`;
}
function alerts(){
  const alert=getAlerts(state)[0];
  if(!alert)return '';
  return `<section class="alert-banner ${alert.kind==='danger'?'danger':''}" aria-label="Действующее предупреждение"><div class="alert-main">${icon('warning')}<div><strong>${esc(alert.title)}</strong><p>${esc(alert.detail)}</p></div></div><button class="text-button" data-route="${route==='fuel'?'diagnostics':'fuel'}">Подробнее${icon('chevron')}</button></section>`;
}
function relativeTime(stamp){const seconds=Math.max(0,Math.floor((stamp-state.startedAt)/1000));return `+${String(Math.floor(seconds/60)).padStart(2,'0')}:${String(seconds%60).padStart(2,'0')}`;}
function eventList(limit=Infinity,full=false){
  const entries=state.events.filter(e=>!full||journalFilter==='all'||(journalFilter==='warnings'?['warning','danger'].includes(e.kind):e.detail.includes('команда'))).slice(0,limit);
  if(!entries.length)return `<div class="empty-state">${icon('journal')}<p>В этом разделе пока нет событий</p></div>`;
  return `<ul class="event-list">${entries.map(e=>`<li class="event-item"><span class="event-icon ${esc(e.kind)}">${icon(e.kind==='warning'||e.kind==='danger'?'warning':e.kind==='success'?'check':'info')}</span><div class="event-content"><h3>${esc(e.title)}</h3><p>${esc(e.detail)}</p>${full&&['warning','danger'].includes(e.kind)?`<div class="ack-row">${e.acknowledged?'<span class="ack-label">Отмечено как прочитанное</span>':`<button class="ack-button" data-ack="${esc(e.id)}">Прочитано</button>`}</div>`:''}</div><time class="event-time" title="Время от запуска демонстрации">${relativeTime(e.time)}</time></li>`).join('')}</ul>`;
}
function diagnosticMini(){
  const fresh=SENSOR_DEFS.filter(({id})=>getReadingStatus(state.telemetry[id],state.now)==='fresh').length;
  const level=getLevel(state);
  return `<div class="diagnostic-mini"><div class="mini-row"><span>Источник показаний</span><span>Демонстрация</span></div><div class="mini-row"><span>Температурные каналы</span><span class="${fresh===4?'good-text':'warn-text'}">${fresh} / 4 актуальны</span></div><div class="mini-row"><span>Два порога уровня</span><span class="${level.fresh&&level.id!=='inconsistent'?'good-text':'warn-text'}">${level.fresh&&level.id!=='inconsistent'?'Согласованы':'Проверьте данные'}</span></div><div class="mini-row"><span>Реальное устройство</span><span class="muted">Не подключено</span></div></div><p class="subtle-note">Состояния датчиков и команды можно проверить через «Сценарии».</p>`;
}
function overview(){return `${temperatureCards()}<div class="grid-main"><section class="panel"><div class="panel-header"><div class="panel-title">${icon('fuel')}<h2>Топливо</h2></div><button class="text-button" data-route="fuel">Открыть${icon('chevron')}</button></div>${fuelSummary()}<div class="fuel-heating-strip"><div><span class="strip-label">${icon('heat')}Подогрев до 80 °C</span><strong>${modeLabel()}</strong></div><span class="muted" style="font-size:11px">Нагреватель: нет данных</span></div>${heatControls()}</section><section class="panel"><div class="panel-header"><div><h2>Температура топлива</h2><p>История демонстрационных показаний</p></div></div><div class="chart-toolbar"><div class="chart-current">${tempUnit('fuel')}<small>${readingLabel(state.telemetry.fuel)}</small></div>${periodSelector()}</div>${chartSlot('fuel')}<button class="inline-link" data-route="history">Все температурные каналы ${icon('arrow')}</button></section></div><div class="overview-bottom"><section class="panel"><div class="panel-header"><h2>Последние события</h2><button class="text-button" data-route="journal">Весь журнал${icon('chevron')}</button></div>${eventList(3)}</section><section class="panel"><div class="panel-header"><h2>Состояние системы</h2><button class="text-button" data-route="diagnostics">Диагностика${icon('chevron')}</button></div>${diagnosticMini()}</section></div>`;}
function periodSelector(){return `<div class="segmented" role="group" aria-label="Период истории">${[['1h','1 час'],['6h','6 часов'],['24h','24 часа']].map(([id,label])=>`<button data-period="${id}" aria-pressed="${period===id}">${label}</button>`).join('')}</div>`;}
function fuelPage(){return `<p class="page-description">Уровень по двум герконовым датчикам и управление режимом подогрева топлива.</p><div class="fuel-grid"><section class="panel"><div class="panel-header"><div class="panel-title">${icon('fuel')}<h2>Уровень топлива</h2></div>${badge('2 датчика','info')}</div>${fuelSummary()}<p class="sensor-footnote">Уровень определяется по двум порогам. Точный объём не измеряется.</p></section><section class="panel"><div class="panel-header"><div class="panel-title">${icon('heat')}<h2>Подогрев топлива</h2></div>${badge('до 80 °C','info')}</div><div class="heating-top"><div><small>Температура топлива</small><div class="heating-temp">${tempUnit('fuel')}</div></div><span class="heat-icon">${icon('heat')}</span></div><div class="state-pair"><div><span>Режим подогрева</span>${badge(modeLabel(),state.heating.mode==='enabled'?'success':'muted')}</div><div><span>Нагреватель</span><span class="muted">Нет данных</span></div></div>${heatControls()}</section></div><div class="fuel-bottom"><section class="panel"><div class="panel-header"><h2>История температуры</h2>${periodSelector()}</div>${chartSlot('fuel')}<button class="inline-link" data-sensor-open="fuel-history">Развернуть график ${icon('arrow')}</button></section><section class="panel"><div class="panel-header"><h2>Подробнее о топливе</h2></div><button class="quick-link" data-route="journal"><span>Журнал событий<small>Смена уровня и команды подогрева</small></span>${icon('chevron')}</button><button class="quick-link" data-route="diagnostics"><span>Диагностика датчиков<small>Показания и свежесть данных</small></span>${icon('chevron')}</button><button class="quick-link" data-route="service"><span>Параметры демонстрации<small>Сценарии и состояние подключения</small></span>${icon('chevron')}</button><p class="subtle-note">Разрешённый режим и фактическая работа нагревателя — отдельные состояния.</p></section></div>`;}
function historyPage(){
  const points=getTemperatureHistory(state,selectedSensor,period),values=points.map(p=>p.value);
  const sensor=SENSOR_DEFS.find(s=>s.id===selectedSensor);
  return `<p class="page-description">Выберите канал и период. Графики построены по демонстрационным данным.</p><div class="history-controls"><div class="sensor-tabs" role="group" aria-label="Температурный канал">${SENSOR_DEFS.map(s=>`<button data-select-sensor="${s.id}" aria-pressed="${s.id===selectedSensor}">${s.label}</button>`).join('')}</div>${periodSelector()}</div><section class="panel large-chart"><div class="panel-header"><div><h2>${sensor.label}</h2><p>${getReadingStatus(state.telemetry[selectedSensor],state.now)==='stale'?'История заканчивается последним полученным показанием':'Температура, °C'}</p></div>${badge(readingLabel(state.telemetry[selectedSensor]),getReadingStatus(state.telemetry[selectedSensor],state.now)==='fresh'?'success':'warning')}</div><div class="chart-current">${tempUnit(selectedSensor)}</div>${chartSlot(selectedSensor,true)}<div class="history-summary">${[['Минимум',values.length?Math.min(...values):null],['Средняя',values.length?values.reduce((a,b)=>a+b,0)/values.length:null],['Максимум',values.length?Math.max(...values):null]].map(([label,val])=>`<div><p>${label}</p><strong>${number(val)} <small>°C</small></strong></div>`).join('')}</div></section><p class="history-note">МСК · значения примера. Переключение периода не меняет показания датчиков.</p>`;
}
function journalPage(){return `<p class="page-description">События этой демонстрационной сессии. Отметка «Прочитано» не снимает действующее предупреждение.</p><div class="journal-tools"><div class="segmented" role="group" aria-label="Фильтр журнала">${[['all','Все'],['warnings','Предупреждения'],['commands','Команды']].map(([id,label])=>`<button data-filter="${id}" aria-pressed="${journalFilter===id}">${label}</button>`).join('')}</div><button class="button ghost" data-action="export" aria-label="Скачать журнал в CSV">${icon('download')}CSV</button></div><section class="panel journal-panel"><div class="panel-header"><div><h2>Журнал событий</h2><p>Время от запуска · мин:сек</p></div>${badge(`${state.events.length} событий`,'muted')}</div>${eventList(Infinity,true)}</section><p class="history-note">Журнал хранится в текущей вкладке. Перезагрузка начинает новую демонстрацию.</p>`;}
function diagnosticRow(name,sub,reading,key='value'){
  const status=getReadingStatus(reading,state.now,key),statusText={fresh:'Актуально',stale:'Устарело','no-data':'Нет данных'}[status],tone={fresh:'success',stale:'warning','no-data':'muted'}[status];
  const value=status==='no-data'?'—':key==='reached'?(reading.reached?'Достигнут':'Не достигнут'):`${number(reading.value)} °C`;
  return `<tr><td class="sensor-name-cell">${esc(name)}<small>${esc(sub)}</small></td><td class="number">${value}${status==='stale'?'<small class="muted" style="display:block;font-size:9px">Последнее значение</small>':''}</td><td class="col-age muted">${reading.updatedAt===null?'—':formatAge(state.now-reading.updatedAt)}</td><td>${badge(statusText,tone)}</td></tr>`;
}
function diagnosticsPage(){
  const fresh=SENSOR_DEFS.filter(s=>getReadingStatus(state.telemetry[s.id],state.now)==='fresh').length;
  const freshReeds=[state.lower,state.upper].filter(r=>getReadingStatus(r,state.now,'reached')==='fresh').length;
  return `<p class="page-description">Достоверность и свежесть показаний. Состояние входа не подтверждает исправность физической линии.</p><div class="diagnostic-grid"><section class="panel diagnostic-card"><p class="metric-label">Температуры</p><p class="metric-value ${fresh===4?'good-text':'warn-text'}">${fresh} / 4</p><p class="metric-note">Актуальные показания</p></section><section class="panel diagnostic-card"><p class="metric-label">Герконовые датчики</p><p class="metric-value ${freshReeds===2?'good-text':'warn-text'}">${freshReeds} / 2</p><p class="metric-note">Актуальные состояния</p></section><section class="panel diagnostic-card"><p class="metric-label">Канал команд в демо</p><p class="metric-value ${state.connection.commandAvailable?'good-text':'warn-text'}">${state.connection.commandAvailable?'Доступен':'Недоступен'}</p><p class="metric-note">Без связи с оборудованием</p></section></div><section class="panel"><div class="panel-header"><h2>Все датчики</h2>${badge('Снимок демо','info')}</div><div class="table-wrap"><table class="sensor-table"><thead><tr><th scope="col">ДАТЧИК</th><th scope="col">ПОКАЗАНИЕ</th><th class="col-age" scope="col">ВОЗРАСТ</th><th scope="col">СОСТОЯНИЕ</th></tr></thead><tbody>${SENSOR_DEFS.map(s=>diagnosticRow(s.label,'Температура',state.telemetry[s.id])).join('')}${diagnosticRow('Верхний уровень','Геркон · нормализованный сигнал',state.upper,'reached')}${diagnosticRow('Нижний уровень','Геркон · нормализованный сигнал',state.lower,'reached')}</tbody></table></div><p class="subtle-note">Показание считается устаревшим через ${FRESHNESS_MS/1000} секунд. При отсутствии достоверного снимка уровень не определяется.</p></section><div class="overview-bottom"><section class="panel"><div class="panel-header"><h2>Согласованность уровней</h2></div>${badge(getLevel(state).label,getLevel(state).tone)}<p class="display-help">${esc(getLevel(state).description)}</p><button class="inline-link" data-route="fuel">Открыть топливо ${icon('arrow')}</button></section><section class="panel"><div class="panel-header"><h2>Аппаратное подключение</h2></div><div class="diagnostic-mini"><div class="mini-row"><span>GPIO и полярность</span><span class="muted">Не назначены</span></div><div class="mini-row"><span>Контроль обрыва линии</span><span class="muted">Не определён</span></div><div class="mini-row"><span>Обратная связь нагревателя</span><span class="muted">Нет данных</span></div></div></section></div>`;
}
function servicePage(){return `<p class="page-description">Настройки просмотра и информация о текущей демонстрации.</p><div class="service-grid"><section class="panel"><div class="panel-header"><div class="panel-title">${icon('display')}<h2>Веб-панель</h2></div>${badge('v0.1','info')}</div><dl class="detail-list"><div><dt>Базовый дисплей</dt><dd>480 × 800</dd></div><div><dt>Источник данных</dt><dd>Демонстрация</dd></div><div><dt>Сценарий</dt><dd>${esc(SCENARIOS.find(s=>s.id===state.scenario).label)}</dd></div><div><dt>Реальный контроллер</dt><dd class="muted">Не подключён</dd></div><div><dt>Время на графиках</dt><dd>Москва, UTC+3</dd></div></dl><p class="service-tip">Интерфейс адаптируется к экрану. На компьютере можно включить вид дисплея 480 × 800 кнопкой в шапке.</p></section><section class="panel"><div class="panel-header"><div class="panel-title">${icon('sliders')}<h2>Демонстрация</h2></div></div><p class="display-help">Проверьте низкий уровень, устаревшие данные и потерю подключения. Изменения не передаются оборудованию.</p><div class="service-actions"><button class="button secondary" data-action="scenarios">${icon('sliders')}Выбрать сценарий</button><button class="button ghost" data-action="reset">${icon('reset')}Начать заново</button></div><p class="subtle-note">«Начать заново» возвращает рабочий диапазон, отключённый режим подогрева и очищает журнал демонстрации.</p><div class="panel-header" style="margin-top:28px;margin-bottom:12px"><h3>Подогрев топлива</h3>${badge('до 80 °C','info')}</div><p class="display-help">В демонстрации кнопки изменяют разрешение режима. Состояние самого нагревателя показано отдельно. Автоматическое регулирование температуры здесь не задано.</p></section></div>`;}

function drawChart(slot){
  const sensor=slot.dataset.sensor,chartPeriod=slot.dataset.period,large=slot.dataset.large==='true';
  const points=getTemperatureHistory(state,sensor,chartPeriod);
  if(!points.length){slot.innerHTML=`<div class="empty-chart">${icon('chart')}<p>История пока недоступна</p><small>Нет показаний в выбранном сценарии</small></div>`;return;}
  const width=Math.max(250,Math.floor(slot.clientWidth)),height=large?310:209;
  const margin={left:39,right:12,top:15,bottom:34},w=width-margin.left-margin.right,h=height-margin.top-margin.bottom;
  const values=points.map(p=>p.value),lo=Math.min(...values),hi=Math.max(...values),pad=Math.max(.7,(hi-lo)*.25),min=lo-pad,max=hi+pad;
  const x=i=>margin.left+i/(points.length-1)*w,y=v=>margin.top+(1-(v-min)/(max-min))*h;
  const path=points.map((p,i)=>`${i?'L':'M'}${x(i).toFixed(1)},${y(p.value).toFixed(1)}`).join(' ');
  const bottom=height-margin.bottom,id=`chartFill${++chartSequence}`;
  const area=`${path} L${x(points.length-1)},${bottom} L${x(0)},${bottom} Z`;
  const yLabels=Array.from({length:4},(_,i)=>{const value=min+(max-min)*i/3,yy=y(value);return `<line x1="${margin.left}" x2="${width-margin.right}" y1="${yy}" y2="${yy}" stroke="#2b3b4e" stroke-width="1" stroke-dasharray="3 5"/><text x="${margin.left-10}" y="${yy+4}" text-anchor="end" fill="#8ba3ba" font-size="11">${number(value)}</text>`;}).join('');
  const xLabels=[0,1,2,3].map(i=>{const index=Math.round(i/3*(points.length-1));return `<text x="${x(index)}" y="${height-9}" text-anchor="${i===0?'start':i===3?'end':'middle'}" fill="#8ba3ba" font-size="11">${time(points[index].time)}</text>`;}).join('');
  const status=getReadingStatus(state.telemetry[sensor],state.now),stale=status==='stale';
  slot.innerHTML=`<figure class="chart"><svg viewBox="0 0 ${width} ${height}" role="img" aria-label="${esc(SENSOR_DEFS.find(s=>s.id===sensor).label)}. Демонстрационная история за ${chartPeriod==='1h'?'час':chartPeriod==='6h'?'6 часов':'24 часа'}. От ${number(lo)} до ${number(hi)} градусов.${stale?' Последние показания устарели.':''}"><defs><linearGradient id="${id}" x1="0" y1="0" x2="0" y2="1"><stop offset="0%" stop-color="${stale?'#8ba3ba':'#67dfc0'}" stop-opacity=".18"/><stop offset="100%" stop-color="${stale?'#8ba3ba':'#67dfc0'}" stop-opacity="0"/></linearGradient></defs>${yLabels}<text x="${margin.left-10}" y="10" text-anchor="end" fill="#8ba3ba" font-size="10">°C</text><path d="${area}" fill="url(#${id})"/><path d="${path}" fill="none" stroke="${stale?'#8ba3ba':'#67dfc0'}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/><circle cx="${x(points.length-1)}" cy="${y(points.at(-1).value)}" r="3.5" fill="${stale?'#8ba3ba':'#67dfc0'}" stroke="#132131" stroke-width="2"/>${xLabels}</svg><figcaption><span class="legend"><i${stale?' style="background:var(--muted)"':''}></i>${stale?'Последние известные данные':'Демонстрационные значения'}</span><span>МСК</span></figcaption></figure>`;
  slot.dataset.drawnWidth=String(width);
}
function drawCharts(){main.querySelectorAll('.chart-slot').forEach(drawChart);}
function updateNavigation(){
  document.getElementById('desktopNav').innerHTML=nav.map(([id,label,,symbol])=>`<button class="nav-button" data-route="${id}" ${id===route?'aria-current="page"':''} aria-label="${label}">${icon(symbol)}<span class="nav-label">${label}</span>${id==='journal'&&state.events.some(e=>['warning','danger'].includes(e.kind)&&!e.acknowledged)?'<span class="nav-count" aria-label="Есть непрочитанные предупреждения">!</span>':''}</button>`).join('');
  document.getElementById('mobileNav').innerHTML=nav.map(([id,label,mobile,symbol])=>`<button data-route="${id}" ${id===route?'aria-current="page"':''} aria-label="${label}">${icon(symbol)}<span>${mobile}</span></button>`).join('');
}
function render({focus=false}={}){
  const title=titles[route];document.getElementById('pageTitle').textContent=title;document.title=`Панель котельной · ${title}`;
  main.innerHTML=alerts()+({overview,fuel:fuelPage,history:historyPage,journal:journalPage,diagnostics:diagnosticsPage,service:servicePage})[route]();
  updateNavigation();drawCharts();
  if(focus)main.focus({preventScroll:true});
}
function navigate(next){if(!Object.hasOwn(titles,next))return;if(next!==route){route=next;history.pushState(null,'',`#${next}`);render({focus:true});if(document.body.classList.contains('display-mode'))document.getElementById('appShell').scrollTop=0;else window.scrollTo({top:0,behavior:'instant'});} }
function showToast(message,error=false){const element=document.getElementById('toast');clearTimeout(toastTimer);element.textContent=message;element.classList.toggle('error',error);element.hidden=false;toastTimer=setTimeout(()=>{element.hidden=true;},4200);}
function openScenarios(){document.getElementById('scenarioOptions').innerHTML=SCENARIOS.map(s=>`<label class="scenario-option"><input type="radio" name="scenario" value="${s.id}" ${state.scenario===s.id?'checked':''}><span><strong>${esc(s.label)}</strong><small>${esc(s.description)}</small></span></label>`).join('');dialog.showModal();}
function exportJournal(){
  const quote=v=>`"${String(v).replaceAll('"','""')}"`;
  const rows=[['Время от запуска','Тип','Событие','Описание','Прочитано'],...state.events.map(e=>[relativeTime(e.time),e.kind,e.title,e.detail,e.acknowledged?'Да':'Нет'])];
  const blob=new Blob(['\uFEFF'+rows.map(row=>row.map(quote).join(';')).join('\r\n')],{type:'text/csv;charset=utf-8'});
  const url=URL.createObjectURL(blob),link=document.createElement('a');link.href=url;link.download='boiler-demo-events.csv';document.body.append(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);showToast('Журнал демонстрации выгружен в CSV');
}
document.addEventListener('click',event=>{
  const button=event.target.closest('button');if(!button||button.disabled)return;
  if(button.dataset.route){navigate(button.dataset.route);return;}
  if(button.dataset.sensorOpen){const id=button.dataset.sensorOpen;if(id==='fuel')navigate('fuel');else{selectedSensor=id==='fuel-history'?'fuel':id;navigate('history');}return;}
  if(button.dataset.heating){const result=setHeating(state,button.dataset.heating==='on');state=result.state;render();showToast(result.message,!result.ok);const next=main.querySelector(`[data-heating="${state.heating.mode==='enabled'?'off':'on'}"]`);if(next&&!next.disabled)next.focus({preventScroll:true});return;}
  if(button.dataset.period){period=button.dataset.period;render();main.querySelector(`[data-period="${period}"]`)?.focus({preventScroll:true});return;}
  if(button.dataset.selectSensor){selectedSensor=button.dataset.selectSensor;render();main.querySelector(`[data-select-sensor="${selectedSensor}"]`)?.focus({preventScroll:true});return;}
  if(button.dataset.filter){journalFilter=button.dataset.filter;render();main.querySelector(`[data-filter="${journalFilter}"]`)?.focus({preventScroll:true});return;}
  if(button.dataset.ack){state=ackEvent(state,button.dataset.ack);render({focus:true});showToast('Событие отмечено как прочитанное');return;}
  if(button.id==='scenarioButton'||button.dataset.action==='scenarios'){openScenarios();return;}
  if(button.dataset.action==='export'){exportJournal();return;}
  if(button.dataset.action==='reset'){state=createState();render();showToast('Новая демонстрация запущена');return;}
  if(button.id==='displayToggle'){const enabled=!document.body.classList.contains('display-mode');document.body.classList.toggle('display-mode',enabled);button.setAttribute('aria-pressed',String(enabled));button.setAttribute('aria-label',enabled?'Вернуться к широкому виду':'Включить вид дисплея 480 на 800');button.title=enabled?'Широкий вид':'Вид дисплея 480 × 800';drawCharts();}
});
dialog.addEventListener('change',event=>{if(event.target.name!=='scenario')return;state=selectScenario(state,event.target.value);render();showToast(`Сценарий: ${SCENARIOS.find(s=>s.id===state.scenario).label}`);});
dialog.addEventListener('click',event=>{if(event.target===dialog){const r=dialog.getBoundingClientRect();if(event.clientX<r.left||event.clientX>r.right||event.clientY<r.top||event.clientY>r.bottom)dialog.close();}});
window.addEventListener('hashchange',()=>{const next=location.hash.slice(1)||'overview';if(Object.hasOwn(titles,next)){route=next;render({focus:true});}});
const resizeObserver=new ResizeObserver(()=>main.querySelectorAll('.chart-slot').forEach(slot=>{if(Math.max(250,Math.floor(slot.clientWidth))!==Number(slot.dataset.drawnWidth))drawChart(slot);}));resizeObserver.observe(main);
function updateClock(){const now=new Date();document.getElementById('clockTime').textContent=time(now);document.getElementById('clockTime').dateTime=now.toISOString();document.getElementById('clockDate').textContent=new Intl.DateTimeFormat('ru-RU',{day:'numeric',month:'long',timeZone:'Europe/Moscow'}).format(now)+' · МСК';}
hydrateIcons();updateClock();setInterval(updateClock,30000);render();
