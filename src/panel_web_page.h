#pragma once
// Served from flash, without CDN, libraries, cookies or external requests.
static const char kBoilerWebPage[] PROGMEM = R"BPHTML(<!doctype html>
<html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#09121f"><title>Котельная — панель</title>
<style>
:root{color-scheme:dark;--bg:#09121f;--card:#15263a;--card2:#101e30;--line:#2b4056;--text:#f1f6fc;--muted:#92abc4;--blue:#69cefc;--ok:#62dcaf;--warn:#eab878}
*{box-sizing:border-box}html{font:16px system-ui,-apple-system,Segoe UI,sans-serif;background:var(--bg);color:var(--text)}body{margin:0;min-width:300px;background:radial-gradient(ellipse at 85% -10%,#14334e 0%,transparent 42%),var(--bg);min-height:100vh}
main{max-width:1140px;margin:auto;padding:24px clamp(12px,3vw,36px) 70px}header{display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:15px;padding:14px 0 28px;border-bottom:1px solid var(--line)}
.brand{display:flex;align-items:center;gap:15px}.mark{display:grid;place-items:center;width:48px;height:48px;border-radius:14px;background:#193f4d;color:var(--blue);font-weight:800;font-size:24px}h1{font-size:clamp(23px,3.5vw,34px);margin:0;font-weight:700;letter-spacing:.02em}p{margin:4px 0 0;color:var(--muted)}.tag{border:1px solid #315b6b;border-radius:100px;padding:8px 13px;color:var(--blue);font-size:13px;display:flex;align-items:center;gap:8px}
.dot{height:9px;width:9px;border-radius:50%;background:var(--warn);display:inline-block}.dot.ok{background:var(--ok);box-shadow:0 0 10px #62dcaf88}
.toprow{display:flex;justify-content:space-between;align-items:center;gap:10px;margin:28px 0 16px;flex-wrap:wrap}h2{font-size:19px;margin:0;font-weight:650}small{color:var(--muted)}.stamp{font-variant-numeric:tabular-nums;color:var(--muted)}
.temperatures{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}
.tile{position:relative;overflow:hidden;background:linear-gradient(142deg,var(--card),var(--card2));border:1px solid var(--line);border-radius:19px;padding:23px;min-height:182px}.tile:before{content:"";height:4px;width:42px;display:block;border-radius:4px;background:var(--accent);margin-bottom:20px}.tile .label{font-size:17px;font-weight:600}.tile .value{font-size:clamp(35px,5vw,54px);line-height:1.3;font-weight:650;letter-spacing:-.03em;margin:11px 0 9px;font-variant-numeric:tabular-nums}.unit{font-size:.48em;color:var(--muted);font-weight:500;letter-spacing:0}.tile .state{font-size:13px;color:var(--muted)}.tile .ghost{position:absolute;right:18px;top:23px;font-size:23px;color:var(--accent);opacity:.75}
.panel{border:1px solid var(--line);background:#132337;border-radius:18px;padding:19px 22px}.row{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:13px 0;border-bottom:1px solid #294056}.row:last-child{border-bottom:0;padding-bottom:0}.row:first-child{padding-top:0}.val{font-variant-numeric:tabular-nums;text-align:right;word-break:break-word}.muted{color:var(--muted)}.mini{font-size:12px}.good{color:var(--ok)}.columns{display:grid;grid-template-columns:minmax(0,1.2fr) minmax(0,1fr);gap:15px}.equip{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:9px}
.eq{background:#101e30;border:1px solid #2b4056;padding:14px;border-radius:13px}.eqname{font-weight:600;margin-bottom:8px;font-size:14px}.eqstate{font-size:12px;color:var(--muted)}.notice{background:#172d3f;border:1px solid #2c5970;border-radius:14px;padding:13px 17px;color:#b3dcf2;margin-top:17px;font-size:13px;line-height:1.55}
footer{padding-top:24px;font-size:12px;color:var(--muted);display:flex;justify-content:space-between;flex-wrap:wrap;gap:10px}
@media(max-width:750px){main{padding-top:12px}.columns{grid-template-columns:1fr}.tile{min-height:158px;padding:16px}.tile:before{margin-bottom:13px}.tile .label{font-size:14px}.tile .value{font-size:34px}.tile .ghost{right:14px;top:16px}.equip{grid-template-columns:1fr 1fr}}
@media(max-width:420px){.temperatures,.equip{grid-template-columns:1fr 1fr;gap:9px}.tile{min-height:145px;padding:13px;border-radius:14px}.tile .value{font-size:31px}.tile .label{font-size:13px}.tile .ghost{font-size:17px}.panel{padding:16px}}
</style></head><body><main>
<header><div class="brand"><div class="mark">°</div><div><h1>КОТЕЛЬНАЯ</h1><p>JC4880P443 · панель мониторинга</p></div></div>
<div class="tag"><span id="onlineDot" class="dot"></span><span id="onlineText">Подключение к панели…</span></div></header>
<div class="toprow"><h2>Температуры</h2><div class="stamp" id="clock">Время не установлено</div></div>
<section class="temperatures">
<article class="tile" style="--accent:#65ddb1"><span class="ghost">⌂</span><div class="label">В котельной</div><div class="value" id="t0">--.-<span class="unit"> °C</span></div><div class="state" id="s0">Нет данных</div></article>
<article class="tile" style="--accent:#71bff8"><span class="ghost">❄</span><div class="label">На улице</div><div class="value" id="t1">--.-<span class="unit"> °C</span></div><div class="state" id="s1">Нет данных</div></article>
<article class="tile" style="--accent:#f3bf81"><span class="ghost">◌</span><div class="label">Теплоноситель</div><div class="value" id="t2">--.-<span class="unit"> °C</span></div><div class="state" id="s2">Нет данных</div></article>
<article class="tile" style="--accent:#b7a4ed"><span class="ghost">♧</span><div class="label">Топливо</div><div class="value" id="t3">--.-<span class="unit"> °C</span></div><div class="state" id="s3">Нет данных</div></article>
</section>
<div class="toprow"><h2>Оборудование</h2><small>Локальная демонстрация · не управляет реле</small></div>
<section class="columns">
<div class="panel"><div class="equip" id="equipment">
<div class="eq"><div class="eqname">Бойлер косвенного нагрева</div><div class="eqstate">Нет данных о фактической работе</div></div>
<div class="eq"><div class="eqname">Электрокотёл</div><div class="eqstate">Нет данных о фактической работе</div></div>
<div class="eq"><div class="eqname">Контуры отопления 1–3</div><div class="eqstate">Нет данных о фактической работе</div></div>
<div class="eq"><div class="eqname">Дизельный отопитель</div><div class="eqstate">Нет данных о фактической работе</div></div>
<div class="eq"><div class="eqname">Подогрев топлива</div><div class="eqstate">Нет данных о фактической работе</div></div></div></div>
<div class="panel"><div class="row"><span class="muted">Уровень топлива</span><strong class="val" id="fuel">Нет данных</strong></div>
<div class="row"><span class="muted">Wi‑Fi</span><strong class="val" id="wifi">Неизвестно</strong></div>
<div class="row"><span class="muted">Адрес панели</span><span class="val" id="ip">—</span></div>
<div class="row"><span class="muted">Уровень сигнала</span><span class="val" id="rssi">—</span></div>
<div class="row"><span class="muted">Таймер экрана</span><span class="val" id="screen">—</span></div>
<div class="row"><span class="muted">Версии ESP-Hosted / C6</span><span class="val" id="radio">—</span></div>
<div class="row"><span class="muted">Время работы</span><span class="val" id="uptime">—</span></div></div>
</section>
<div class="notice"><strong>Режим наблюдения.</strong> Этот веб-интерфейс показывает состояние панели и локальной демонстрации. Реальные температурные датчики и исполнительные выходы пока не подключены. Веб-команды управления котлами и настройками намеренно отключены. Доступ по обычному HTTP только для доверенной локальной сети; не открывайте порт в интернет.</div>
<footer><span id="version">Boiler Panel</span><span id="updated">Ожидание данных…</span><span>Обновление каждые 5 секунд</span></footer>
</main><script>
(function(){
"use strict";
var names=["Бойлер косвенного нагрева","Электрокотёл","Контур 1","Контур 2","Контур 3","Дизельный отопитель","Подогрев топлива"];
function el(id){return document.getElementById(id)}
function put(id,text){el(id).textContent=text}
function mode(v){return v==="enabled"?"Разрешено (ДЕМО)":v==="off"?"Выключено (ДЕМО)":"Неизвестно"}
function request(v){return v==="pending"?" · ожидание ответа":v==="rejected"?" · отказ":v==="timeout"?" · нет ответа":""}
function timeout(s){return !s?"Не выключать":s<60?s+" сек":s/60+" мин"}
function paint(d){
  put("version","Boiler Panel "+(d.version||""));
  put("clock",d.clock||"Время не установлено");
  put("wifi",d.wifi&&d.wifi.connected?"Подключено":"Нет соединения");
  put("ip",d.wifi&&d.wifi.ip?d.wifi.ip:"—");
  put("rssi",d.wifi&&d.wifi.connected?d.wifi.rssi+" dBm":"—");
  put("radio",(d.host_version||"—")+" / "+(d.c6_version||"—"));
  put("screen",timeout(d.screen_timeout_s));
  put("uptime",Math.floor(d.uptime_s/60)+" мин");
  put("fuel",d.fuel_level_text||"Нет данных");
  var e=d.equipment||[];
  el("equipment").replaceChildren();
  for(var i=0;i<names.length;i++){
    var outer=document.createElement("div");outer.className="eq";
    var title=document.createElement("div");title.className="eqname";title.textContent=names[i];
    var status=document.createElement("div");status.className="eqstate";
    status.textContent=mode(e[i]&&e[i].mode)+request(e[i]&&e[i].request)+". Фактическая работа: нет данных";
    outer.append(title,status);el("equipment").append(outer);
  }
  for(var j=0;j<4;j++){
    var t=d.temperatures&&d.temperatures[j],valid=!!(t&&t.valid&&typeof t.value==="number");
    el("t"+j).textContent=valid?t.value.toFixed(1):"--.-";
    var u=document.createElement("span");u.className="unit";u.textContent=" °C";el("t"+j).append(u);
    put("s"+j,valid?"Обновлено "+t.age_s+" с назад":"Нет данных от датчика");
  }
  el("onlineDot").className="dot ok";put("onlineText","Панель доступна");
  put("updated","Последнее обновление: "+new Date().toLocaleTimeString("ru-RU"));
}
function poll(){
  fetch("/api/status",{cache:"no-store"}).then(function(r){if(!r.ok)throw Error("HTTP "+r.status);return r.json()})
  .then(paint).catch(function(){el("onlineDot").className="dot";put("onlineText","Нет ответа от панели");put("updated","Связь потеряна — повторяем запрос");});
}
poll();setInterval(poll,5000);
})();
</script></body></html>)BPHTML";
