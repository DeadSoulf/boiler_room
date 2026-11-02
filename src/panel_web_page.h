#pragma once
// Served directly from ESP32-P4. No CDN or external HTTP dependencies.
static const char kBoilerWebPage[] PROGMEM = R"BPHTML(<!doctype html><html lang="ru">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#09121f"><title>Котельная — управление</title>
<style>
:root{color-scheme:dark;--bg:#08131f;--card:#15273b;--card2:#101f30;--border:#2b4055;--text:#f1f6fc;--muted:#93a8c0;--sky:#74cbf5;--green:#67ddb3;--amber:#f1bb78;--red:#f49294}
*{box-sizing:border-box}html{font:15px/1.5 system-ui,Segoe UI,Arial,sans-serif;background:var(--bg);color:var(--text)}
body{margin:0;min-width:310px;background:radial-gradient(ellipse at 95% -8%,#12354b,transparent 48%),var(--bg);min-height:100vh}
.container{max-width:1200px;margin:auto;padding:16px clamp(12px,3vw,34px) 70px}
header{display:flex;justify-content:space-between;align-items:center;gap:14px;flex-wrap:wrap;padding:10px 0 22px;border-bottom:1px solid var(--border)}
.brand{display:flex;align-items:center;gap:12px}.mark{display:grid;place-items:center;width:46px;height:46px;background:#1e4354;color:var(--sky);font-size:27px;font-weight:bold;border-radius:14px}
h1{font-size:clamp(21px,3.5vw,31px);line-height:1.2;margin:0}h2{font-size:20px;margin:0 0 14px}h3{font-size:16px;margin:0 0 10px}
p{margin:6px 0;color:var(--muted)}.muted,.hint{color:var(--muted)}small{font-size:12px}
.toolbar{display:flex;align-items:center;gap:10px;flex-wrap:wrap}.status{border:1px solid var(--border);padding:8px 12px;border-radius:50px;color:var(--amber);font-size:13px}
.status.ok{color:var(--green)}.tag{font-size:11px;letter-spacing:.04em;color:var(--sky);padding:5px 8px;border:1px solid #31576c;border-radius:7px}
nav{display:flex;overflow:auto;gap:9px;padding:16px 0;scrollbar-width:thin}
button{font:inherit;border:1px solid #345369;border-radius:11px;background:#1b3449;color:var(--text);cursor:pointer;padding:10px 14px;min-height:42px}
button:hover{background:#24455a}button:focus-visible,input:focus-visible,select:focus-visible{outline:2px solid var(--sky);outline-offset:2px}
button:disabled{opacity:.4;cursor:not-allowed}.active,button.primary{background:#126c82;border-color:#2b94a6;color:white}
button.danger{background:#593741;border-color:#7a4553}button.ghost{background:#122536}nav button{white-space:nowrap;flex:1;min-width:96px}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:13px}
.two{display:grid;grid-template-columns:1.1fr 1fr;gap:13px}
.card{background:linear-gradient(150deg,var(--card),var(--card2));border:1px solid var(--border);border-radius:17px;padding:20px;min-width:0}
.temp{min-height:175px;border-top:3px solid var(--accent)}.temp-name{font-weight:600}.temp-value{font-size:clamp(32px,4vw,46px);font-weight:700;letter-spacing:-.02em;font-variant-numeric:tabular-nums;margin:10px 0}.unit{font-size:17px;color:var(--muted)}.temp-detail{font-size:12px;color:var(--muted)}
.row{display:flex;gap:12px;align-items:center;justify-content:space-between;padding:10px 0;border-bottom:1px solid var(--border)}
.row:last-child{border-bottom:0}.row span:last-child{text-align:right;overflow-wrap:anywhere}
.stack{display:flex;gap:9px;align-items:center;flex-wrap:wrap}.section{margin-top:24px}.hidden{display:none!important}
input,select{background:#0b1a2b;color:var(--text);border:1px solid #3b556d;border-radius:9px;font:inherit;padding:11px 12px;width:100%;min-height:42px}
input[type=checkbox]{width:19px;min-height:19px;accent-color:#3ca3b4}
label{display:block;color:#b9d0e4;font-size:13px;font-weight:600;margin-bottom:7px}
.field{margin:10px 0}.actions{display:flex;gap:8px;flex-wrap:wrap;margin:12px 0}
table{width:100%;border-collapse:collapse}th,td{padding:10px 7px;border-bottom:1px solid var(--border);text-align:left;font-size:13px;overflow-wrap:anywhere}
tr:last-child td{border-bottom:0}th{color:var(--muted);font-weight:500}
.footnote,.notice{border:1px solid #305165;background:#132d3e;color:#b8dff2;padding:13px 15px;border-radius:13px;margin:16px 0;font-size:13px}
.warn{border-color:#68563e;background:#34291e;color:#f0c797}.error{color:#ffafb0}.good{color:var(--green)}
.details{border:1px dashed #43617b;border-radius:14px;margin-top:14px;padding:16px}
.badge{font-size:12px;border:1px solid #395772;border-radius:50px;padding:5px 10px;color:var(--muted)}.badge.on{color:var(--green)}
.detail-chart{min-height:85px;width:100%;display:block;background:#0d1c2c;border-radius:9px;margin-top:9px}
.feed{max-height:310px;overflow:auto}.event{border-bottom:1px solid var(--border);padding:10px 0}.event:last-child{border:0}
.equip-list{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}
.equip .actions button{flex:1}.equip h3{font-size:16px}
.tab-row{display:flex;gap:8px;flex-wrap:wrap;margin:0 0 14px}.tab-row button{flex:1;min-width:105px}
#toast{position:fixed;bottom:14px;left:50%;transform:translateX(-50%);background:#23475a;border:1px solid #4b798c;border-radius:12px;padding:12px 20px;z-index:20;box-shadow:0 8px 32px #0008;max-width:92vw;text-align:center;pointer-events:none}
code{font-size:12px;color:#aadffd;overflow-wrap:anywhere}.tiny{font-size:12px}
footer{margin-top:29px;color:var(--muted);font-size:12px;display:flex;gap:10px;justify-content:space-between;flex-wrap:wrap}
@media(max-width:820px){.two{grid-template-columns:1fr}.equip-list{grid-template-columns:1fr 1fr}}
@media(max-width:620px){.container{padding:12px 10px 60px}.card{padding:15px;border-radius:13px}.equip-list{grid-template-columns:1fr}.grid{gap:9px}.temp{min-height:145px}.temp-value{font-size:34px}.toolbar{width:100%}.toolbar button{flex:1}nav{gap:5px}nav button{min-width:88px;padding:9px 8px;font-size:12px}}
</style></head>
<body><div class="container">
<header>
 <div class="brand"><div class="mark">°</div><div><h1>КОТЕЛЬНАЯ</h1><p>Boiler Panel <span id="version">v0.5.4</span> · ESP32-P4</p></div></div>
 <div class="toolbar"><span id="net" class="status">Подключение…</span><span id="authstate" class="tag">Только просмотр</span><button id="pairbutton" data-action="show-pair">Сопряжение</button><button id="logout" data-action="logout" class="ghost hidden">Выход</button></div>
</header>
<nav>
<button data-page="home" class="active">Главная</button>
<button data-page="equipment">Управление</button>
<button data-page="fuel">Топливо</button>
<button data-page="settings">Настройки</button>
<button data-page="service">Сервис</button>
</nav>
<div id="pairbox" class="card hidden">
 <h2>Сопряжение с дисплеем</h2>
 <p>На физической панели откройте <b>Настройки → Веб-доступ → Создать новый код</b>. Введите код ниже. Он действует 2 минуты, а доступ — 1 час. Без сопряжения доступны показания.</p>
 <div class="stack"><input style="max-width:180px" id="paircode" inputmode="numeric" maxlength="6" placeholder="Код с дисплея" autocomplete="off"><button data-action="pair" class="primary">Подключить управление</button><button data-action="hide-pair" class="ghost">Закрыть</button></div>
 <p class="tiny">Веб-соединение по HTTP не зашифровано. Используйте только в доверенной локальной сети.</p>
</div>
<section id="home" class="page">
 <div class="stack" style="justify-content:space-between"><h2>Температуры</h2><span id="clock" class="muted">Время не установлено</span></div>
 <div class="grid">
 <button class="card temp" style="--accent:#65ddb1;text-align:left" data-temp="0"><div class="temp-name">В котельной</div><div class="temp-value" id="t0">--.- °C</div><div class="temp-detail" id="tstate0">Нет данных</div></button>
 <button class="card temp" style="--accent:#71bff8;text-align:left" data-temp="1"><div class="temp-name">На улице</div><div class="temp-value" id="t1">--.- °C</div><div class="temp-detail" id="tstate1">Нет данных</div></button>
 <button class="card temp" style="--accent:#f3bf81;text-align:left" data-temp="2"><div class="temp-name">Теплоноситель</div><div class="temp-value" id="t2">--.- °C</div><div class="temp-detail" id="tstate2">Нет данных</div></button>
 <button class="card temp" style="--accent:#b7a4ed;text-align:left" data-temp="3"><div class="temp-name">Топливо</div><div class="temp-value" id="t3">--.- °C</div><div class="temp-detail" id="tstate3">Нет данных</div><div class="temp-detail" id="fuel-summary">Уровень: нет данных</div></button>
 </div>
 <div id="temperature-detail" class="card section hidden"><div class="stack" style="justify-content:space-between"><h2 id="detail-title">Температура</h2><button data-action="close-detail">Закрыть</button></div>
 <div id="detail-meta" class="muted">История измерений недоступна.</div><svg id="detail-chart" class="detail-chart" viewBox="0 0 400 100" preserveAspectRatio="none" aria-label="График измерений"></svg><div id="detail-stats" class="muted tiny"></div></div>
 <div class="card section"><h3>Общая информация</h3><div class="row"><span>Уровень топлива</span><b id="home-fuel">Нет данных</b></div><div class="row"><span>Wi-Fi</span><span id="home-wifi">Неизвестно</span></div><div class="row"><span>Источники измерений</span><span>Не назначены (показываются только реальные данные)</span></div></div>
</section>
<section id="equipment" class="page hidden">
 <h2>Оборудование котельной</h2><div class="notice warn">Контроллеры оборудования ещё не подключены. Доступно отображение состава и статуса подключения. Команды включения и выключения отсутствуют.</div>
 <div class="equip-list section" id="equipment-list"></div>
</section>
<section id="fuel" class="page hidden">
 <h2>Топливо</h2>
 <div class="two"><div class="card"><h3>Уровень по двум герконам</h3>
 <div class="row"><span>Общий уровень</span><strong id="fuel-status">Нет данных</strong></div>
 <div class="row"><span>Нижний датчик</span><span id="lower-status">Нет данных</span></div>
 <div class="row"><span>Верхний датчик</span><span id="upper-status">Нет данных</span></div>
 <p class="tiny">Проценты и литры не рассчитываются: герконы дают только два порога.</p></div>
 <div class="card"><h3>Подогрев топлива — до 80 °C</h3><div class="row"><span>Температура</span><b id="fuel-temp">--.- °C</b></div><div class="row"><span>Контроллер подогрева</span><strong id="fuel-controller">Не подключён</strong></div>
 <p class="tiny">80 °C — проектное требование, не подтверждённая уставка исполнительного регулятора.</p>
 <button class="ghost" data-temp="3">История температуры</button></div></div>
</section>
<section id="settings" class="page hidden">
 <h2>Настройки</h2>
 <div class="tab-row" id="tabs">
 <button data-tab="wifi" class="active">Wi-Fi</button><button data-tab="ble">Bluetooth</button><button data-tab="clock">Часы</button><button data-tab="screen">Экран</button><button data-tab="web">Веб-доступ</button>
 </div>
 <div id="tab-wifi" class="tab card"><h3>Wi-Fi · 2.4 ГГц</h3>
 <div class="row"><span>Соединение</span><strong id="wifi-connected">Не подключено</strong></div>
 <div class="row"><span>SSID</span><span id="wifi-ssid">Доступно после сопряжения</span></div>
 <div class="row"><span>IP</span><span id="wifi-ip">—</span></div>
 <div class="row"><span>RSSI</span><span id="wifi-rssi">—</span></div>
 <p class="hint" id="wifi-message"></p>
 <div class="actions"><button data-action="wifi_scan" data-write="1">Найти сети</button><button data-action="wifi_disconnect" data-write="1">Отключить</button><button class="danger" data-action="wifi_forget" data-write="1">Забыть сеть</button></div>
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
 <div id="tab-web" class="tab card hidden"><h3>Веб-доступ</h3><div class="row"><span>Режим</span><span id="web-auth">Только просмотр</span></div>
 <p>Для управления создайте одноразовый код на физическом дисплее: <b>Настройки → Веб-доступ</b>, затем нажмите «Сопряжение» вверху браузера. Сессия — 1 час.</p>
 <button data-action="show-pair">Ввести код с дисплея</button>
 <div class="notice warn">Обычный HTTP в локальной сети не шифруется. Не включайте перенаправление порта 80 на маршрутизаторе.</div></div>
</section>
<section id="service" class="page hidden">
<h2>Сервис</h2><div class="two">
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
</div></div>
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
 <p class="tiny muted">Каталог: <code>/boiler-panel/logs/</code>. Только данные этой панели. Автоматическое форматирование, просмотр личных файлов карты и сетевой доступ к ним отсутствуют.</p>
</div>
<div class="actions"><button data-action="refresh" class="primary">Обновить сведения</button></div>
<div class="notice">Эта страница не записывает новую прошивку. OTA, управление реальными выходами и аппаратные датчики пока не реализованы.</div>
</section>
<div id="toast" class="hidden" role="status"></div>
<footer><span>Локальная панель мониторинга · без подключённых контроллеров</span><span id="last-update">Ожидание ответа</span><span>Автообновление каждые 3 секунды</span></footer>
</div>
<script>
(()=>{
"use strict";
const $=id=>document.getElementById(id);
const names=["Бойлер косвенного нагрева","Электрокотёл","Контур 1","Контур 2","Контур 3","Дизельный отопитель","Подогрев топлива"];
const tnames=["В котельной","На улице","Теплоноситель","Топливо"];
let token=sessionStorage.getItem("boiler_panel_token")||"",current=null,page="home",tab="wifi",detail=-1;
let lastServerError="",lastToast=null,refreshInFlight=false;
const timeout=s=>s===0?"Не выключать":s<60?s+" секунд":s/60+" минут";
const storageBytes=n=>Number.isFinite(n)&&n>0
    ?(n>=1073741824?(n/1073741824).toFixed(2)+" ГБ":
      n>=1048576?(n/1048576).toFixed(1)+" МБ":
      (n/1024).toFixed(0)+" КБ"):"—";
function text(id,v){const n=$(id);if(n)n.textContent=v==null?"—":String(v)}
function show(id,on){const n=$(id);if(n)n.classList.toggle("hidden",!on)}
function notice(message,problem=false){
 const n=$("toast");n.textContent=message;n.style.color=problem?"#ffacaf":"#d6f9ff";show("toast",true);
 if(lastToast)clearTimeout(lastToast);lastToast=setTimeout(()=>show("toast",false),4700);
}
function numeric(n){return Number.isFinite(Number(n))?Number(n):0}
function apiHeaders(){const h={};if(token)h.Authorization="Bearer "+token;return h}
function drawPage(){
 for(const n of document.querySelectorAll(".page"))n.classList.toggle("hidden",n.id!==page);
 for(const n of document.querySelectorAll("[data-page]"))n.classList.toggle("active",n.dataset.page===page);
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
 const d=current||{},items=d.equipment||[],holder=$("equipment-list");
 holder.replaceChildren();
 for(let i=0;i<names.length;i++){
   const item=items[i]||{};
   const card=document.createElement("div");card.className="card equip";
   const heading=document.createElement("h3");heading.textContent=item.name||names[i];
   const status=document.createElement("p");status.className="error";
   status.textContent=item.connected?"Подключено":"Контроллер не подключён";
   const explanation=document.createElement("p");explanation.className="tiny muted";
   explanation.textContent="Фактическое состояние неизвестно. Команды недоступны до назначения и проверки контроллера.";
   card.append(heading,status,explanation);
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
function updateView(){
 const d=current;if(!d)return;
 text("version",d.version);text("service-version",d.version);
 text("clock",d.clock||"Нет времени");text("clock-current",d.clock||"Нет времени");
 text("net",d.wifi&&d.wifi.connected?"● Wi-Fi подключён":"● Wi-Fi не подключён");
 $("net").className="status"+(d.wifi&&d.wifi.connected?" ok":"");
 text("authstate",d.authorized?"Управление разрешено":"Только просмотр");
 text("web-auth",d.authorized?"Сопряжено (1 час)":"Только просмотр");
 show("pairbutton",!d.authorized);show("logout",d.authorized);
 text("last-update","Обновлено: "+new Date().toLocaleTimeString("ru-RU"));
 text("home-fuel",d.fuel_level_text||"Нет данных");
 text("fuel-summary","Уровень: "+(d.fuel_level_text||"Нет данных"));
 text("home-wifi",d.wifi&&d.wifi.connected?"Подключено":"Не подключено");
 for(let i=0;i<4;i++){
   const t=d.temperatures&&d.temperatures[i]||{};
   const value=t.valid&&typeof t.value==="number"?t.value.toFixed(1)+" °C":"--.- °C";
   text("t"+i,value);text("tstate"+i,t.valid?"Измерено "+t.age_s+" с назад":"Нет данных");
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
 if(document.activeElement!==$("zone-select"))$("zone-select").value=String(tm.zone||0);
 if(document.activeElement!==$("screen-select"))$("screen-select").value=String(screen.timeout_s||0);
 for(const b of document.querySelectorAll('[data-write="1"]')){
   const wifiAction=b.dataset.action&&b.dataset.action.startsWith("wifi_");
   const bleAction=b.dataset.action==="ble_scan";
   const needsTouch=b.dataset.action==="screen"&&numeric($("screen-select").value)>0;
   b.disabled=!d.authorized || (wifi.busy&&(wifiAction||bleAction||b.dataset.action==="zone"||b.dataset.action==="clock"||b.dataset.action==="screen"))
     || (needsTouch&&!screen.touch_ready) || (!wifi.supported&&wifiAction) || (!ble.supported&&bleAction);
 }
 renderAP();renderBLE();renderEquipment();
}
async function sendPost(path,data,secret){
 const headers={"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"};
 if(secret&&token)headers.Authorization="Bearer "+token;
 const r=await fetch(path,{method:"POST",headers,body:new URLSearchParams(data),cache:"no-store"});
 let obj;
 try{obj=await r.json()}catch(_){obj={error:"invalid_response"}}
 if(r.status===401){token="";sessionStorage.removeItem("boiler_panel_token");}
 if(!r.ok)throw Error(obj.error||("HTTP "+r.status));
 return obj;
}
async function command(action,options={}){
 if(!token){show("pairbox",true);notice("Сначала введите код с физической панели",true);return}
 try{
   await sendPost("/api/command",Object.assign({action},options),true);
   notice("Запрос принят. Результат появится в состоянии панели.");
   setTimeout(poll,550);
 }catch(e){
   notice("Ошибка действия: "+e.message,true);
 }
}
async function login(){
 const code=$("paircode").value.trim();
 if(!/^[0-9]{6}$/.test(code)){notice("Введите шестизначный код",true);return}
 try{
   const r=await sendPost("/api/pair",{code},false);
   token=r.token;sessionStorage.setItem("boiler_panel_token",token);
   $("paircode").value="";show("pairbox",false);
   notice("Управление веб-панелью разрешено на 1 час");
   await poll();
 }catch(e){notice("Сопряжение не выполнено: "+e.message,true)}
}
async function logout(){
 try{await sendPost("/api/logout",{},true)}catch(_){}
 token="";sessionStorage.removeItem("boiler_panel_token");
 notice("Веб-управление отключено");
 await poll();
}
function handleAction(b){
 const action=b.dataset.action;
 if(action==="show-pair"){show("pairbox",true);$("paircode").focus();return}
 if(action==="hide-pair"){show("pairbox",false);return}
 if(action==="pair"){login();return}
 if(action==="logout"){logout();return}
 if(action==="close-detail"){detail=-1;updateDetail();return}
 if(action==="refresh"){poll();return}
 if(action==="wifi_forget"&&!confirm("Удалить сохранённую сеть и отключить автоподключение на панели?"))return;
 if(action==="wifi_disconnect"&&!confirm("Отключить Wi-Fi? Веб-интерфейс может стать недоступным."))return;
 if(action==="zone"){command("zone",{value:$("zone-select").value});return}
 if(action==="clock"){command("clock",{value:$("manual-clock").value.trim()});return}
 if(action==="screen"){command("screen",{value:$("screen-select").value});return}
 command(action);
}
document.addEventListener("click",e=>{
 const target=e.target;
 if(!(target instanceof Element))return;
 const p=target.closest("[data-page]");
 if(p){page=p.dataset.page;detail=-1;drawPage();return}
 const t=target.closest("[data-tab]");
 if(t){setTab(t.dataset.tab);return}
 const temperature=target.closest("[data-temp]");
 if(temperature){detail=Number(temperature.dataset.temp);page="home";drawPage();$("temperature-detail").scrollIntoView({behavior:"smooth",block:"nearest"});return}
 const button=target.closest("[data-action]");
 if(button){if(button.disabled)return;handleAction(button)}
});
$("wifi-open").addEventListener("change",e=>{
 const isOpen=e.target.checked;$("wifi-pass").disabled=isOpen;
 if(isOpen)$("wifi-pass").value="";
});
$("wifi-form").addEventListener("submit",e=>{
 e.preventDefault();
 if(!token){show("pairbox",true);notice("Сначала сопряжение",true);return}
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
   const r=await fetch("/api/status",{cache:"no-store",headers:apiHeaders()});
   if(!r.ok)throw Error("HTTP "+r.status);
   const d=await r.json();
   if(!d.authorized&&token){token="";sessionStorage.removeItem("boiler_panel_token");notice("Сессия истекла. Повторите сопряжение.");}
   current=d;lastServerError="";updateView();
 }catch(e){
   if(lastServerError!==e.message)notice("Нет связи с панелью: "+e.message,true);
   lastServerError=e.message;
   text("net","● Нет ответа панели");
   $("net").className="status";
   text("last-update","Сервер не отвечает, повторяем");
 }finally{refreshInFlight=false}
}
drawPage();poll();setInterval(poll,3000);
})();
</script></body></html>)BPHTML";
