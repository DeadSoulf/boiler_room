#pragma once
// Self-contained on-device UI. No CDN, tracking, remote assets or credentials in HTML.
static const char kBoilerWebPage[] PROGMEM = R"BPHTML(<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="theme-color" content="#101719"><title>Котельная</title><style>
:root{color-scheme:dark;--bg:#101719;--card:#1b2528;--line:#304044;--ink:#eff5f3;--muted:#a1b3b5;--accent:#76d6c3;--warn:#e7bb78;--red:#ef9f9a;font-family:system-ui,-apple-system,Segoe UI,Arial,sans-serif;font-size:16px}
*{box-sizing:border-box}html{background:var(--bg);color:var(--ink)}body{margin:0;min-width:320px}button,input,select{font:inherit}button{color:var(--ink);background:#26373b;border:1px solid transparent;border-radius:12px;padding:12px 18px;min-height:46px;cursor:pointer;transition:background .15s}button:hover{background:#33494e}button:focus-visible,input:focus-visible,select:focus-visible,summary:focus-visible{outline:2px solid var(--accent);outline-offset:3px}button:disabled{opacity:.45;cursor:not-allowed}.primary{background:#245d55;border-color:#3b8b7c}.primary:hover{background:#30786b}.danger{background:#593738;border-color:#825453}.ghost{background:transparent;border-color:var(--line)}.hidden{display:none!important}
.container{max-width:1180px;margin:0 auto;padding:32px 28px 26px}header{display:flex;align-items:center;justify-content:space-between;gap:24px;margin-bottom:32px}h1{margin:0;font-size:32px;font-weight:600;letter-spacing:-.025em}h2{margin:0 0 24px;font-size:24px;font-weight:550}h3{font-size:18px;font-weight:550;margin:0 0 18px}p{color:var(--muted);line-height:1.55;margin:12px 0}.eyebrow{font-size:12px;letter-spacing:.12em;text-transform:uppercase;color:var(--muted);margin:0 0 8px}.header-meta{display:flex;align-items:center;gap:14px}.header-time{font-size:15px;color:var(--muted);font-variant-numeric:tabular-nums}.status,.badge,.tag{font-size:12px;color:var(--muted);padding:6px 0}.status.ok,.good{color:var(--accent)}.tag{border-left:1px solid var(--line);padding-left:14px}.toolbar{display:flex;align-items:center;gap:12px;flex-wrap:wrap}.topnav{display:flex;gap:8px;padding-bottom:22px;margin-bottom:30px;border-bottom:1px solid var(--line)}.topnav button{background:transparent;color:var(--muted);padding:12px 20px}.topnav button.active{color:var(--ink);background:#26373b}.topnav button:hover{color:var(--ink)}.page{animation:appear .16s ease-out}@keyframes appear{from{opacity:.4;transform:translateY(3px)}to{opacity:1;transform:none}}
.card{background:var(--card);border:1px solid var(--line);border-radius:20px;padding:26px;min-width:0}.grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:16px}.two{display:grid;grid-template-columns:1fr 1fr;gap:20px}.temp{display:flex;flex-direction:column;align-items:flex-start;text-align:left;min-height:290px;padding:24px;border:1px solid var(--line);background:var(--card)}.temp:hover{background:#233136;border-color:#52766d}.temp-name{color:var(--muted);font-size:15px;font-weight:400}.temp-value{font-size:clamp(30px,3.1vw,43px);font-weight:500;letter-spacing:-.04em;white-space:nowrap;line-height:1.2;margin:auto 0 12px;font-variant-numeric:tabular-nums}.temp-detail{color:var(--muted);font-size:12px;min-height:18px}.temp-fuel .temp-value{font-size:18px;letter-spacing:0;margin:13px 0 4px}.temp-fuel .temp-detail{margin-top:auto}.canister{height:152px;width:130px;align-self:center;color:var(--muted);margin:6px 0}.canister[data-state=full]{color:var(--accent)}.canister[data-state=half]{color:var(--warn)}.canister[data-state=empty],.canister[data-state=invalid]{color:var(--red)}.can-fill{fill:currentColor}.can-outline{stroke:currentColor;fill:none;stroke-width:3}.can-symbol{fill:currentColor;font-size:42px;font-weight:550}.can-cross{stroke:var(--bg);stroke-width:3;fill:none}.section{margin-top:24px}.row{display:flex;align-items:center;justify-content:space-between;gap:20px;padding:14px 0;border-bottom:1px solid var(--line);font-size:14px}.row:last-child{border-bottom:0}.row>span:first-child{color:var(--muted)}.row>:last-child{text-align:right;overflow-wrap:anywhere}.stack,.actions{display:flex;align-items:center;gap:12px;flex-wrap:wrap}.actions{margin:20px 0 8px}.actions button{max-width:100%}.field{margin:20px 0}label{display:block;color:var(--muted);font-size:13px;margin-bottom:8px}input,select{display:block;background:#101b1e;border:1px solid #40575c;border-radius:10px;color:var(--ink);width:100%;max-width:100%;min-height:46px;padding:12px}input[type=checkbox]{width:20px;min-height:20px;accent-color:var(--accent);flex-shrink:0}.stack select{flex:1;min-width:160px}.tiny,small{font-size:12px;line-height:1.55}.muted,.hint{color:var(--muted)}.hint{font-size:13px}.hint:empty{display:none}.notice,.footnote{font-size:13px;line-height:1.6;color:var(--muted);padding:16px 18px;background:#182528;border:1px solid var(--line);border-radius:12px;margin:20px 0}.warn{color:#eacfa8;border-color:#66543d;background:#29251f}.error{color:var(--red)}.details{border:1px solid var(--line);border-radius:14px;padding:16px;margin-top:20px}summary{cursor:pointer;color:var(--ink);padding:6px 0;line-height:1.5}details>summary{min-height:40px}.danger-zone{border-color:#73484a}details[open]>summary{margin-bottom:14px}.menu-grid{display:grid;grid-template-columns:1fr 1fr;gap:14px}.menu-item{display:grid;grid-template-columns:1fr auto;gap:9px;text-align:left;padding:24px;background:var(--card);border:1px solid var(--line);border-radius:17px}.menu-item strong{font-size:18px;font-weight:500}.menu-item span{font-size:13px;color:var(--muted);grid-row:2}.menu-item b{grid-column:2;grid-row:1/3;align-self:center;color:var(--muted);font-weight:400}.subhead{display:flex;align-items:center;gap:18px;margin:0 0 24px}.subhead h2{margin:0}.subhead button{background:transparent;border:1px solid var(--line)}.tab-row{display:none}.feed{max-height:320px;overflow:auto}.feed>button{min-height:56px}.equip-list{display:grid;grid-template-columns:1fr 1fr;gap:16px}.equip{padding:24px}.equip h3{margin-bottom:10px}.equip p{font-size:13px;margin:0}.equip .error{color:var(--muted)}.equip .tiny{display:none}.detail-chart{display:block;width:100%;height:140px;background:#122025;border-radius:12px;margin:20px 0}.fuel-summary-value{font-size:30px;font-weight:500}.service-grid{margin-top:20px}code{overflow-wrap:anywhere;font-size:12px;color:#b9ded7}footer{display:flex;justify-content:space-between;gap:16px;flex-wrap:wrap;border-top:1px solid var(--line);margin-top:38px;padding-top:20px;font-size:12px;color:var(--muted)}
dialog{color:var(--ink);background:var(--card);border:1px solid #49625f;border-radius:22px;padding:30px;width:min(460px,calc(100% - 32px));max-height:90vh;overflow:auto}dialog::backdrop{background:#050a0dd9;backdrop-filter:blur(4px)}dialog h2{margin-bottom:12px}.pair-dialog input{text-align:center;font-size:30px;letter-spacing:.28em;font-variant-numeric:tabular-nums}.pair-dialog .actions button{flex:1}.pair-dialog .close-modal{float:right;min-height:40px;padding:8px 12px;background:transparent}.pair-symbol{height:44px;width:44px;border:1px solid #3a7167;border-radius:12px;display:grid;place-items:center;margin-bottom:22px;color:var(--accent)}#pair-error{min-height:20px;font-size:13px}.pair-dialog .tiny{margin-bottom:0}#toast{position:fixed;bottom:22px;left:50%;transform:translateX(-50%);padding:14px 20px;border:1px solid #42625a;background:#203c35;color:var(--ink);border-radius:12px;max-width:calc(100% - 32px);z-index:100;box-shadow:0 5px 25px #0006;text-align:center;font-size:14px;pointer-events:none}
@media(max-width:1000px){.grid{grid-template-columns:repeat(2,minmax(0,1fr))}.temp{min-height:255px}.temp-value{font-size:42px}.header-meta{display:none}}
@media(max-width:680px){.container{padding:22px 16px}header{align-items:flex-start;gap:16px;margin-bottom:24px}h1{font-size:27px}.toolbar{justify-content:flex-end;gap:8px}.toolbar .tag{display:none}.toolbar button{padding:10px 13px;min-height:44px}.topnav{gap:3px;justify-content:space-between;margin-bottom:24px;padding-bottom:16px}.topnav button{font-size:13px;padding:10px 9px;min-height:44px}.grid{gap:12px}.temp{padding:18px;min-height:245px}.temp-value{font-size:32px}.temp-name{font-size:13px}.canister{height:137px;width:112px}.temp-fuel .temp-value{font-size:17px}.two,.equip-list,.menu-grid{grid-template-columns:1fr}.card{padding:20px}.temp.card{padding:18px}.menu-item{padding:20px}.subhead{gap:10px}.subhead h2{font-size:22px}.row{gap:12px}.row>:last-child{max-width:60%}.actions button{flex:1 1 auto}footer{font-size:11px}.notice{padding:14px}dialog{padding:24px}.header-time{font-size:13px}}
@media(max-width:380px){.topnav{display:grid;grid-template-columns:repeat(2,minmax(0,1fr))}.temp-value{font-size:28px}}
@media(prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
</style></head><body><div class="container">
<header><div><p class="eyebrow">Панель дома</p><h1>Котельная</h1></div><div class="header-meta"><span class="header-time" id="clock">--:--</span></div><div class="toolbar"><span id="authstate" class="tag">Просмотр</span><button id="pairbutton" class="primary" data-action="show-pair">Сопряжение</button><button id="logout" class="ghost hidden" data-action="logout">Завершить доступ</button></div></header>
<nav class="topnav" aria-label="Разделы"><button data-page="home" class="active">Главная</button><button data-page="equipment">Оборудование</button><button data-page="fuel">Топливо</button><button data-page="settings">Настройки</button></nav>
<dialog id="pairbox" class="pair-dialog" aria-labelledby="pair-title"><button class="close-modal" data-action="hide-pair" aria-label="Отменить сопряжение">×</button><div class="pair-symbol" aria-hidden="true">⌁</div><h2 id="pair-title">Сопряжение</h2><p id="pair-message">Запрашиваем код на дисплее…</p><form id="pair-form"><label for="paircode">Код с физической панели</label><input id="paircode" inputmode="numeric" autocomplete="one-time-code" pattern="[0-9]{6}" maxlength="6" placeholder="······" required><p id="pair-error" class="error" role="alert"></p><p id="pair-expiry" class="tiny"></p><div class="actions"><button type="button" class="ghost" data-action="pair-retry">Новый код</button><button type="submit" class="primary" id="pair-submit" disabled>Подключить</button></div></form><p class="tiny">Доступ на 1 час. HTTP не шифруется — только доверенная локальная сеть.</p></dialog>
<section id="home" class="page"><div class="grid">
<button class="card temp" data-temp="0"><span class="temp-name">В котельной</span><span class="temp-value" id="t0">--.- °C</span><span class="temp-detail" id="tstate0">Нет данных</span></button>
<button class="card temp" data-temp="1"><span class="temp-name">На улице</span><span class="temp-value" id="t1">--.- °C</span><span class="temp-detail" id="tstate1">Нет данных</span></button>
<button class="card temp" data-temp="2"><span class="temp-name">Теплоноситель</span><span class="temp-value" id="t2">--.- °C</span><span class="temp-detail" id="tstate2">Нет данных</span></button>
<button class="card temp temp-fuel" data-page="fuel"><span class="temp-name">Топливо</span><span class="temp-value" id="t3">--.- °C</span><svg id="fuel-canister" class="canister" data-state="unknown" viewBox="0 0 120 148" role="img" aria-label="Уровень топлива: нет данных"><title id="canister-title">Нет данных</title><rect id="canister-fill" class="can-fill" x="18" y="136" width="84" height="0" rx="5"/><g class="can-outline"><rect x="12" y="24" width="96" height="118" rx="10"/><path d="M62 24V4H99V24M21 18V10H41V18Z"/></g><path id="canister-cross" class="can-cross" d="M43 62L78 104M78 62L43 104" visibility="hidden"/><text id="canister-symbol" class="can-symbol" x="60" y="101" text-anchor="middle">?</text></svg><span class="temp-detail" id="tstate3">Нет данных</span></button>
</div>
<div id="temperature-detail" class="card section hidden"><div class="stack" style="justify-content:space-between"><h2 id="detail-title">Температура</h2><button data-action="close-detail" class="ghost">Закрыть</button></div><p id="detail-meta"></p><svg id="detail-chart" class="detail-chart" viewBox="0 0 400 100" preserveAspectRatio="none" aria-label="История измерений"></svg><p id="detail-stats" class="tiny"></p></div></section>
<section id="equipment" class="page hidden"><h2>Оборудование</h2><div class="equip-list" id="equipment-list"></div><p class="tiny">Управление появится после подключения и проверки исполнительных контроллеров.</p></section>
<section id="fuel" class="page hidden"><h2>Топливо</h2><div class="two"><div class="card"><h3>Уровень</h3><p id="fuel-status" class="fuel-summary-value">Нет данных</p><div class="row"><span>Нижний датчик</span><span id="lower-status">Нет данных</span></div><div class="row"><span>Верхний датчик</span><span id="upper-status">Нет данных</span></div><details><summary>Как определяется уровень</summary><p class="tiny">Два геркона дают три состояния, без расчёта литров. Для принятой схемы контактов используйте режим NC: оба разомкнуты — полная канистра; замкнут верхний — половина; замкнуты оба — пустая. Обрыв провода неотличим от размыкания контакта.</p></details></div><div class="card"><h3>Температура и подогрев</h3><div class="row"><span>Температура топлива</span><b id="fuel-temp">--.- °C</b></div><div class="row"><span>Контроллер подогрева</span><span id="fuel-controller">Не подключён</span></div><button class="ghost section" data-temp="3">История температуры</button><p class="tiny">До 80 °C — требование проекта, не действующая уставка. Команд нагрева нет.</p></div></div></section>
<section id="settings" class="page hidden">
 <h2>Настройки</h2><div id="settings-menu" class="menu-grid">
<button class="menu-item" data-tab="wifi"><strong>Wi-Fi</strong><span>Сеть и подключение</span><b>›</b></button>
<button class="menu-item" data-tab="screen"><strong>Экран</strong><span>Сон и пробуждение</span><b>›</b></button>
<button class="menu-item" data-tab="ble"><strong>Bluetooth</strong><span>Устройства поблизости</span><b>›</b></button>
<button class="menu-item" data-tab="clock"><strong>Часы</strong><span>Время и часовой пояс</span><b>›</b></button>
<button class="menu-item" data-page="ha"><strong>Home Assistant</strong><span>Подключение через MQTT</span><b>›</b></button>
<button class="menu-item" data-page="connections"><strong>Датчики</strong><span>Назначения и контакты JP1</span><b>›</b></button>
<button class="menu-item" data-page="service"><strong>Сервис</strong><span>microSD, диагностика, версия</span><b>›</b></button>
</div><div id="settings-back" class="subhead hidden"><button data-action="settings-root" aria-label="Назад к настройкам">‹ Настройки</button></div>
 <div id="tab-wifi" class="tab card"><h3>Wi-Fi · 2.4 ГГц</h3>
 <div class="row"><span>Соединение</span><strong id="wifi-connected">Не подключено</strong></div>
 <div class="row"><span>SSID</span><span id="wifi-ssid">Доступно после сопряжения</span></div>
 <div class="row"><span>IP</span><span id="wifi-ip">—</span></div>
 <div class="row"><span>RSSI</span><span id="wifi-rssi">—</span></div>
 <p class="hint" id="wifi-message"></p>
 <div class="actions"><button class="primary" data-action="wifi_scan" data-write="1">Найти сети</button></div><details class="details"><summary>Отключение и удаление сети</summary><div class="actions"><button data-action="wifi_disconnect" data-write="1">Отключить</button><button class="danger" data-action="wifi_forget" data-write="1">Забыть сеть</button></div></details>
 <div class="field"><label>Найденные сети</label><div id="ap-list" class="feed"><p class="muted">Нажмите «Найти сети»</p></div></div>
 <form id="wifi-form" autocomplete="off"><div class="field"><label for="ssid">SSID (до 32 байт)</label><input id="ssid" name="ssid" maxlength="32" required placeholder="Имя сети"></div>
 <div class="field"><label for="wifi-pass">Пароль сети</label><input id="wifi-pass" name="password" type="password" autocomplete="new-password" maxlength="64" placeholder="WPA2/WPA3 пароль"></div>
 <div class="stack"><label style="display:flex;align-items:center;gap:8px;margin:0"><input type="checkbox" id="wifi-open">Открытая сеть, без пароля</label><button type="submit" class="primary" data-write="1">Подключиться</button></div></form>
 <div class="notice warn">При смене Wi-Fi веб-страница может потерять соединение; новый IP смотрите на дисплее. Не публикуйте пароль сети.</div></div>
 <div id="tab-ble" class="tab card hidden"><h3>Поиск Bluetooth LE</h3><p>Поиск объявлений устройств. Сопряжение и подключение BLE не выполняются.</p><div class="actions"><button data-action="ble_scan" data-write="1">Поиск BLE (5 секунд)</button></div><p class="hint" id="ble-message"></p><div id="ble-list" class="feed"></div></div>
 <div id="tab-clock" class="tab card hidden"><h3>Часы и часовой пояс</h3><div class="row"><span>Время панели</span><strong id="clock-current">—</strong></div><div class="row"><span>Часовой пояс</span><strong id="zone-name">—</strong></div><p class="hint" id="time-message"></p>
 <div class="stack"><select id="zone-select" aria-label="Часовой пояс"><option value="0">UTC</option><option value="1">Москва UTC+3</option><option value="2">Берлин CET/CEST</option></select><button data-action="zone" data-write="1">Установить часовой пояс</button></div>
 <div class="field"><label for="manual-clock">Установить вручную (ГГГГ-ММ-ДД ЧЧ:ММ)</label><input id="manual-clock" placeholder="2026-11-02 18:45" maxlength="16"></div><button data-action="clock" data-write="1">Установить время</button><p class="muted tiny">После подключения Wi-Fi время синхронизируется NTP.</p></div>
 <div id="tab-screen" class="tab card hidden"><h3>Подсветка и пробуждение касанием</h3><div class="row"><span>Текущий таймер</span><b id="screen-current">Не выключать</b></div>
 <div class="row"><span>Сенсор касаний</span><span id="touch-ready">—</span></div><div class="field"><label for="screen-select">Выключение после бездействия</label><select id="screen-select"><option value="0">Не выключать</option><option value="30">30 секунд</option><option value="60">1 минута</option><option value="120">2 минуты</option><option value="300">5 минут</option><option value="600">10 минут</option><option value="1800">30 минут</option></select></div>
 <button data-action="screen" data-write="1">Сохранить таймер</button><p id="screen-message" class="hint"></p><p class="muted tiny">Первое касание только пробуждает экран. Wi-Fi и сам контроллер продолжают работать.</p></div>
</section>
<section id="connections" class="page hidden">
<div class="subhead"><button data-action="settings-root">‹ Настройки</button><h2>Датчики</h2></div>
<div class="notice warn">Настройка входных датчиков на JP1. Подключайте только согласованные цепи 3,3 В. Управляющие выходы не подключены.</div>
<div class="card">
  <div class="row"><span>Ревизия конфигурации</span><strong id="cfg-revision">0</strong></div>
  <div class="row"><span>Применение GPIO</span><strong id="cfg-state">Не активировано</strong></div>
  <div class="field"><label for="cfg-channel">Канал / назначение</label>
  <select id="cfg-channel">
    <option value="0">Температура котельной</option>
    <option value="1">Температура улицы</option>
    <option value="2">Температура теплоносителя</option>
    <option value="3">Температура топлива</option>
    <option value="4">Нижний геркон топлива</option>
    <option value="5">Верхний геркон топлива</option>
  </select></div>
  <div class="field"><label for="cfg-type">Тип датчика/источника</label>
  <select id="cfg-type">
    <option value="0">Не назначено</option>
    <option value="1">DS18B20 / OneWire (только температура)</option>
    <option value="2">Геркон NO: замкнут = достигнут</option>
    <option value="3">Геркон NC: разомкнут = достигнут</option>
    <option value="4">Modbus RTU / RS-485 (требует контроллер)</option>
    <option value="5">MQTT (требует настроенный брокер)</option>
  </select></div>
  <div class="field"><label for="cfg-gpio">GPIO ESP32-P4 (для локального датчика)</label>
  <select id="cfg-gpio">
   <option value="-1">Без GPIO (не назначено / удалённый источник)</option>
   <option value="52">GPIO52 — JP1 контакт 7</option>
   <option value="33">GPIO33 — JP1 контакт 8</option>
   <option value="51">GPIO51 — JP1 контакт 9</option>
   <option value="31">GPIO31 — JP1 контакт 10</option>
   <option value="50">GPIO50 — JP1 контакт 11</option>
   <option value="30">GPIO30 — JP1 контакт 12</option>
   <option value="49">GPIO49 — JP1 контакт 13</option>
   <option value="29">GPIO29 — JP1 контакт 14</option>
   <option value="34">GPIO34 — JP1 контакт 17</option>
   <option value="32">GPIO32 — JP1 контакт 19</option>
   <option value="28">GPIO28 — JP1 контакт 21</option>
  </select>
  <p class="tiny muted">Только физические выводы JP1. GPIO35 — BOOT, его использовать нельзя. Датчики DS18B20 подключаются по одному на линию с внешней подтяжкой 4,7 кОм к 3,3 В. Герконы — контакт GPIO ↔ GND; обрыв провода обычным входом не диагностируется.</p></div>
  <div class="field"><label for="cfg-address">Адрес Modbus (0–247)</label><input id="cfg-address" type="number" min="0" max="247" value="0"></div>
  <div class="field"><label for="cfg-debounce">Подавление дребезга геркона, мс</label><input id="cfg-debounce" type="number" min="0" max="5000" value="50"></div>
  <div class="actions"><button class="primary" data-action="connection_set" data-write="1">Применить реальное подключение</button><button class="ghost" data-action="connection_reset" data-write="1">Отключить канал</button><button class="ghost" data-action="connection_rollback" data-write="1">Вернуть прошлую конфигурацию</button></div>
  <p id="cfg-message" class="hint"></p>
</div>
<div class="card section">
<h3>Сохранённые назначения</h3>
<div id="cfg-table" class="feed muted">Нет конфигурации.</div>
<p class="tiny muted">Поддерживаются реальные входные датчики на JP1: DS18B20 (один на GPIO) и герконы NO/NC. До подключения датчика статус «Не найден». Modbus/MQTT пока только описываются, без работы драйвера. Реле, котлы и нагреватели не управляются.</p>
</div>
</section>
<section id="ha" class="page hidden">
 <div class="subhead"><button data-action="settings-root">‹ Настройки</button><h2>Home Assistant</h2></div>
 <div class="notice">Панель передаёт показания через MQTT Discovery. Нужен MQTT-брокер, подключённый к Home Assistant. Это не адрес веб-интерфейса HA (порт 8123). Управление нагревом и реле не передаётся.</div>
 <div class="two"><div class="card">
  <h3>Состояние интеграции</h3>
  <div class="row"><span>MQTT</span><strong id="ha-state">Выключен</strong></div>
  <div class="row"><span>Идентификатор панели</span><code id="ha-id">—</code></div>
  <div class="row"><span>Пакетов обнаружения</span><span id="ha-discovery-count">0</span></div>
  <p id="ha-message" class="hint"></p>
  <button data-action="ha_discover" data-write="1">Повторить обнаружение</button>
  <p class="tiny">12 сущностей: четыре температуры, два геркона, уровень топлива, Wi-Fi, время работы, память и microSD. Без настоящего датчика или при устаревших данных сущность недоступна.</p>
 </div><form id="ha-form" class="card" autocomplete="off">
  <h3>Подключение к MQTT-брокеру</h3>
  <label class="stack"><input type="checkbox" id="ha-enabled">Включить интеграцию</label>
  <div class="field"><label for="ha-host">Адрес брокера (IP или имя, без http://)</label><input id="ha-host" maxlength="128" placeholder="192.168.1.10" spellcheck="false"></div>
  <div class="field"><label for="ha-port">Порт MQTT</label><input id="ha-port" type="number" min="1" max="65535" value="1883" required></div>
  <div class="field"><label for="ha-user">Логин MQTT</label><input id="ha-user" maxlength="64" autocomplete="off"></div>
  <div class="field"><label for="ha-password">Пароль MQTT (пусто — оставить сохранённый)</label><input id="ha-password" type="password" maxlength="128" autocomplete="new-password"><small id="ha-password-info">Пароль не сохранён</small></div>
  <label class="stack"><input id="ha-clear-password" type="checkbox">Удалить сохранённый пароль</label>
  <div class="field"><label for="ha-prefix">Префикс Discovery</label><input id="ha-prefix" maxlength="64" value="homeassistant" pattern="[A-Za-z0-9_-]+" required></div>
  <div class="notice warn">Эта версия использует MQTT/TCP без TLS; веб-настройки передаются по HTTP. Только доверенная локальная сеть, без проброса портов. Пароль хранится в NVS панели, не выводится через API. Используйте отдельную учётную запись брокера.</div>
  <label class="stack"><input id="ha-lan" type="checkbox">Подтверждаю использование доверенной локальной сети</label>
  <div class="actions"><button type="submit" class="primary" data-write="1">Сохранить</button><button type="button" data-action="ha_reload" class="ghost">Перечитать настройки</button></div>
  <p class="tiny">Сохранение доступно после сопряжения с дисплеем. Для смены адреса брокера или логина введите пароль заново либо явно удалите его. Отключение попробует удалить Discovery на прежнем брокере; недоступный старый брокер может потребовать ручной очистки.</p>
 </form></div>
</section>
<section id="service" class="page hidden">
<div class="subhead"><button data-action="settings-root">‹ Настройки</button><h2>Сервис</h2></div><details class="details"><summary>Диагностика ESP32-P4 / ESP32-C6</summary><div class="two service-grid">
<div class="card"><h3>ESP32-P4</h3>
<div class="row"><span>Версия панели</span><b id="service-version">—</b></div>
<div class="row"><span>Время работы</span><b id="uptime">—</b></div>
<div class="row"><span>Свободная память</span><b id="heap">—</b></div>
<div class="row"><span>Первый кадр</span><b id="first-frame">—</b></div>
<div class="row"><span>Инициализация дисплея</span><b id="display-ms">—</b></div>
<div class="row"><span>Инициализация LVGL / касания</span><b id="lvgl-touch">—</b></div>
</div><div class="card"><h3>ESP32-C6 / сеть</h3>
<div class="row"><span>ESP-Hosted (P4)</span><b id="host-version">—</b></div>
<div class="row"><span>Прошивка C6</span><b id="c6-version">—</b></div>
<div class="row"><span>Wi-Fi</span><span id="service-wifi">—</span></div>
<div class="row"><span>IP панели</span><span id="service-ip">—</span></div>
<div class="row"><span>Действие веб-сервера</span><span>Наблюдение + сопряжение</span></div>
</div></div></details>
<div class="card section">
 <h3>Накопитель microSD</h3>
 <div class="row"><span>Состояние</span><b id="sd-state">Проверка…</b></div>
 <div class="row"><span>Ёмкость карты</span><b id="sd-card">—</b></div>
 <div class="row"><span>Размер файловой системы</span><b id="sd-total">—</b></div>
 <div class="row"><span>Использовано</span><b id="sd-used">—</b></div>
 <div class="row"><span>Свободно</span><b id="sd-free">—</b></div>
 <div class="row"><span>Журналы</span><b id="sd-logging">—</b></div>
 <div class="row"><span>Записано событий / измерений</span><b id="sd-events">—</b></div>
 <p id="sd-message" class="hint"></p>
 <p class="tiny muted">Каталог: <code>/boiler-panel/logs/</code>. Только данные этой панели. Форматирование при загрузке отключено; произвольные файлы карты недоступны через HTTP.</p>
 <details class="details danger-zone"><summary>Форматирование microSD</summary><div class="notice warn">
  <strong>Форматирование microSD — полное удаление данных!</strong>
  <p>Существующие файлы и разделы карты будут уничтожены. Будет создана совместимая FAT-файловая система.
  Настройки ESP32-P4, прошивка ESP32-C6 и накопители компьютера не затрагиваются.</p>
  <p>Нужно сопряжение браузера с дисплеем. После запроса в браузере подтвердите стирание <strong>физически на дисплее</strong> в разделе «Настройки → Сервис → microSD» в течение 90 секунд.</p>
 </div>
 <div class="field">
  <label for="sd-format-phrase">Чтобы разрешить операцию, введите точно: СТЕРЕТЬ MICROSD</label>
  <input id="sd-format-phrase" autocomplete="off" spellcheck="false" maxlength="24" placeholder="СТЕРЕТЬ MICROSD">
 </div>
 <div class="actions"><button class="danger" id="sd-format-button" data-action="sd_format_prepare" data-write="1">Запросить форматирование microSD</button></div>
 </details><div id="sd-format-progress" class="notice hidden" role="status"></div>
</div>
<div class="actions"><button data-action="refresh" class="primary">Обновить сведения</button></div>
<div class="notice">Эта страница не обновляет прошивку. Управляющие выходы отключены.</div>
</section><footer><span>Boiler Panel <span id="version">подключение...</span></span><span id="net" class="status">Подключение…</span><span id="last-update">Ожидание панели</span></footer></div><div id="toast" class="hidden" role="status" aria-live="polite"></div><script>
(()=>{
"use strict";
const $=id=>document.getElementById(id);
const names=["Бойлер косвенного нагрева","Электрокотёл","Контур 1","Контур 2","Контур 3","Дизельный отопитель","Подогрев топлива"];
const tnames=["В котельной","На улице","Теплоноситель","Топливо"];
let token=sessionStorage.getItem("boiler_panel_token")||"",current=null,page="home",tab="",detail=-1;
let lastServerError="",lastToast=null,refreshInFlight=false,serverOnline=false;
let pairTicket="",pairDeadline=0,pairGeneration=0,pairBusy=false;
let haFormRevision=null,haDirty=false;
const timeout=s=>s===0?"Не выключать":s<60?s+" секунд":s/60+" минут";
const storageBytes=n=>Number.isFinite(n)&&n>0
    ?(n>=1073741824?(n/1073741824).toFixed(2)+" ГБ":
      n>=1048576?(n/1048576).toFixed(1)+" МБ":
      (n/1024).toFixed(0)+" КБ"):"—";
function text(id,v){const n=$(id);if(n)n.textContent=v==null?"—":String(v)}
function show(id,on){const n=$(id);if(!n)return;if(id==="pairbox"){if(on&&!n.open)n.showModal();if(!on&&n.open)n.close();return;}n.classList.toggle("hidden",!on)}
function notice(message,problem=false){
 const n=$("toast");n.textContent=message;n.style.color=problem?"#ffacaf":"#d6f9ff";show("toast",true);
 if(lastToast)clearTimeout(lastToast);lastToast=setTimeout(()=>show("toast",false),4700);
}
function numeric(n){return Number.isFinite(Number(n))?Number(n):0}
function apiHeaders(){const h={};if(token)h.Authorization="Bearer "+token;return h}
function drawPage(){
 for(const n of document.querySelectorAll(".page"))n.classList.toggle("hidden",n.id!==page);
 for(const n of document.querySelectorAll(".topnav [data-page]"))n.classList.toggle("active",n.dataset.page===(["connections","ha","service"].includes(page)?"settings":page));
 show("settings-menu",!tab);show("settings-back",!!tab);
 for(const n of document.querySelectorAll(".tab"))n.classList.toggle("hidden",n.id!=="tab-"+tab);
 for(const n of document.querySelectorAll("[data-tab]"))n.classList.toggle("active",n.dataset.tab===tab);
 updateView();
}
function setTab(newTab){tab=newTab;page="settings";drawPage()}
function updateDetail(){
 if(detail<0||!current){show("temperature-detail",false);return}
 const t=current.temperatures&&current.temperatures[detail];if(!t)return;
 show("temperature-detail",true);
 text("detail-title",tnames[detail]+" — подробности");
 text("detail-meta",t.valid?"Свежие данные":"Датчик не назначен или данные устарели");
 text("detail-stats",t.ever?("Минимум: "+t.minimum+" °C · Максимум: "+t.maximum+" °C · Измерений в памяти: "+t.history.length+" · "+(t.trend==null?"Тренд отсутствует":"Тренд "+(t.trend>0?"+":"")+t.trend+" °C")):"Реальных измерений и истории пока нет");
 const svg=$("detail-chart");
 while(svg.firstChild)svg.removeChild(svg.firstChild);
 const vals=(t.history||[]).filter(x=>Number.isFinite(x.value));
 if(vals.length<2)return;
 const min=Math.min(...vals.map(v=>v.value)),max=Math.max(...vals.map(v=>v.value));
 const range=Math.max(max-min,0.5),step=390/(vals.length-1);
 const points=vals.map((v,i)=>(5+i*step).toFixed(1)+","+(92-84*(v.value-min)/range).toFixed(1)).join(" ");
 const line=document.createElementNS("http://www.w3.org/2000/svg","polyline");
 line.setAttribute("points",points);line.setAttribute("fill","none");line.setAttribute("stroke","#71ccfa");line.setAttribute("stroke-width","2.5");svg.append(line);
}
function renderEquipment(){
 const items=current?.equipment||[],holder=$("equipment-list");holder.replaceChildren();
 for(const [index,title] of [[0,"Бойлер"],[1,"Электрокотёл"],[5,"Дизельный отопитель"],[6,"Подогрев топлива"]]){
  const card=document.createElement("details");card.className="card equip";
  const summary=document.createElement("summary"),heading=document.createElement("h3"),status=document.createElement("p");
  heading.textContent=title;status.textContent=items[index]?.connected?"Подключён":"Не подключён";
  summary.append(heading,status);card.append(summary);
  const explanation=document.createElement("p");explanation.textContent="Управление недоступно до подключения и проверки контроллера.";card.append(explanation);
  if(index===1)for(const n of [2,3,4]){const row=document.createElement("div");row.className="row";const name=document.createElement("span"),value=document.createElement("span");name.textContent=items[n]?.name||names[n];value.textContent=items[n]?.connected?"Подключён":"Нет данных";row.append(name,value);card.append(row);}
  holder.append(card);
 }
}
function renderAP(){
 const d=current||{},aps=d.wifi&&d.wifi.aps||[],list=$("ap-list");
 list.replaceChildren();
 if(!d.authorized){const p=document.createElement("p");p.textContent="После сопряжения можно искать сети и подключаться.";list.append(p);return}
 if(!aps.length){const p=document.createElement("p");p.textContent="Сетей в списке нет. Нажмите «Найти сети».";list.append(p);return}
 for(const ap of aps){
   const b=document.createElement("button");b.className="ghost";b.style.width="100%";b.style.textAlign="left";b.style.margin="4px 0";
   b.textContent=ap.ssid+" · "+ap.rssi+" dBm · "+(ap.auth===0?"Открытая":ap.supported?"WPA Personal":"Не поддерживается");
   b.disabled=!ap.supported;
   b.addEventListener("click",()=>{
     $("ssid").value=ap.ssid;$("wifi-open").checked=ap.auth===0;
     $("wifi-pass").disabled=ap.auth===0;
     if(ap.auth===0)$("wifi-pass").value="";
     notice("Сеть выбрана. Введите пароль и нажмите «Подключиться».");
   });list.append(b);
 }
}
function renderBLE(){
 const d=current||{},items=d.ble&&d.ble.devices||[],list=$("ble-list");
 list.replaceChildren();
 if(!d.authorized){list.textContent="Сопрягите браузер с дисплеем для поиска BLE.";return}
 if(!items.length){list.textContent="Устройства не найдены или поиск ещё не выполнялся.";return}
 for(const e of items){
   const row=document.createElement("div");row.className="row";
   const name=document.createElement("span");name.textContent=e.name||"Без имени";
   const rssi=document.createElement("span");rssi.className="muted";rssi.textContent=(e.address||"")+" · "+e.rssi+" dBm";
   row.append(name,rssi);list.append(row);
 }
}
function renderHA(){
 const d=current||{},h=d.home_assistant||{},admin=!!d.authorized;
 text("ha-state",!h.ready?"Модуль недоступен":!h.enabled?"Выключен":h.connected?"Подключён к брокеру":"Нет соединения");
 text("ha-id",h.id||"—");text("ha-message",(h.message||"")+(h.error_code?" · код "+h.error_code:""));
 text("ha-discovery-count",h.discovery_messages||0);
 text("ha-password-info",admin?(h.password_set?"Пароль сохранён, его содержимое скрыто":"Пароль не сохранён"):"Требуется сопряжение");
 for(const el of $("ha-form").querySelectorAll("input"))el.disabled=!admin||!h.ready;
 if(!admin){
  for(const id of ["ha-host","ha-user","ha-password"])$(id).value="";
  haFormRevision=null;haDirty=false;return;
 }
 if(!haDirty&&(haFormRevision===null||haFormRevision!==h.revision)){
  $("ha-enabled").checked=!!h.enabled;$("ha-host").value=h.host||"";
  $("ha-port").value=h.port||1883;$("ha-user").value=h.username||"";
  $("ha-prefix").value=h.discovery_prefix||"homeassistant";
  $("ha-password").value="";$("ha-clear-password").checked=false;haFormRevision=h.revision;
 }
}
async function saveHA(e){
 e.preventDefault();if(!current||!current.authorized||haFormRevision===null){startPairing();return;}
 const enabled=$("ha-enabled").checked;
 if(enabled&&!$("ha-lan").checked){notice("Подтвердите доверенную локальную сеть",true);return;}
 if($("ha-password").value&&$("ha-clear-password").checked){notice("Выберите новый пароль ИЛИ удаление сохранённого",true);return;}
 const mode=$("ha-clear-password").checked?"clear":$("ha-password").value?"set":"keep";
 const values={action:"ha_save",enabled:enabled?"1":"0",host:$("ha-host").value.trim(),port:$("ha-port").value,
   username:$("ha-user").value,password:$("ha-password").value,password_mode:mode,
   prefix:$("ha-prefix").value.trim(),revision:String(haFormRevision),lan_confirm:$("ha-lan").checked?"1":"0"};
 $("ha-password").value="";
 try{
  await sendPost("/api/command",values,true);haDirty=false;haFormRevision=null;
  notice("Настройки сохранены. Соединение с брокером проверяется отдельно.");await poll();
 }catch(e){notice("Не сохранено: "+e.message,true);}
 finally{values.password="";}
}
async function request(path,options){const c=new AbortController(),timer=setTimeout(()=>c.abort(),8000);try{return await fetch(path,{...options,signal:c.signal});}finally{clearTimeout(timer);}}
function errorText(code){return ({same_origin_required:"Запрос разрешён только из локальной страницы панели",display_not_ready:"Дисплей ещё запускается",pairing_in_progress:"Код уже запрошен в другом браузере. Завершите тот запрос или дождитесь истечения срока.",pairing_rate_limited:"Слишком много попыток. Подождите до двух минут и запросите новый код.",code_invalid_or_expired:"Неверный или просроченный код",pair_required:"Выполните сопряжение",invalid_code:"Введите шесть цифр",ha_configuration_changed_reload:"Настройки изменились. Перечитайте форму",ha_invalid_or_password_required_for_new_broker:"Проверьте адрес брокера и заново введите пароль при смене сервера"})[code]||code;}
function updateCanister(f){
 let state="unknown",title="Нет данных",height=0,symbol="?";
 if(f?.lower?.valid&&f?.upper?.valid){
  if(f.lower.age_s>=30||f.upper.age_s>=30){state="stale";title="Данные устарели";}
  else if(f.upper.reached&&f.lower.reached){state="full";title="Полная";height=106;symbol="";}
  else if(f.lower.reached&&!f.upper.reached){state="half";title="Половина";height=53;symbol="";}
  else if(!f.lower.reached&&!f.upper.reached){state="empty";title="Пустая";symbol="!";}
  else{state="invalid";title="Проверьте герконы";}
 }
 const icon=$("fuel-canister");icon.dataset.state=state;icon.setAttribute("aria-label","Уровень топлива: "+title);
 text("canister-title",title);text("canister-symbol",symbol);
 $("canister-fill").setAttribute("height",height);$("canister-fill").setAttribute("y",136-height);
 $("canister-cross").setAttribute("visibility",height?"visible":"hidden");
 text("fuel-status",title);
}
function markOffline(){
 for(let i=0;i<4;i++){text("t"+i,"--.- °C");text("tstate"+i,"Нет связи");}
 text("fuel-temp","--.- °C");text("lower-status","Нет связи");text("upper-status","Нет связи");updateCanister(null);
}
function updateView(){
 const d=current;if(!d)return;
 text("version",d.version);text("service-version",d.version);
 text("clock",d.clock||"Нет времени");text("clock-current",d.clock||"Нет времени");
 text("net",d.wifi&&d.wifi.connected?"Wi-Fi подключён":"Нет Wi-Fi");
 $("net").className="status"+(d.wifi&&d.wifi.connected?" ok":"");
 text("authstate",d.authorized?"Доступ открыт":"Просмотр");
 text("web-auth",d.authorized?"Сопряжено (1 час)":"Только просмотр");
 show("pairbutton",!d.authorized);show("logout",d.authorized);
 text("last-update","Обновлено: "+new Date().toLocaleTimeString("ru-RU"));
 text("home-fuel",d.fuel_level_text||"Нет данных");
 text("fuel-summary","Уровень: "+(d.fuel_level_text||"Нет данных"));
 text("home-wifi",d.wifi&&d.wifi.connected?"Подключено":"Не подключено");
 for(let i=0;i<4;i++){
   const t=d.temperatures&&d.temperatures[i]||{};
   const value=t.valid&&typeof t.value==="number"?t.value.toFixed(1)+" °C":"--.- °C";
   text("t"+i,value);text("tstate"+i,t.valid?"":t.ever?"Устарело":"Нет данных");
   if(i===3)text("fuel-temp",value);
 }
 updateDetail();
 text("fuel-status",d.fuel_level_text);
 const reed=(item)=>item&&item.valid?(item.reached?"Уровень достигнут":"Уровень не достигнут"):"Нет данных";
 text("lower-status",reed(d.fuel&&d.fuel.lower));text("upper-status",reed(d.fuel&&d.fuel.upper));
 text("fuel-controller","Не подключён");
 const wifi=d.wifi||{},ble=d.ble||{},screen=d.screen||{},tm=d.time||{},service=d.service||{};
 text("wifi-connected",wifi.connected?"Подключено":"Не подключено");
 text("wifi-ssid",d.authorized?(wifi.ssid||"Нет данных"):"Сопряжение требуется");
 text("wifi-ip",wifi.ip||"—");text("wifi-rssi",wifi.connected?wifi.rssi+" dBm":"—");
 text("wifi-message",wifi.message||"");
 text("ble-message",ble.message||"");
 text("zone-name",tm.zone_name||"—");text("time-message",tm.message||"");
 text("screen-current",timeout(screen.timeout_s||0));
 text("touch-ready",screen.touch_ready?"Готов":"Недоступен");
 text("screen-message",screen.message||"");
 text("uptime",Math.floor(d.uptime_s/60)+" мин "+(d.uptime_s%60)+" сек");
 text("heap",Math.round((service.free_heap||0)/1024)+" КБ");
 text("first-frame",service.first_frame_ms+" мс");
 text("display-ms",service.display_ms+" мс");
 text("lvgl-touch",service.lvgl_ms+" / "+service.touch_ms+" мс");
 text("host-version",d.host_version||"—");text("c6-version",d.c6_version||"—");
 text("service-wifi",wifi.connected?"Подключён":"Не подключён");text("service-ip",wifi.ip||"—");
 const sd=d.storage||{};
 text("sd-state",sd.mounted?"Карта смонтирована":sd.started?"Карта не смонтирована":"Инициализация");
 text("sd-card",storageBytes(sd.card_bytes));
 text("sd-total",storageBytes(sd.total_bytes));
 text("sd-used",sd.mounted?storageBytes(sd.used_bytes):"—");
 text("sd-free",sd.mounted?storageBytes(sd.free_bytes):"—");
 text("sd-logging",sd.logging?"Запись включена":"Отключена");
 text("sd-events",(sd.written_events||0)+" / "+(sd.written_samples||0));
 text("sd-message",sd.message||"");
 const cfg=d.connections||{};
 text("cfg-revision",cfg.revision||0);
 text("cfg-state",cfg.drivers_active?"Считывание входов запущено":"Нет активных местных датчиков");
 text("cfg-message",cfg.message||"");
 const holder=$("cfg-table");if(holder){
    holder.replaceChildren();
    const channels=["Котельная","Улица","Теплоноситель","Топливо","Нижний геркон","Верхний геркон"];
    for(const item of (cfg.items||[])){
      const row=document.createElement("div");row.className="row";
      const title=document.createElement("span");title.textContent=channels[item.channel]||"Канал";
      const detail=document.createElement("span");
      detail.textContent=(item.type||"none")+" · "+
        (item.gpio>=0?"GPIO "+item.gpio+" / JP1-"+(item.jp1_pin||"?"):"удалённый вход")+
        (item.active?(item.online?" · показания получены":" · ожидание датчика"):" · неактивно")+
        (item.rom?" · ROM "+item.rom:"")+
        (item.status?" · "+item.status:"");
      row.append(title,detail);holder.append(row);
    }
 }
 const formatPhases=["Нет запроса","Ожидается подтверждение на дисплее","Запрос в очереди","Форматирование microSD","Проверка чтения/записи","Форматирование завершено","Ошибка форматирования","Отменено на дисплее","Время подтверждения истекло"];
 const phase=numeric(sd.format_state||0);
 const formatActive=phase>=1&&phase<=4;
 show("sd-format-progress",phase!==0);
 text("sd-format-progress",(formatPhases[phase]||"Неизвестное состояние")+
   (phase===1?" · осталось "+(sd.format_remaining_s||0)+" сек":"")+
   (sd.format_message?" · "+sd.format_message:""));
 const formButton=$("sd-format-button");
 formButton.disabled=!d.authorized || !!sd.format_attempted || formatActive;
 // The last step still requires physically touching the separate panel button.
 if(document.activeElement!==$("zone-select"))$("zone-select").value=String(tm.zone||0);
 if(document.activeElement!==$("screen-select"))$("screen-select").value=String(screen.timeout_s||0);
 for(const b of document.querySelectorAll('[data-write="1"]')){
   const wifiAction=b.dataset.action&&b.dataset.action.startsWith("wifi_");
   const bleAction=b.dataset.action==="ble_scan";
   const needsTouch=b.dataset.action==="screen"&&numeric($("screen-select").value)>0;
   b.disabled=!d.authorized || (wifi.busy&&(wifiAction||bleAction||b.dataset.action==="zone"||b.dataset.action==="clock"||b.dataset.action==="screen"))
     || (needsTouch&&!screen.touch_ready) || (!wifi.supported&&wifiAction) || (!ble.supported&&bleAction)
     || (b.dataset.action==="sd_format_prepare" && (!!sd.format_attempted || formatActive));
 }
 renderAP();renderBLE();renderEquipment();renderHA();updateCanister(d.fuel);
 if(!serverOnline)markOffline();
 for(const b of document.querySelectorAll('[data-action="ha_discover"]'))b.disabled=!d.authorized||!d.home_assistant?.enabled;
}
async function sendPost(path,data,secret){
 const headers={"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8","X-Boiler-Pairing":"1"};
 if(secret&&token)headers.Authorization="Bearer "+token;
 const r=await request(path,{method:"POST",headers,body:new URLSearchParams(data),cache:"no-store"});
 let obj;
 try{obj=await r.json()}catch(_){obj={error:"invalid_response"}}
 if(r.status===401){token="";sessionStorage.removeItem("boiler_panel_token");}
 if(!r.ok)throw Error(errorText(obj.error||("HTTP "+r.status)));
 return obj;
}
async function command(action,options={}){
 if(!token){startPairing();return}
 try{
   await sendPost("/api/command",Object.assign({action},options),true);
   notice("Запрос принят. Результат появится в состоянии панели.");
   setTimeout(poll,550);
 }catch(e){
   notice("Ошибка действия: "+e.message,true);
 }
}
async function startPairing(){
 show("pairbox",true);text("pair-error","");
 if(pairTicket&&pairDeadline>Date.now()){$("paircode").focus();return;}
 const generation=++pairGeneration;
 if(!globalThis.crypto?.getRandomValues){text("pair-error","Браузер не поддерживает безопасное сопряжение");return;}
 const bytes=new Uint8Array(16);crypto.getRandomValues(bytes);
 const ticket=Array.from(bytes,x=>x.toString(16).padStart(2,"0")).join("");pairTicket=ticket;pairDeadline=0;pairBusy=true;
 $("paircode").value="";$("pair-submit").disabled=true;text("pair-message","Запрашиваем код на дисплее…");
 try{
  const result=await sendPost("/api/pair/start",{request_id:ticket},false);
  if(generation!==pairGeneration){await sendPost("/api/pair/cancel",{request_id:ticket},false);return;}
  pairDeadline=Date.now()+result.expires_in_s*1000;
  text("pair-message","Код показан на панели слева от часов. Введите его здесь.");
  $("pair-submit").disabled=false;$("paircode").focus();
 }catch(e){if(generation===pairGeneration){pairTicket="";text("pair-error",e.message);text("pair-message","Код не запрошен. Повторите попытку.");}}
 finally{if(generation===pairGeneration)pairBusy=false;}
}
async function cancelPairing(){
 const ticket=pairTicket;++pairGeneration;pairTicket="";pairDeadline=0;pairBusy=false;
 $("paircode").value="";show("pairbox",false);
 if(ticket){try{await sendPost("/api/pair/cancel",{request_id:ticket},false);}catch(_){/* Code still expires on panel. */}}
}
async function login(){
 if(pairBusy)return;
 const code=$("paircode").value.trim();
 if(!/^[0-9]{6}$/.test(code)){text("pair-error","Введите шестизначный код с панели");return;}
 if(!pairTicket||Date.now()>=pairDeadline){text("pair-error","Запросите новый код");return;}
 pairBusy=true;$("pair-submit").disabled=true;
 try{
  const result=await sendPost("/api/pair",{code,request_id:pairTicket},false);
  token=result.token;sessionStorage.setItem("boiler_panel_token",token);
  pairTicket="";pairDeadline=0;++pairGeneration;$("paircode").value="";show("pairbox",false);
  notice("Сопряжение выполнено. Код на дисплее погашен.");await poll();
 }catch(e){text("pair-error",e.message);$("paircode").value="";$("paircode").focus();}
 finally{pairBusy=false;$("pair-submit").disabled=Date.now()>=pairDeadline;}
}
async function logout(){
 try{await sendPost("/api/logout",{},true)}catch(_){}
 token="";sessionStorage.removeItem("boiler_panel_token");
 notice("Веб-управление отключено");
 await poll();
}
async function requestStorageFormat(){
 if(!token){startPairing();return}
 const phrase=$("sd-format-phrase").value.trim();
 if(phrase!=="СТЕРЕТЬ MICROSD"){
   notice("Введите точную фразу: СТЕРЕТЬ MICROSD",true);return;
 }
 if(!confirm("Все файлы и разделы на microSD будут уничтожены. Продолжить запрос?"))return;
 try {
   const r=await sendPost("/api/command",{action:"sd_format_prepare",confirm:phrase},true);
   $("sd-format-phrase").value="";
   notice("Запрос принят. Теперь подтвердите стирание на ФИЗИЧЕСКОМ дисплее в «Сервис → microSD».");
   await poll();
 }catch(e){notice("Форматирование не запрошено: "+e.message,true)}
}
function handleAction(b){
 const action=b.dataset.action;
 if(action==="settings-root"){tab="";page="settings";drawPage();return;}
 if(action==="ha_reload"){haDirty=false;haFormRevision=null;renderHA();return;}
 if(action==="show-pair"||action==="pair-retry"){startPairing();return;}
 if(action==="hide-pair"){cancelPairing();return;}
 if(action==="pair"){login();return}
 if(action==="logout"){logout();return}
 if(action==="close-detail"){detail=-1;updateDetail();return}
 if(action==="refresh"){poll();return}
 if(action==="wifi_forget"&&!confirm("Удалить сохранённую сеть и отключить автоподключение на панели?"))return;
 if(action==="wifi_disconnect"&&!confirm("Отключить Wi-Fi? Веб-интерфейс может стать недоступным."))return;
 if(action==="zone"){command("zone",{value:$("zone-select").value});return}
 if(action==="clock"){command("clock",{value:$("manual-clock").value.trim()});return}
 if(action==="screen"){command("screen",{value:$("screen-select").value});return}
 if(action==="connection_set"){
   command("connection_set",{
     channel:$("cfg-channel").value,type:$("cfg-type").value,
     gpio:$("cfg-gpio").value,address:$("cfg-address").value,
     debounce:$("cfg-debounce").value
   });return;
 }
 if(action==="connection_rollback"){
   if(confirm("Восстановить предыдущую сохранённую конфигурацию датчиков?"))
     command("connection_rollback");
   return;
 }
 if(action==="connection_reset"){
   command("connection_set",{
     channel:$("cfg-channel").value,type:"0",
     gpio:"-1",address:"0",debounce:"50"
   });return;
 }
 if(action==="sd_format_prepare"){requestStorageFormat();return}
 command(action);
}
document.addEventListener("click",e=>{
 const target=e.target;
 if(!(target instanceof Element))return;
 const p=target.closest("[data-page]");
 if(p){page=p.dataset.page;if(page==="settings")tab="";detail=-1;drawPage();return;}
 const t=target.closest("[data-tab]");
 if(t){setTab(t.dataset.tab);return}
 const temperature=target.closest("[data-temp]");
 if(temperature){detail=Number(temperature.dataset.temp);page="home";drawPage();$("temperature-detail").scrollIntoView({behavior:"smooth",block:"nearest"});return}
 const button=target.closest("[data-action]");
 if(button){if(button.disabled)return;handleAction(button)}
});
$("ha-form").addEventListener("input",()=>{haDirty=true;});
$("ha-form").addEventListener("submit",saveHA);
$("cfg-type").addEventListener("change",e=>{
 const t=Number(e.target.value);
 $("cfg-gpio").disabled=t===0||t===4||t===5;
 if($("cfg-gpio").disabled)$("cfg-gpio").value="-1";
 $("cfg-address").disabled=t!==4;
 $("cfg-address").value=t===4?"1":"0";
 $("cfg-debounce").disabled=t!==2&&t!==3;
 $("cfg-debounce").value="50";
});
$("wifi-open").addEventListener("change",e=>{
 const isOpen=e.target.checked;$("wifi-pass").disabled=isOpen;
 if(isOpen)$("wifi-pass").value="";
});
$("wifi-form").addEventListener("submit",e=>{
 e.preventDefault();
 if(!token){startPairing();return}
 const ssid=$("ssid").value,pass=$("wifi-pass").value,open=$("wifi-open").checked;
 if(!ssid||new TextEncoder().encode(ssid).length>32){notice("SSID до 32 байт",true);return}
 if(!open&&!((pass.length>=8&&pass.length<=63)||/^[0-9a-fA-F]{64}$/.test(pass))){
   notice("Пароль WPA2/WPA3: 8–63 символа или 64 hex",true);return;
 }
 if(!confirm("Подключить панель к выбранной сети? Это может изменить её IP."))return;
 command("wifi_connect",{ssid,password:pass,open:open?"1":"0"});
 $("wifi-pass").value="";
});
async function poll(){
 if(refreshInFlight)return;
 refreshInFlight=true;
 try{
   const r=await request("/api/status",{cache:"no-store",headers:apiHeaders()});
   if(!r.ok)throw Error("HTTP "+r.status);
   const d=await r.json();
   if(!d.authorized&&token){token="";sessionStorage.removeItem("boiler_panel_token");notice("Сессия истекла. Повторите сопряжение.");}
   current=d;serverOnline=true;lastServerError="";updateView();
 }catch(e){
   if(lastServerError!==e.message)notice("Нет связи с панелью: "+e.message,true);
   lastServerError=e.message;serverOnline=false;markOffline();
   text("net","● Нет ответа панели");
   $("net").className="status";
   text("last-update","Сервер не отвечает, повторяем");
 }finally{refreshInFlight=false}
}
$("pair-form").addEventListener("submit",e=>{e.preventDefault();login();});
$("pairbox").addEventListener("cancel",e=>{e.preventDefault();cancelPairing();});
setInterval(()=>{if(!$("pairbox").open)return;const left=Math.max(0,Math.ceil((pairDeadline-Date.now())/1000));text("pair-expiry",pairDeadline?(left?"Осталось "+left+" с":"Срок кода истёк. Запросите новый."):"");if(!left)$("pair-submit").disabled=true;},500);
drawPage();poll();setInterval(poll,3000);
})();
</script></body></html>)BPHTML";
