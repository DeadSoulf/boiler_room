#pragma once
// Self-contained on-device UI. No CDN, tracking, remote assets or credentials in HTML.
static const char kBoilerWebPage[] PROGMEM = R"BPHTML(<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="theme-color" content="#101719"><title>Котельная</title><style>
:root{color-scheme:dark;--bg:#101719;--card:#1b2528;--line:#304044;--ink:#eff5f3;--muted:#a1b3b5;--accent:#76d6c3;--warn:#e7bb78;--red:#ef9f9a;font-family:system-ui,-apple-system,Segoe UI,Arial,sans-serif;font-size:16px}
*{box-sizing:border-box}html{background:var(--bg);color:var(--ink)}body{margin:0;min-width:320px}button,input,select{font:inherit}button{color:var(--ink);background:#26373b;border:1px solid transparent;border-radius:12px;padding:12px 18px;min-height:46px;cursor:pointer;transition:background .15s}button:hover{background:#33494e}button:focus-visible,input:focus-visible,select:focus-visible,summary:focus-visible{outline:2px solid var(--accent);outline-offset:3px}button:disabled{opacity:.45;cursor:not-allowed}.primary{background:#245d55;border-color:#3b8b7c}.primary:hover{background:#30786b}.danger{background:#593738;border-color:#825453}.ghost{background:transparent;border-color:var(--line)}.hidden{display:none!important}
.container{max-width:1180px;margin:0 auto;padding:32px 28px 26px}header{display:flex;align-items:center;justify-content:space-between;gap:24px;margin-bottom:32px}h1{margin:0;font-size:32px;font-weight:600;letter-spacing:-.025em}h2{margin:0 0 24px;font-size:24px;font-weight:550}h3{font-size:18px;font-weight:550;margin:0 0 18px}p{color:var(--muted);line-height:1.55;margin:12px 0}.eyebrow{font-size:12px;letter-spacing:.12em;text-transform:uppercase;color:var(--muted);margin:0 0 8px}.header-meta{display:flex;align-items:center;gap:14px}.header-time{font-size:15px;color:var(--muted);font-variant-numeric:tabular-nums}.status,.badge,.tag{font-size:12px;color:var(--muted);padding:6px 0}.status.ok,.good{color:var(--accent)}.tag{border-left:1px solid var(--line);padding-left:14px}.toolbar{display:flex;align-items:center;gap:12px;flex-wrap:wrap}.topnav{display:flex;gap:8px;padding-bottom:22px;margin-bottom:30px;border-bottom:1px solid var(--line)}.topnav button{background:transparent;color:var(--muted);padding:12px 20px}.topnav button.active{color:var(--ink);background:#26373b}.topnav button:hover{color:var(--ink)}.page{animation:appear .16s ease-out}@keyframes appear{from{opacity:.4;transform:translateY(3px)}to{opacity:1;transform:none}}
.card{background:var(--card);border:1px solid var(--line);border-radius:20px;padding:26px;min-width:0}.grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:16px}.two{display:grid;grid-template-columns:1fr 1fr;gap:20px}.temp{display:flex;flex-direction:column;align-items:flex-start;text-align:left;min-height:290px;padding:24px;border:1px solid var(--line);background:var(--card)}button.temp:hover{background:#233136;border-color:#52766d}.temp-name{color:var(--muted);font-size:15px;font-weight:400}.temp-value{font-size:clamp(30px,3.1vw,43px);font-weight:500;letter-spacing:-.04em;white-space:normal;max-width:100%;line-height:1.2;margin:auto 0 12px;font-variant-numeric:tabular-nums}.temp-detail{color:var(--muted);font-size:12px;min-height:18px}.temp-fuel .temp-detail{margin-top:auto}.canister{height:152px;width:130px;align-self:center;color:var(--muted);margin:6px 0}.canister[data-state=full]{color:var(--accent)}.canister[data-state=half]{color:var(--warn)}.canister[data-state=empty],.canister[data-state=invalid]{color:var(--red)}.can-fill{fill:currentColor}.can-outline{stroke:currentColor;fill:none;stroke-width:3}.can-symbol{fill:currentColor;font-size:42px;font-weight:550}.can-cross{stroke:var(--bg);stroke-width:3;fill:none}.section{margin-top:24px}.row{display:flex;align-items:center;justify-content:space-between;gap:20px;padding:14px 0;border-bottom:1px solid var(--line);font-size:14px}.row:last-child{border-bottom:0}.row>span:first-child{color:var(--muted)}.row>:last-child{text-align:right;overflow-wrap:anywhere}.stack,.actions{display:flex;align-items:center;gap:12px;flex-wrap:wrap}.actions{margin:20px 0 8px}.actions button{max-width:100%}.field{margin:20px 0}label{display:block;color:var(--muted);font-size:13px;margin-bottom:8px}input,select{display:block;background:#101b1e;border:1px solid #40575c;border-radius:10px;color:var(--ink);width:100%;max-width:100%;min-height:46px;padding:12px}input[type=checkbox]{width:20px;min-height:20px;accent-color:var(--accent);flex-shrink:0}.stack select{flex:1;min-width:160px}.tiny,small{font-size:12px;line-height:1.55}.muted,.hint{color:var(--muted)}.hint{font-size:13px}.hint:empty{display:none}.notice,.footnote{font-size:13px;line-height:1.6;color:var(--muted);padding:16px 18px;background:#182528;border:1px solid var(--line);border-radius:12px;margin:20px 0}.warn{color:#eacfa8;border-color:#66543d;background:#29251f}.error{color:var(--red)}.details{border:1px solid var(--line);border-radius:14px;padding:16px;margin-top:20px}summary{cursor:pointer;color:var(--ink);padding:6px 0;line-height:1.5}details>summary{min-height:40px}.danger-zone{border-color:#73484a}details[open]>summary{margin-bottom:14px}.menu-grid{display:grid;grid-template-columns:1fr 1fr;gap:14px}.menu-item{display:grid;grid-template-columns:1fr auto;gap:9px;text-align:left;padding:24px;background:var(--card);border:1px solid var(--line);border-radius:17px}.menu-item strong{font-size:18px;font-weight:500}.menu-item span{font-size:13px;color:var(--muted);grid-row:2}.menu-item b{grid-column:2;grid-row:1/3;align-self:center;color:var(--muted);font-weight:400}.subhead{display:flex;align-items:center;gap:18px;margin:0 0 24px}.subhead h2{margin:0}.subhead button{background:transparent;border:1px solid var(--line)}.tab-row{display:none}.feed{max-height:320px;overflow:auto}.feed>button{min-height:56px}.equip-list{display:grid;grid-template-columns:1fr 1fr;gap:16px}.equip{padding:24px}.equip h3{margin-bottom:10px}.equip p{font-size:13px;margin:0}.equip .error{color:var(--muted)}.equip .tiny{display:none}.detail-chart{display:block;width:100%;height:140px;background:#122025;border-radius:12px;margin:20px 0}.fuel-summary-value{font-size:30px;font-weight:500}.service-grid{margin-top:20px}code{overflow-wrap:anywhere;font-size:12px;color:#b9ded7}footer{display:flex;justify-content:space-between;gap:16px;flex-wrap:wrap;border-top:1px solid var(--line);margin-top:38px;padding-top:20px;font-size:12px;color:var(--muted)}
dialog{color:var(--ink);background:var(--card);border:1px solid #49625f;border-radius:22px;padding:30px;width:min(460px,calc(100% - 32px));max-height:90vh;overflow:auto}dialog::backdrop{background:#050a0dd9;backdrop-filter:blur(4px)}dialog h2{margin-bottom:12px}.pair-dialog input{text-align:center;font-size:30px;letter-spacing:.28em;font-variant-numeric:tabular-nums}.pair-dialog .actions button{flex:1}.pair-dialog .close-modal{float:right;min-height:40px;padding:8px 12px;background:transparent}.pair-symbol{height:44px;width:44px;border:1px solid #3a7167;border-radius:12px;display:grid;place-items:center;margin-bottom:22px;color:var(--accent)}#pair-error{min-height:20px;font-size:13px}.pair-dialog .tiny{margin-bottom:0}#toast{position:fixed;bottom:22px;left:50%;transform:translateX(-50%);padding:14px 20px;border:1px solid #42625a;background:#203c35;color:var(--ink);border-radius:12px;max-width:calc(100% - 32px);z-index:100;box-shadow:0 5px 25px #0006;text-align:center;font-size:14px;pointer-events:none}
@media(max-width:1000px){.grid{grid-template-columns:repeat(2,minmax(0,1fr))}.temp{min-height:255px}.temp-value{font-size:42px}.header-meta{display:none}}
@media(max-width:680px){.container{padding:22px 16px}header{align-items:flex-start;gap:16px;margin-bottom:24px}h1{font-size:27px}.toolbar{justify-content:flex-end;gap:8px}.toolbar .tag{display:none}.toolbar button{padding:10px 13px;min-height:44px}.topnav{gap:3px;justify-content:space-between;margin-bottom:24px;padding-bottom:16px}.topnav button{font-size:13px;padding:10px 9px;min-height:44px}.grid{gap:12px}.temp{padding:18px;min-height:245px}.temp-value{font-size:32px}.temp-name{font-size:13px}.canister{height:137px;width:112px}.two,.equip-list,.menu-grid{grid-template-columns:1fr}.card{padding:20px}.temp.card{padding:18px}.menu-item{padding:20px}.subhead{gap:10px;flex-wrap:wrap}.subhead h2{font-size:22px;min-width:0;overflow-wrap:anywhere}.row{gap:12px}.row>:last-child{max-width:60%}.actions button{flex:1 1 auto}footer{font-size:11px}.notice{padding:14px}dialog{padding:24px}.header-time{font-size:13px}}
@media(max-width:480px){header{flex-wrap:wrap;gap:12px}.toolbar{width:100%;justify-content:flex-start}.topnav{display:grid;grid-template-columns:repeat(2,minmax(0,1fr))}.topnav button{min-width:0}.temp-value{font-size:28px}}
@media(prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
/* Static location pictograms; not weather forecasts or heater status indicators. */
.temp .temp-value{margin:18px 0 4px}
.temp-icon{width:130px;height:122px;align-self:center;flex-shrink:0;margin:4px 0;color:var(--accent);pointer-events:none}
.temp-icon g{stroke:currentColor;stroke-width:3;stroke-linecap:round;stroke-linejoin:round;fill:var(--card)}
.temp-icon .icon-tint{fill:#245d55}.temp:not(.temp-fuel) .temp-detail{margin-top:auto}
@media(max-width:680px){.temp-icon{width:112px;height:112px}.temp .temp-value{margin:13px 0 4px}}
.temp-heading{display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:4px 8px;width:100%;margin-bottom:4px}
.temp-heading h2{margin:0}.temp-heading .temp-icon{width:34px;height:30px;margin:0}
.temp-reading{display:flex;flex-direction:column;align-items:flex-start;justify-content:center;width:100%;min-height:48px;padding:8px 0;text-align:left;background:transparent;border:0;border-radius:4px}
.temp-reading+.temp-reading{border-top:1px solid var(--line)}.temp-reading:hover,.temp-reading:focus-visible{background:#233136}
.temp .temp-reading .temp-value{margin:3px 0 2px}.temp .temp-reading .temp-detail{margin:0}
.fuel-open{display:grid;place-items:center;min-height:44px;min-width:44px;border:0;border-radius:6px;background:transparent;padding:0}.fuel-open:hover,.fuel-open:focus-visible{background:#233136}.fuel-open .canister{width:30px;height:38px;margin:0}
.update-offer{display:flex;align-items:center;justify-content:space-between;gap:16px;margin:-12px 0 24px;padding:10px 16px;border:1px solid var(--line);border-radius:12px;font-size:13px;color:var(--accent)}.update-offer button{padding:8px 14px;flex-shrink:0}.update-offer span{overflow-wrap:anywhere}.update-panel{max-width:760px}.update-panel progress{display:block;width:100%;height:12px;accent-color:var(--accent);margin:24px 0 10px}.update-panel .row b{font-variant-numeric:tabular-nums}#update-message{white-space:pre-wrap}#update-progress-text{font-variant-numeric:tabular-nums}#update-error:empty{display:none}
.pinout{max-width:800px;margin:24px auto}.pinout figcaption{font-size:18px;font-weight:550;margin-bottom:12px}.pinout a{display:block;color:var(--accent);text-decoration:none}.pinout a:focus-visible{outline:2px solid var(--accent);outline-offset:3px}.pinout img{display:block;width:100%;height:auto}.pinout-open{display:block;min-height:46px;padding:12px 0;font-size:14px;line-height:1.5;text-decoration:underline;text-underline-offset:3px}
.jp1-section{max-width:800px;margin:24px auto}.jp1-section h3{margin-bottom:8px}.jp1-legend{display:flex;flex-wrap:wrap;gap:8px 18px;font-size:12px}.jp1-legend span::before{content:"";display:inline-block;width:7px;height:7px;border-radius:50%;background:currentColor;margin-right:6px}.jp1-free{color:var(--accent)}.jp1-busy{color:var(--warn)}.jp1-map{display:grid;gap:6px;margin:16px 0}.jp1-row{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:28px}.jp1-pin{--pin-color:var(--muted);position:relative;display:grid;grid-template-columns:minmax(0,1fr) auto;align-content:center;gap:3px 4px;min-width:0;min-height:52px;padding:8px;border:1px solid var(--pin-color);border-radius:8px;background:#142024}.jp1-pin.right{grid-template-columns:auto minmax(0,1fr)}.jp1-name{font-size:14px;font-weight:550;overflow-wrap:anywhere}.jp1-number{font-size:11px;color:var(--muted);align-self:center}.jp1-status{grid-column:1/-1;color:var(--pin-color);font-size:12px;line-height:1.4;overflow-wrap:anywhere}.jp1-pin::after{content:"";position:absolute;right:-11px;top:calc(50% - 3px);width:6px;height:6px;border-radius:50%;background:var(--pin-color)}.jp1-pin.right::after{right:auto;left:-11px}.jp1-pin[data-state=free]{--pin-color:var(--accent)}.jp1-pin[data-state=busy]{--pin-color:var(--warn)}.jp1-pin[data-state=conflict]{--pin-color:var(--red)}.jp1-pin[data-kind=power],.jp1-pin[data-kind=boot]{--pin-color:#efb28c}.jp1-pin[data-kind=c6]{--pin-color:#9cc9ed}.jp1-pin[data-kind=i2c]{--pin-color:#c2b1e9}
button.jp1-pin{text-align:left;width:100%;transition:background .15s}button.jp1-pin:hover{background:#213437}button.jp1-pin:focus-visible{outline-offset:3px}.pin-dialog{width:min(560px,calc(100% - 32px));overflow-wrap:anywhere}.pin-dialog-head{display:flex;align-items:flex-start;justify-content:space-between;gap:12px}.pin-dialog-head h2{font-size:24px;min-width:0}.pin-dialog-head button{padding:8px 12px;min-height:44px;flex-shrink:0}.pin-dialog .field{margin:18px 0}.pin-dialog .actions{margin-top:16px}.pin-dialog .actions button{flex:1 1 auto}.pin-dialog .notice{margin:16px 0}.pin-dialog-status{min-height:21px}.pin-dialog-status:empty{display:none}.pin-dialog .tiny{margin:8px 0}.pin-dialog [role=status]{white-space:pre-line}#pin-impact{white-space:pre-line}#pin-reload{width:100%;margin-top:12px}#pin-current{color:var(--ink)}
</style></head><body><div class="container">
<header><div><p class="eyebrow">Панель дома</p><h1>Котельная</h1></div><div class="header-meta"><span class="header-time" id="clock">--:--</span></div><div class="toolbar"><span id="authstate" class="tag">Просмотр</span><button id="pairbutton" class="primary" data-action="show-pair">Сопряжение</button><button id="logout" class="ghost hidden" data-action="logout">Завершить доступ</button></div></header>
<nav class="topnav" aria-label="Разделы"><button data-page="home" class="active">Главная</button><button data-page="equipment">Оборудование</button><button data-page="fuel">Топливо</button><button data-page="settings">Настройки</button></nav>
<div id="update-offer" class="update-offer hidden" role="status"><span id="update-offer-text"></span><button class="ghost" data-page="update">Открыть</button></div>
<dialog id="pairbox" class="pair-dialog" aria-labelledby="pair-title"><button class="close-modal" data-action="hide-pair" aria-label="Отменить сопряжение">×</button><div class="pair-symbol" aria-hidden="true">⌁</div><h2 id="pair-title">Сопряжение</h2><p id="pair-message">Запрашиваем код на дисплее…</p><form id="pair-form"><label for="paircode">Код с физической панели</label><input id="paircode" inputmode="numeric" autocomplete="one-time-code" pattern="[0-9]{6}" maxlength="6" placeholder="······" required><p id="pair-error" class="error" role="alert"></p><p id="pair-expiry" class="tiny"></p><div class="actions"><button type="button" class="ghost" data-action="pair-retry">Новый код</button><button type="submit" class="primary" id="pair-submit" disabled>Подключить</button></div></form><p class="tiny">Доступ на 1 час. HTTP не шифруется — только доверенная локальная сеть.</p></dialog>
<section id="home" class="page"><div class="grid">
<button class="card temp" data-temp="0"><span class="temp-name">В доме</span><span class="temp-value" id="t0">--.- °C</span><svg class="temp-icon" data-icon="house" viewBox="0 0 120 108" aria-hidden="true" focusable="false"><g><rect x="82" y="16" width="10" height="27" rx="2"/><rect x="24" y="44" width="72" height="56" rx="6"/><polyline points="10,46 60,8 110,46" fill="none"/><rect class="icon-tint" x="56" y="65" width="22" height="35" rx="3"/><rect class="icon-tint" x="35" y="61" width="12" height="14" rx="2"/></g></svg><span class="temp-detail" id="tstate0">Нет данных</span></button>
<button class="card temp" data-temp="1"><span class="temp-name">На улице</span><span class="temp-value" id="t1">--.- °C</span><svg class="temp-icon" data-icon="outdoor" viewBox="0 0 120 108" aria-hidden="true" focusable="false"><g><circle cx="38" cy="36" r="20"/><path d="M38 3V9M8 36H2M64 10L59 15M10 10L15 15M67 36H74M10 62L15 57" fill="none"/><path d="M34 94C24 94 18 87 18 76C18 66 25 60 34 60C37 43 48 34 65 34C78 34 86 43 88 54C103 52 112 63 112 74C112 87 106 94 96 94Z"/></g></svg><span class="temp-detail" id="tstate1">Нет данных</span></button>
<section class="card temp temp-coolant" aria-labelledby="coolant-title">
 <div class="temp-heading"><h2 class="temp-name" id="coolant-title">Теплоноситель</h2><svg class="temp-icon" data-icon="radiator" viewBox="0 0 120 108" aria-hidden="true" focusable="false"><g><path d="M15 40V18H5M105 82H115V98M28 92V100M92 92V100" fill="none"/><rect x="14" y="32" width="92" height="58" rx="7"/><rect class="icon-tint" x="25" y="36" width="12" height="50" rx="4"/><rect class="icon-tint" x="44" y="36" width="12" height="50" rx="4"/><rect class="icon-tint" x="63" y="36" width="12" height="50" rx="4"/><rect class="icon-tint" x="82" y="36" width="12" height="50" rx="4"/></g></svg></div>
 <button type="button" class="temp-reading" data-temp="2" aria-controls="temperature-detail"><span class="temp-name">Подача</span><span class="temp-value" id="t2">--.- °C</span><span class="temp-detail" id="tstate2">Нет данных</span></button>
 <button type="button" class="temp-reading" data-temp="4" aria-controls="temperature-detail"><span class="temp-name">Обратка</span><span class="temp-value" id="t4">--.- °C</span><span class="temp-detail" id="tstate4">Нет данных</span></button>
</section>
<section class="card temp temp-fuel" aria-labelledby="fuel-title">
 <div class="temp-heading"><h2 class="temp-name" id="fuel-title">Топливо</h2><button type="button" id="fuel-open" class="fuel-open" data-page="fuel" aria-label="Уровень топлива: нет данных. Открыть раздел топлива"><svg id="fuel-canister" class="canister" data-state="unknown" viewBox="0 0 120 148" role="img" aria-label="Уровень топлива: нет данных"><title id="canister-title">Нет данных</title><rect id="canister-fill" class="can-fill" x="18" y="136" width="84" height="0" rx="5"/><g class="can-outline"><rect x="12" y="24" width="96" height="118" rx="10"/><path d="M62 24V4H99V24M21 18V10H41V18Z"/></g><path id="canister-cross" class="can-cross" d="M43 62L78 104M78 62L43 104" visibility="hidden"/><text id="canister-symbol" class="can-symbol" x="60" y="101" text-anchor="middle">?</text></svg></button></div>
 <button type="button" class="temp-reading" data-temp="3" aria-controls="temperature-detail"><span class="temp-name">В баке</span><span class="temp-value" id="t3">--.- °C</span><span class="temp-detail" id="tstate3">Нет данных</span></button>
 <button type="button" class="temp-reading" data-temp="5" aria-controls="temperature-detail"><span class="temp-name">На выходе</span><span class="temp-value" id="t5">--.- °C</span><span class="temp-detail" id="tstate5">Нет данных</span></button>
</section>
</div>
<div id="temperature-detail" class="card section hidden"><div class="stack" style="justify-content:space-between"><h2 id="detail-title">Температура</h2><button data-action="close-detail" class="ghost">Закрыть</button></div><p id="detail-meta"></p><svg id="detail-chart" class="detail-chart" viewBox="0 0 400 100" preserveAspectRatio="none" aria-label="История измерений"></svg><p id="detail-stats" class="tiny"></p></div></section>
<section id="equipment" class="page hidden"><h2>Оборудование</h2><div class="equip-list" id="equipment-list"></div><p class="tiny">Управление появится после подключения и проверки исполнительных контроллеров.</p></section>
<template id="threshold-controls"><div>
 <div class="row"><span data-part="temperature"></span><strong>Нет данных</strong></div>
 <button type="button" data-part="enable" disabled></button>
 <p class="hint" data-part="hardware"></p>
 <form data-part="form" novalidate>
 <div class="field"><label data-label="on">Включать при температуре ≤ (°C)</label><input data-part="on" inputmode="numeric" maxlength="2" placeholder="Не задано" disabled></div>
 <div class="field"><label data-label="off">Выключать при температуре ≥ (°C)</label><input data-part="off" inputmode="numeric" maxlength="2" placeholder="Не задано" disabled></div>
 <div class="actions"><button type="submit" data-part="save" class="primary" disabled>Сохранить пороги</button><button type="button" data-part="reload" class="ghost hidden" disabled>Перечитать с панели</button></div>
 <p class="hint" data-part="feedback" role="status"></p></form>
</div></template>
<section id="fuel" class="page hidden"><h2>Топливо</h2><div class="two"><div class="card"><h3>Уровень</h3><p id="fuel-status" class="fuel-summary-value">Нет данных</p><div class="row"><span>Нижний датчик</span><span id="lower-status">Нет данных</span></div><div class="row"><span>Верхний датчик</span><span id="upper-status">Нет данных</span></div><details><summary>Как определяется уровень</summary><p class="tiny">Два геркона дают три состояния, без расчёта литров. Для принятой схемы контактов используйте режим NC: оба разомкнуты — полная канистра; замкнут верхний — половина; замкнуты оба — пустая. Обрыв провода неотличим от размыкания контакта.</p></details></div><div class="card"><h3>Температура и подогрев</h3><div class="row"><span>В баке</span><b id="fuel-temp">--.- °C</b></div><div class="row"><span>На выходе</span><b id="fuel-outlet-temp">--.- °C</b></div><div class="row"><span>Контроллер подогрева</span><span id="fuel-controller">Не подключён</span></div><div class="actions"><button class="ghost" data-temp="3" aria-controls="temperature-detail">История в баке</button><button class="ghost" data-temp="5" aria-controls="temperature-detail">История на выходе</button></div><p class="tiny">До 80 °C — требование проекта, не действующая уставка. Команд нагрева нет.</p></div></div></section>
<section id="settings" class="page hidden">
 <h2>Настройки</h2><div id="settings-menu" class="menu-grid">
<button class="menu-item" data-tab="wifi"><strong>Wi-Fi</strong><span>Сеть и подключение</span><b>›</b></button>
<button class="menu-item" data-tab="screen"><strong>Экран</strong><span>Сон и пробуждение</span><b>›</b></button>
<button class="menu-item" data-tab="ble"><strong>Bluetooth</strong><span>Устройства поблизости</span><b>›</b></button>
<button class="menu-item" data-tab="clock"><strong>Часы</strong><span>Время и часовой пояс</span><b>›</b></button>
<button class="menu-item" data-page="ha"><strong>Home Assistant</strong><span>Подключение через MQTT</span><b>›</b></button>
<button class="menu-item" data-page="connections"><strong>Датчики</strong><span>Назначения и контакты JP1</span><b>›</b></button>
<button class="menu-item" data-page="service"><strong>Сервис</strong><span>Обновление, microSD, диагностика</span><b>›</b></button>
</div><div id="settings-back" class="subhead hidden"><button data-action="settings-root" aria-label="Назад к настройкам">‹ Настройки</button></div>
 <div id="tab-wifi" class="tab card"><h3>Wi-Fi · 2.4 ГГц</h3>
 <div class="row"><span>Соединение</span><strong id="wifi-connected">Не подключено</strong></div>
 <div class="row"><span>SSID</span><span id="wifi-ssid">Доступно после сопряжения</span></div>
 <div class="row"><span>IP</span><span id="wifi-ip">—</span></div>
 <div class="row"><span>RSSI</span><span id="wifi-rssi">—</span></div>
 <p class="hint" id="wifi-message"></p>
 <div class="actions"><button class="primary" data-action="wifi_scan" data-write="1">Найти сети</button></div><details class="details"><summary>Сохранённые сети и отключение</summary>
 <div class="field"><label for="wifi-profile">Сохранённая сеть</label><select id="wifi-profile"><option value="">Выберите сеть</option></select></div>
 <div class="actions"><button id="wifi-edit-password" type="button" data-write="1">Новый пароль</button><button class="danger" data-action="wifi_forget" data-write="1">Забыть выбранную сеть</button></div>
 <div class="actions"><button data-action="wifi_disconnect" data-write="1">Отключить Wi-Fi панели</button></div></details>
 <div class="field"><label>Найденные сети</label><div id="ap-list" class="feed"><p class="muted">Нажмите «Найти сети»</p></div></div>
 <p class="hint">Нажмите на сохранённую сеть для подключения без ввода пароля. Для новой или скрытой сети используйте форму ниже.</p>
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
  <section class="jp1-section" aria-labelledby="jp1-title">
    <h3 id="jp1-title">Схема контактов JP1</h3>
    <p class="tiny">Нажмите GPIO, чтобы назначить датчик, изменить подключение или освободить пин. Занятость показана по сохранённым назначениям. Ориентация — как на фото ниже.</p>
    <div class="jp1-legend" aria-label="Обозначения"><span class="jp1-free">Свободен</span><span class="jp1-busy">Занят</span><span class="muted">Служебный / питание</span></div>
    <div id="jp1-map" class="jp1-map" role="group" aria-label="Разъём JP1, 26 контактов в двух колонках"></div>
    <p id="jp1-state" class="tiny muted" role="status">Ожидаем назначения с панели.</p>
    <details id="jp1-photo" class="details"><summary>Фото распиновки</summary><figure class="pinout">
    <figcaption>Расположение контактов JP1</figcaption>
    <a id="jp1-pinout-link" href="/assets/jp1-pinout.jpg" target="_blank" rel="noopener" aria-label="Открыть схему контактов JP1 в полном размере, в новой вкладке">
      <img id="jp1-pinout" src="/assets/jp1-pinout.jpg" width="800" height="800" loading="lazy" decoding="async" alt="Схема JP1: расположение GPIO, питания, GND, I2C и выводов ESP32-C6">
      <span class="pinout-open">Открыть схему в полном размере ↗</span>
    </a>
    </figure></details>
  </section>
  <details id="cfg-advanced" class="details"><summary>Дополнительные настройки источников</summary>
  <p class="tiny">Для локального датчика выберите GPIO на схеме. Здесь также доступны описания удалённых источников Modbus/MQTT и возврат прошлой конфигурации.</p>
  <div class="field"><label for="cfg-channel">Канал / назначение</label>
  <select id="cfg-channel">
    <option value="0">Температура в доме</option>
    <option value="1">Температура улицы</option>
    <option value="2">Температура подачи</option><option value="6">Температура обратки</option>
    <option value="3">Топливо в баке</option><option value="7">Топливо на выходе</option>
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
  </details>
</div>
<div class="card section">
<h3>Сохранённые назначения</h3>
<div id="cfg-table" class="feed muted">Нет конфигурации.</div>
<p class="tiny muted">Поддерживаются реальные входные датчики на JP1: DS18B20 (один на GPIO) и герконы NO/NC. До подключения датчика статус «Не найден». Modbus/MQTT пока только описываются, без работы драйвера. Реле, котлы и нагреватели не управляются.</p>
</div>
</section>
<dialog id="pin-dialog" class="pin-dialog" aria-labelledby="pin-title" aria-describedby="pin-current pin-dialog-status">
 <div class="pin-dialog-head"><div><p class="eyebrow" id="pin-location"></p><h2 id="pin-title">Настройка пина</h2></div><button type="button" id="pin-close" class="ghost" aria-label="Закрыть настройку пина">×</button></div>
 <p id="pin-current"></p><p id="pin-base-revision" class="tiny muted"></p>
 <form id="pin-form">
  <div class="field"><label for="pin-channel">Назначить на</label><select id="pin-channel" required>
   <option value="">Выберите назначение</option><option value="0">Температура в доме</option><option value="1">Температура улицы</option><option value="2">Температура подачи</option><option value="6">Температура обратки</option><option value="3">Топливо в баке</option><option value="7">Топливо на выходе</option><option value="4">Нижний геркон топлива</option><option value="5">Верхний геркон топлива</option>
  </select></div>
  <p id="pin-temperature" class="tiny hidden">Датчик DS18B20 · отдельная линия OneWire.</p>
  <div id="pin-reed" class="hidden">
   <div class="field"><label for="pin-type">Режим геркона</label><select id="pin-type"><option value="">Выберите NO или NC</option><option value="2">NO — замкнут = достигнут</option><option value="3">NC — разомкнут = достигнут</option></select></div>
   <div class="field"><label for="pin-debounce">Подавление дребезга, мс</label><input id="pin-debounce" type="number" min="0" max="5000" step="1" value="50" inputmode="numeric"></div>
  </div>
  <p id="pin-wiring" class="tiny"></p><p id="pin-impact" class="notice hidden"></p>
  <p id="pin-dialog-status" class="pin-dialog-status hint" role="status" aria-live="polite"></p>
  <button type="button" id="pin-pair" class="ghost hidden" data-action="show-pair">Сопряжение</button>
  <button type="button" id="pin-reload" class="ghost hidden">Перечитать назначения</button>
  <p id="pin-reload-hint" class="tiny hidden">Перечитывание заменит несохранённый выбор текущими настройками панели.</p>
  <div class="actions"><button type="submit" id="pin-save" class="primary" disabled>Сохранить</button><button type="button" id="pin-clear" class="danger hidden" disabled>Освободить пин</button></div>
 </form>
</dialog>
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
  <p class="tiny">13 сущностей: пять температур, два геркона, уровень топлива, Wi-Fi, время работы, память и microSD. Без настоящего датчика или при устаревших данных сущность недоступна.</p>
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
<div class="subhead"><button data-action="settings-root">‹ Настройки</button><h2>Сервис</h2></div><button class="menu-item" data-page="update"><strong>Обновление</strong><span id="update-service-summary">Новые версии панели</span><b>›</b></button><details class="details"><summary>Диагностика ESP32-P4 / ESP32-C6</summary><div class="two service-grid">
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
<div class="notice">Выходы управления отоплением не подключены.</div>
</section>
<section id="update" class="page hidden">
 <div class="subhead"><button data-page="service">‹ Сервис</button><h2>Обновление</h2></div>
 <div class="card update-panel">
  <div class="row"><span>Установлено</span><b id="update-current">—</b></div>
  <div class="row"><span>Последняя версия</span><b id="update-latest">Ещё не проверено</b></div>
  <div class="row"><span>Состояние</span><b id="update-phase">Ожидание панели</b></div>
  <p id="update-message" role="status" aria-live="polite"></p>
  <progress id="update-progress" max="100" value="0" aria-label="Ход обновления"></progress>
  <p id="update-progress-text" class="tiny muted"></p><p id="update-error" class="error" role="alert"></p>
  <div id="update-result" class="notice hidden" role="status" aria-live="polite"></div>
  <p id="update-auth" class="hint"></p>
  <div class="actions"><button id="update-check" data-action="update_check" data-write="1">Проверить</button><button id="update-install" class="primary" data-action="update_install" data-write="1">Установить</button><button id="update-cancel" class="ghost hidden" data-action="update_cancel" data-write="1">Отменить обновление</button></div>
  <details id="update-rollback-box" class="details hidden"><summary>Вернуть предыдущую версию</summary><p id="update-previous" class="tiny"></p><button id="update-rollback" class="ghost" data-action="update_rollback" data-write="1">Вернуть версию</button></details>
  <p class="tiny muted">Проверка новых выпусков выполняется автоматически. Установка начинается после вашего подтверждения. Во время установки сохраняйте питание панели; после неё панель перезапустится.</p>
 </div>
</section><footer><span>Boiler Panel <span id="version">подключение...</span></span><span id="net" class="status">Подключение…</span><span id="last-update">Ожидание панели</span></footer></div><div id="toast" class="hidden" role="status" aria-live="polite"></div><script>
(()=>{
"use strict";
const $=id=>document.getElementById(id);
const names=["Бойлер косвенного нагрева","Электрокотёл","Контур 1","Контур 2","Контур 3","Дизельный отопитель","Подогрев топлива"];
const tnames=["В доме","На улице","Подача","Топливо в баке","Обратка","Топливо на выходе"];
// Saved input IDs 4 and 5 are reed switches. Appended temperatures follow them.
const inputNames=[...tnames.slice(0,4),"Нижний геркон","Верхний геркон",...tnames.slice(4)];
const isReedInput=channel=>channel===4||channel===5;
function selectedPinChannel(){
 const value=$("pin-channel").value,channel=Number(value);
 return value!==""&&Number.isInteger(channel)&&channel>=0&&channel<inputNames.length?channel:-1;
}
// Physical JP1 order; assignable GPIOs match panel_pin_profile.h.
const jp1Pins=[
 ["3V3","power","Питание"],["5V","power","Питание"],
 ["3V3","power","Питание"],["5V","power","Питание"],
 ["GND","gnd","Общий"],["GND","gnd","Общий"],
 ["GPIO52","gpio",52],["GPIO33","gpio",33],
 ["GPIO51","gpio",51],["GPIO31","gpio",31],
 ["GPIO50","gpio",50],["GPIO30","gpio",30],
 ["GPIO49","gpio",49],["GPIO29","gpio",29],
 ["GPIO35","boot","BOOT · резерв"],["GND","gnd","Общий"],
 ["GPIO34","gpio",34],["ESP_3V3","power","Служебное питание"],
 ["GPIO32","gpio",32],["C6_U0RXD","c6","ESP32-C6 · резерв"],
 ["GPIO28","gpio",28],["C6_U0TXD","c6","ESP32-C6 · резерв"],
 ["I2C_SDA","i2c","I2C · резерв"],["C6_IO9","c6","ESP32-C6 · резерв"],
 ["I2C_SCL","i2c","I2C · резерв"],["C6_CHIP_PU","c6","ESP32-C6 · резерв"]
];
const jp1Nodes=[];
let token=sessionStorage.getItem("boiler_panel_token")||"",current=null,page="home",tab="",detail=-1,detailTrigger=null;
let lastServerError="",lastToast=null,refreshInFlight=false,serverOnline=false;
let pairTicket="",pairDeadline=0,pairGeneration=0,pairBusy=false;
let haFormRevision=null,haDirty=false;
const wifiRows=new Map();let wifiRequestInFlight=false,wifiChoicesSignature="";
let updateRequestInFlight=false,pendingUpdate=null,updateResult="",updateResultError=false;
let pinSnapshot=null,pinEditor=null,pinPollSerial=0,pinPollStarted=0,pinPollQueued=false;
try{const saved=JSON.parse(sessionStorage.getItem("boiler_panel_update")||"null");if(saved&&typeof saved.target==="string"&&saved.target.length>0&&saved.target.length<24&&typeof saved.from==="string"&&["update_install","update_rollback"].includes(saved.action)&&Number.isFinite(saved.at)&&Date.now()>=saved.at&&Date.now()-saved.at<3600000)pendingUpdate=saved;}catch(_){}
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
function readPinSnapshot(cfg){
 const seen=new Set(),assigned=new Map(),items=[];
 let valid=!!cfg&&cfg.available===true&&Number.isInteger(cfg.revision)&&cfg.revision>=0&&cfg.revision<=4294967295&&Array.isArray(cfg.items)&&cfg.items.length===inputNames.length;
 if(valid)for(const item of cfg.items){
  if(!item||!Number.isInteger(item.channel)||item.channel<0||item.channel>=inputNames.length||seen.has(item.channel)){valid=false;break;}
  seen.add(item.channel);items[item.channel]=item;
  if(["ds18b20","reed_no","reed_nc"].includes(item.type)){
   const pin=jp1Nodes.find(node=>node.kind==="gpio"&&node.value===item.gpio);
   if(!pin||item.jp1_pin!==pin.index+1||(item.type==="ds18b20")!==(!isReedInput(item.channel))){valid=false;break;}
   if(!assigned.has(item.gpio))assigned.set(item.gpio,[]);
   assigned.get(item.gpio).push(item);
  }else if(!["none","modbus","mqtt"].includes(item.type)||item.gpio!==-1){valid=false;break;}
 }
 const conflict=valid&&Array.from(assigned.values()).some(owners=>owners.length>1);
 // Occupancy needs the GPIO/type mapping; editing also needs every saved field.
 const editable=valid&&!conflict&&items.every(item=>Number.isInteger(item.debounce_ms)&&item.debounce_ms>=0&&item.debounce_ms<=5000&&
  Number.isInteger(item.address)&&item.address>=0&&item.address<=247&&
  (item.type==="modbus"?item.address>=1:item.address===0)&&
  (item.type!=="ds18b20"||item.debounce_ms===50)&&
  (item.gpio>=0||item.jp1_pin===0));
 return {valid,conflict,editable,assigned,items,revision:valid?cfg.revision:null};
}
function renderPinout(cfg){
 if(!jp1Nodes.length){
  const map=$("jp1-map");let row;
  jp1Pins.forEach(([name,kind,value],index)=>{
   if(index%2===0){row=document.createElement("div");row.className="jp1-row";map.append(row);}
   const pin=document.createElement(kind==="gpio"?"button":"div"),label=document.createElement("span"),number=document.createElement("span"),status=document.createElement("span");
   pin.className="jp1-pin"+(index%2?" right":"");pin.dataset.pin=String(index+1);pin.dataset.kind=kind;
   pin.dataset.state=kind==="gpio"?"unknown":"fixed";
   if(kind==="gpio"){pin.type="button";pin.dataset.gpio=String(value);pin.setAttribute("aria-haspopup","dialog");pin.setAttribute("aria-controls","pin-dialog");}
   label.className="jp1-name";label.textContent=name;
   number.className="jp1-number";number.textContent=String(index+1);number.setAttribute("aria-label","Контакт "+(index+1));
   status.className="jp1-status";status.textContent=kind==="gpio"?"Нет данных":value;
   if(index%2)pin.append(number,label,status);else pin.append(label,number,status);
   row.append(pin);jp1Nodes.push({pin,status,kind,value,index});
  });
 }
 pinSnapshot=readPinSnapshot(cfg);
 const {valid,conflict,assigned}=pinSnapshot;
 for(const node of jp1Nodes){
  if(node.kind!=="gpio")continue;
  const owners=(assigned.get(node.value)||[]).map(item=>inputNames[item.channel]);
  let state="unknown",label=lastServerError&&!serverOnline?"Нет связи":"Нет данных";
  if(serverOnline&&valid){
   if(conflict){if(owners.length>1){state="conflict";label="Конфликт · "+owners.join(" / ");}}
   else{state=owners.length?"busy":"free";label=owners.length?"Занят · "+owners[0]:"Свободен";}
  }
  if(node.pin.dataset.state!==state)node.pin.dataset.state=state;
  if(node.status.textContent!==label)node.status.textContent=label;
  node.pin.setAttribute("aria-label","GPIO"+node.value+", контакт JP1 "+(node.index+1)+". "+label+". Настроить пин");
 }
 const message=!serverOnline?(lastServerError?"Нет связи с панелью. Занятость контактов неизвестна.":"Ожидаем назначения с панели."):
  !valid?"Нет достоверных назначений. Занятость контактов неизвестна.":
  conflict?"Конфликт назначений. Свободные контакты не определены.":"Сохранённые назначения · ревизия "+cfg.revision;
 if($("jp1-state").textContent!==message)text("jp1-state",message);
}
function copyPinItems(items){return items.map(item=>({channel:item.channel,type:item.type,gpio:item.gpio,address:item.address,debounce_ms:item.debounce_ms}));}
function samePinItems(left,right){return left.length===right.length&&left.every((item,i)=>["channel","type","gpio","address","debounce_ms"].every(key=>item[key]===right[i][key]));}
function pinOwner(editor){return editor.base?.items.find(item=>item.gpio===editor.gpio)||null;}
function pinTypeLabel(type){return ({ds18b20:"DS18B20",reed_no:"Геркон NO",reed_nc:"Геркон NC",mqtt:"MQTT",modbus:"Modbus"})[type]||"Не назначено";}
function loadPinDraft(editor){
 editor.base=pinSnapshot?.editable&&serverOnline?{revision:pinSnapshot.revision,items:copyPinItems(pinSnapshot.items)}:null;
 editor.stale=false;editor.needsReload=false;editor.awaiting=null;editor.reloadAfter=null;
 const owner=pinOwner(editor);
 $("pin-channel").value=owner?String(owner.channel):"";
 $("pin-type").value=owner?.type==="reed_no"?"2":owner?.type==="reed_nc"?"3":"";
 $("pin-debounce").value=String(owner?.debounce_ms??50);
}
function openPinEditor(button){
 if(pinEditor?.pending)return;
 const node=jp1Nodes.find(node=>node.pin===button&&node.kind==="gpio");if(!node)return;
 pinEditor={gpio:node.value,button,pending:false,message:"",problem:false};loadPinDraft(pinEditor);
 text("pin-title","GPIO"+node.value);text("pin-location","JP1 · контакт "+(node.index+1));
 renderPinEditor();$("pin-dialog").showModal();$("pin-channel").focus();
}
function closePinEditor(){
 if(pinEditor?.pending)return;
 const button=pinEditor?.button;$("pin-dialog").close();pinEditor=null;
 if(button?.isConnected)button.focus();
}
function pinDraft(){
 const channel=selectedPinChannel();
 if(channel<0)return null;
 if(!isReedInput(channel))return {channel,type:1,debounce:50};
 const type=Number($("pin-type").value),raw=$("pin-debounce").value;
 if(![2,3].includes(type)||!/^\d+$/.test(raw)||Number(raw)>5000)return null;
 return {channel,type,debounce:Number(raw)};
}
function pinBlockReason(editor){
 if(!serverOnline)return "Нет связи с панелью. Несохранённый выбор сохранён в этом окне.";
 if(!pinSnapshot?.editable)return "Нет полного достоверного снимка назначений. Сохранение недоступно.";
 if(editor.needsReload)return editor.message||"Сохранение не подтверждено. Перечитайте назначения.";
 if(editor.stale)return "Назначения изменились. Перечитайте их перед сохранением; ваш выбор пока сохранён в этом окне.";
 if(!editor.base)return "Перечитайте назначения, чтобы начать настройку по свежим данным.";
 if(!current?.authorized||!token)return "Для сохранения выполните сопряжение. Ваш выбор останется в этом окне.";
 if(current?.update?.busy||current?.update?.boot_pending||pendingUpdate||updateRequestInFlight)return "Дождитесь завершения обновления панели. Ваш выбор сохранён в этом окне.";
 return "";
}
function renderPinEditor(){
 const editor=pinEditor;if(!editor)return;
 if(editor.reloadAfter!=null&&pinPollSerial>editor.reloadAfter){
  editor.reloadAfter=null;
  if(serverOnline&&pinSnapshot?.editable){loadPinDraft(editor);editor.message="Назначения перечитаны. Выберите действие.";editor.problem=false;}
  else{editor.message="Не удалось перечитать назначения. Ваш выбор сохранён в этом окне.";editor.problem=true;}
 }
 if(editor.awaiting){
  const ack=editor.awaiting;
  if(pinPollSerial>ack.after&&serverOnline&&pinSnapshot?.editable&&pinSnapshot.revision===ack.revision&&samePinItems(pinSnapshot.items,ack.items)){
   const clear=ack.clear;loadPinDraft(editor);
   editor.message=clear?"Пин освобождён. Сохранённые назначения подтверждены панелью.":"Сохранено. Назначения подтверждены панелью.";editor.problem=false;
  }else if(Date.now()>=ack.deadline||(pinPollSerial>ack.after&&serverOnline&&pinSnapshot?.editable&&pinSnapshot.revision!==editor.base.revision&&pinSnapshot.revision!==ack.revision)){
   editor.awaiting=null;editor.needsReload=true;editor.message="Сохранение не подтверждено. Перечитайте назначения.";editor.problem=true;
  }
 }
 if(!editor.pending&&!editor.awaiting&&editor.base&&pinSnapshot?.editable&&pinSnapshot.revision!==editor.base.revision)editor.stale=true;
 const owner=pinOwner(editor),channel=selectedPinChannel(),reed=isReedInput(channel);
 text("pin-current",editor.base?(owner?"Занят: "+inputNames[owner.channel]+" · "+pinTypeLabel(owner.type):"Свободен — датчик не назначен"):"Занятость не подтверждена");
 text("pin-base-revision",editor.base?"Снимок назначений · ревизия "+editor.base.revision:"");
 show("pin-temperature",channel>=0&&!reed);show("pin-reed",reed);
 text("pin-wiring",channel<0?"Выберите, показания какого датчика получать с этого GPIO.":reed?"Геркон подключается между выбранным GPIO и GND. Выберите NO/NC по схеме вашего датчика; обрыв провода отдельно не определяется.":"DS18B20: один датчик на GPIO, внешняя подтяжка 4,7 кОм к 3,3 В. Питание и GND — по схеме JP1.");
 const impact=[];
 if(channel>=0&&editor.base){
  const target=editor.base.items[channel];
  if(owner&&owner.channel!==channel)impact.push("«"+inputNames[owner.channel]+"» будет отключён от GPIO"+editor.gpio+".");
  if(target.gpio>=0&&target.gpio!==editor.gpio)impact.push("«"+inputNames[channel]+"» перейдёт с GPIO"+target.gpio+" на GPIO"+editor.gpio+". GPIO"+target.gpio+" освободится.");
  else if(["modbus","mqtt"].includes(target.type))impact.push("Источник "+pinTypeLabel(target.type)+" для «"+inputNames[channel]+"» будет заменён датчиком на GPIO"+editor.gpio+".");
  else impact.push("Назначение: «"+inputNames[channel]+"» → GPIO"+editor.gpio+".");
 }
 text("pin-impact",impact.join("\n"));show("pin-impact",impact.length>0);
 const working=editor.pending||!!editor.awaiting||editor.reloadAfter!=null,blocked=pinBlockReason(editor),draft=pinDraft();
 $("pin-dialog").setAttribute("aria-busy",working?"true":"false");
 $("pin-channel").disabled=working;$("pin-type").disabled=working||!reed;$("pin-type").required=reed;
 $("pin-debounce").disabled=working||!reed;$("pin-debounce").required=reed;
 $("pin-close").disabled=editor.pending;
 $("pin-save").disabled=working||!!blocked||!draft;
 show("pin-clear",!!owner);$("pin-clear").disabled=working||!!blocked||!owner;
 const reload=editor.stale||editor.needsReload||!editor.base||!pinSnapshot?.editable||editor.reloadAfter!=null;
 show("pin-reload",reload);show("pin-reload-hint",reload);
 $("pin-reload").disabled=working;
 show("pin-pair",!current?.authorized||!token);$("pin-pair").disabled=working||!serverOnline;
 let message=editor.pending?"Сохраняем назначение…":editor.awaiting?"Запрос принят. Проверяем сохранённые назначения на панели…":editor.reloadAfter!=null?"Перечитываем назначения…":blocked||editor.message;
 if(!message&&!draft)message=channel<0?"Выберите назначение для этого пина.":reed&&!$("pin-type").value?"Выберите режим NO или NC.":"Дребезг: целое число от 0 до 5000 мс.";
 text("pin-dialog-status",message);$("pin-dialog-status").classList.toggle("error",!!blocked||editor.problem);
}
function changePinChannel(){
 const editor=pinEditor;if(!editor||editor.pending||editor.awaiting)return;
 const channel=selectedPinChannel(),target=editor.base?.items[channel];
 $("pin-type").value=target?.type==="reed_no"?"2":target?.type==="reed_nc"?"3":"";
 $("pin-debounce").value=String(target?.debounce_ms??50);
 editor.message="";editor.problem=false;renderPinEditor();
}
function reloadPinEditor(){
 const editor=pinEditor;if(!editor||editor.pending||editor.awaiting||editor.reloadAfter!=null)return;
 editor.reloadAfter=pinPollStarted;editor.message="";editor.problem=false;renderPinEditor();pollPinFresh();
}
function pollPinFresh(){
 // An already-running GET may predate this action. Queue one subsequent GET.
 if(refreshInFlight){pinPollQueued=true;return;}
 poll();
}
async function savePin(clear=false){
 const editor=pinEditor;if(!editor||editor.pending||editor.awaiting||editor.reloadAfter!=null)return;
 renderPinEditor();if(pinBlockReason(editor))return;
 const owner=pinOwner(editor),draft=pinDraft();if(clear?!owner:!draft)return;
 if(clear&&!confirm("Освободить GPIO"+editor.gpio+"? Назначение «"+inputNames[owner.channel]+"» будет отключено."))return;
 const items=copyPinItems(editor.base.items);
 for(let i=0;i<items.length;i++)if(items[i].gpio===editor.gpio)items[i]={channel:i,type:"none",gpio:-1,address:0,debounce_ms:50};
 if(!clear)items[draft.channel]={channel:draft.channel,type:["none","ds18b20","reed_no","reed_nc"][draft.type],gpio:editor.gpio,address:0,debounce_ms:draft.debounce};
 const data={action:clear?"connection_pin_clear":"connection_pin_set",gpio:editor.gpio,revision:editor.base.revision};
 if(!clear)Object.assign(data,draft);
 editor.pending=true;editor.message="";editor.problem=false;renderPinEditor();
 try{
  const result=await sendPost("/api/command",data,true);
  if(result.ok!==true||result.saved!==true||!Number.isInteger(result.revision)||result.revision<0||result.revision>4294967295)throw Error("Сохранение не подтверждено");
  editor.awaiting={revision:result.revision,items,clear,after:pinPollStarted,deadline:Date.now()+12000};
  setTimeout(()=>{if(pinEditor===editor)renderPinEditor();},12050);
 }catch(error){
  editor.problem=true;
  if(error.code==="stale_revision"){editor.stale=true;editor.message="Назначения изменились. Перечитайте их перед сохранением.";}
  else if(!error.httpStatus||error.httpStatus>=500){editor.needsReload=true;editor.message="Сохранение не подтверждено. Перечитайте назначения. Повторный запрос автоматически не отправляется.";}
  else editor.message=error.message;
 }finally{
  editor.pending=false;renderPinEditor();pollPinFresh();
 }
}
function drawPage(){
 for(const n of document.querySelectorAll(".page"))n.classList.toggle("hidden",n.id!==page);
 for(const n of document.querySelectorAll(".topnav [data-page]"))n.classList.toggle("active",n.dataset.page===(["connections","ha","service","update"].includes(page)?"settings":page));
 show("settings-menu",!tab);show("settings-back",!!tab);
 for(const n of document.querySelectorAll(".tab"))n.classList.toggle("hidden",n.id!=="tab-"+tab);
 for(const n of document.querySelectorAll("[data-tab]"))n.classList.toggle("active",n.dataset.tab===tab);
 updateView();
}
function setTab(newTab){tab=newTab;page="settings";drawPage()}
function updateDetail(){
 if(detail<0||!current){show("temperature-detail",false);return}
 const t=current.temperatures?.[detail]||{};
 show("temperature-detail",true);
 text("detail-title",tnames[detail]+" — подробности");
 text("detail-meta",!serverOnline?"Нет связи с панелью":t.valid?"Свежие данные":"Датчик не назначен или данные устарели");
 text("detail-stats",t.ever?("Минимум: "+t.minimum+" °C · Максимум: "+t.maximum+" °C · Измерений в памяти: "+(t.history||[]).length+" · "+(t.trend==null?"Тренд отсутствует":"Тренд "+(t.trend>0?"+":"")+t.trend+" °C")):"Реальных измерений и истории пока нет");
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
const heatingSettings={
 boiler:{status:"boiler_settings",action:"boiler_thresholds",temperature:"Температура воды в баке",
   button:"Включить бойлер",hardware:"Не подключены датчик воды и контроллер нагрева.",unavailable:"Настройки бойлера недоступны."},
 electric:{status:"electric_settings",action:"electric_thresholds",temperature:"Температура электрокотла",
   button:"Включить ТЭНы",hardware:"Не подключены датчик температуры электрокотла и контроллер ТЭНов.",unavailable:"Настройки электрокотла недоступны."}
};
// Each equipment card keeps its own draft, original revision and save/readback state.
const thresholdViews=[];
function validThresholdConfig(b){
 if(!b||!Number.isInteger(b.revision)||b.revision<0||b.revision>0xffffffff)return null;
 if(b.configured===false&&b.revision===0&&b.on_c===null&&b.off_c===null)return b;
 return b.configured===true&&b.revision>0&&Number.isInteger(b.on_c)&&Number.isInteger(b.off_c)&&
   b.on_c>=0&&b.on_c<b.off_c&&b.off_c<=99?b:null;
}
function mountThresholdEditor(card,kind){
 const settings=heatingSettings[kind], content=$("threshold-controls").content.cloneNode(true), fields={};
 for(const part of content.querySelectorAll("[data-part]")){
   fields[part.dataset.part]=part;
   part.id=kind+"-"+part.dataset.part;
 }
 for(const label of content.querySelectorAll("[data-label]"))label.htmlFor=fields[label.dataset.label].id;
 fields.enable.textContent=settings.button;fields.enable.setAttribute("aria-describedby",fields.hardware.id);
 fields.hardware.textContent=settings.hardware;fields.temperature.textContent=settings.temperature;
 card.append(content);
 const e={base:null,dirty:false,saving:false,awaiting:null,reloadAfter:null,review:false,message:""};
 const readConfig=()=>validThresholdConfig(current?.[settings.status]);
 function requestStatus(){if(refreshInFlight)pinPollQueued=true;else poll();}
 function allowed(){
  const u=current?.update;
  return serverOnline&&!!token&&current?.authorized===true&&!!readConfig()&&u?.started===true&&
    u.busy===false&&u.boot_pending===false&&!pendingUpdate&&!updateRequestInFlight;
 }
 function values(){
  const on=fields.on.value,off=fields.off.value;
  return /^\d{1,2}$/.test(on)&&/^\d{1,2}$/.test(off)&&Number(on)<Number(off)?{on:Number(on),off:Number(off)}:null;
 }
 function render(){
  const b=readConfig(),valid=serverOnline&&!!b;
  if(e.reloadAfter!==null&&pinPollSerial>e.reloadAfter){
    e.reloadAfter=null;
    if(valid){e.base=null;e.dirty=e.review=false;e.message="Показаны сохранённые пороги.";}
    else e.message="Перечитать настройки не удалось. Введённые значения оставлены в форме.";
  }
  if(e.awaiting&&pinPollSerial>e.awaiting.after){
    const a=e.awaiting;e.awaiting=null;
    if(valid&&current?.authorized===true&&token===a.token&&b.configured&&b.revision===a.revision&&b.on_c===a.on&&b.off_c===a.off){
      e.base=null;e.dirty=e.review=false;e.message="Оба порога сохранены на панели.";
    }else{e.review=true;e.message="Сохранение не подтверждено. Перечитайте пороги с панели.";}
  }
  if(valid&&!e.dirty&&!e.saving&&!e.awaiting&&e.base!==b.revision){
    fields.on.value=b.configured?String(b.on_c):"";fields.off.value=b.configured?String(b.off_c):"";
    e.base=b.revision;
  }
  const stale=e.dirty&&valid&&e.base!==b.revision;
  const busy=e.saving||!!e.awaiting||e.reloadAfter!==null;
  const editable=allowed()&&!busy&&!e.review&&!stale;
  fields.on.disabled=fields.off.disabled=!editable;
  fields.save.disabled=!editable||!e.dirty||!values();
  fields.reload.classList.toggle("hidden",!(e.dirty||e.review));fields.reload.disabled=!allowed()||busy;
  const hint=!serverOnline?"Нет связи с панелью.":!current?.authorized||!token?"Для изменения порогов выполните сопряжение.":
    !b?settings.unavailable:busy?"Проверяем настройки панели…":
    e.review?e.message:stale?"Пороги изменились на панели. Перечитайте их перед сохранением.":
    !allowed()?"Изменение недоступно, пока не подтверждено завершение обновления.":
    e.dirty&&!values()?"Введите два целых числа от 0 до 99. Порог включения должен быть ниже порога выключения.":
    e.dirty?"Изменения не сохранены. Нажмите «Сохранить пороги».":
    e.message||(b.configured?"Показаны сохранённые пороги.":"Пороги пока не заданы.");
  if(fields.feedback.textContent!==hint)fields.feedback.textContent=hint;
 }
 fields.form.addEventListener("input",()=>{e.dirty=true;e.message="";render();});
 fields.reload.addEventListener("click",()=>{
   if(fields.reload.disabled)return;
   e.reloadAfter=pinPollStarted;render();requestStatus();
 });
 fields.form.addEventListener("submit",async event=>{
   event.preventDefault();const pair=values(),b=readConfig(),session=token;
   if(!allowed()||!pair||!e.dirty||e.saving||e.awaiting||e.review||e.reloadAfter!==null||e.base!==b.revision)return;
   e.saving=true;render();
   try{
     const ack=await sendPost("/api/command",{action:settings.action,revision:String(e.base),on_c:String(pair.on),off_c:String(pair.off)},true);
     if(!ack||ack.ok!==true||ack.saved!==true||!Number.isInteger(ack.revision)||ack.revision<1||
        ack.revision>0xffffffff||session!==token)throw Error("unconfirmed");
     e.awaiting={...pair,revision:ack.revision,after:pinPollStarted,token:session};
   }catch(_){e.review=true;e.message="Сохранение не подтверждено. Перечитайте пороги с панели.";}
   finally{e.saving=false;render();requestStatus();}
 });
 return render;
}
function renderEquipment(){
 const items=current?.equipment||[],holder=$("equipment-list");
 for(const [index,title] of [[0,"Бойлер"],[1,"Электрокотёл"],[5,"Дизельный отопитель"],[6,"Подогрев топлива"]]){
  // API positions identify fixed equipment types. Retain details/summary so polls
  // preserve the user's open/closed state and native keyboard focus.
  let card=holder.querySelector('[data-equipment="'+index+'"]');
  if(!card){
   card=document.createElement("details");card.className="card equip";card.dataset.equipment=String(index);
   const summary=document.createElement("summary"),heading=document.createElement("h3"),status=document.createElement("p");
   heading.textContent=title;summary.append(heading,status);card.append(summary);
   if(index>1){const explanation=document.createElement("p");explanation.textContent="Управление недоступно до подключения и проверки контроллера.";card.append(explanation);}
   if(index===1)for(const n of [2,3,4]){const row=document.createElement("div");row.className="row";row.dataset.circuit=String(n);row.append(document.createElement("span"),document.createElement("span"));card.append(row);}
   holder.append(card);
   if(index===0||index===1)thresholdViews[index]=mountThresholdEditor(card,index===0?"boiler":"electric");
  }
  thresholdViews[index]?.();
  const connected=items[index]?.connected;
  card.querySelector("summary p").textContent=connected===true?"Подключён":connected===false?"Не подключён":"Нет данных";
  if(index===1)for(const n of [2,3,4]){const row=card.querySelector('[data-circuit="'+n+'"]');row.children[0].textContent=items[n]?.name||names[n];row.children[1].textContent=items[n]?.connected?"Подключён":"Нет данных";}
 }
}
function wifiReady(){
 const d=current,w=d?.wifi;
 return !!(token&&serverOnline&&d?.authorized&&w&&w.ready!==false&&w.supported&&!w.busy&&
   !d.update?.busy&&!d.update?.boot_pending&&!pendingUpdate&&!updateRequestInFlight&&!wifiRequestInFlight);
}
const profileKey=n=>JSON.stringify([n.ssid,n.open]);
const apKey=a=>JSON.stringify([a.ssid,a.auth,!!a.saved,!!a.supported]);
function selectedProfile(){return (current?.wifi?.savedNetworks||[]).find(n=>profileKey(n)===$("wifi-profile").value);}
function wifiForm(ssid,open){
 $("ssid").value=ssid;$("wifi-open").checked=open;
 const pass=$("wifi-pass");pass.value="";pass.disabled=open;
 if(!open)pass.focus();else $("wifi-form").querySelector("button").focus();
}
async function wifiWrite(action,params){
 if(!wifiReady())return;
 wifiRequestInFlight=true;renderAP();
 try{await command(action,params);await poll();}
 finally{wifiRequestInFlight=false;renderAP();}
}
function renderAP(){
 const d=current||{},w=d.wifi||{},auth=!!(d.authorized&&token),enabled=wifiReady();
 const profiles=auth?w.savedNetworks||[]:[],select=$("wifi-profile");
 const signature=JSON.stringify(profiles.map(profileKey));
 if(signature!==wifiChoicesSignature){
   const old=select.value;select.replaceChildren(new Option("Выберите сеть",""));
   for(const n of profiles)select.add(new Option(n.ssid+" · "+(n.open?"Открытая":"Защищённая"),profileKey(n)));
   select.value=profiles.some(n=>profileKey(n)===old)?old:"";wifiChoicesSignature=signature;
 }
 const profile=auth&&selectedProfile();
 select.disabled=!enabled||!profiles.length;
 $("wifi-edit-password").disabled=!enabled||!profile||profile.open;
 document.querySelector('[data-action="wifi_forget"]').disabled=!enabled||!profile;
 $("wifi-form").querySelector("button").disabled=!enabled;
 const aps=auth?w.aps||[]:[],list=$("ap-list"),active=new Set(),next=[];
 for(const ap of aps){
   const key=apKey(ap);active.add(key);
   let b=wifiRows.get(key);
   if(!b){
     b=document.createElement("button");b.type="button";b.className="ghost";b.style.width="100%";b.style.textAlign="left";b.style.margin="4px 0";
     b.dataset.saved=ap.saved?"1":"0";b.dataset.write="1";wifiRows.set(key,b);
     b.addEventListener("click",()=>{
       if(!wifiReady()||!ap.supported||!current.wifi.aps.some(a=>apKey(a)===key))return;
       if(ap.saved){wifiWrite("wifi_connect",{ssid:ap.ssid,open:ap.auth===0?"1":"0",saved:"1"});return;}
       wifiForm(ap.ssid,ap.auth===0);
       notice(ap.auth===0?"Открытая сеть выбрана. Нажмите «Подключиться».":"Сеть выбрана. Введите пароль и нажмите «Подключиться».");
     });
   }
   const caption=ap.ssid+" · "+(ap.saved?"Сохранена · ":"")+ap.rssi+" dBm · "+(ap.auth===0?"Открытая":ap.supported?"WPA Personal":"Не поддерживается");
   if(b.textContent!==caption)b.textContent=caption;
   b.disabled=!enabled||!ap.supported;next.push(b);
 }
 for(const key of wifiRows.keys())if(!active.has(key))wifiRows.delete(key);
 if(!next.length){
   const msg=auth?"Сетей в списке нет. Нажмите «Найти сети».":"После сопряжения можно искать сети и подключаться.";
   if(list.textContent!==msg)list.textContent=msg;
 }else if(list.children.length!==next.length||next.some((b,i)=>list.children[i]!==b))list.replaceChildren(...next);
 if(!auth){$("ssid").value="";$("wifi-pass").value="";$("wifi-open").checked=false;}
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
 for(const el of $("ha-form").querySelectorAll("input"))el.disabled=!admin||!h.ready||!!d.update?.busy||!!d.update?.boot_pending;
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
 if(current.update?.busy||current.update?.boot_pending){notice("Дождитесь завершения обновления панели",true);return;}
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
async function request(path,options){
 const c=new AbortController(),timer=setTimeout(()=>c.abort(),8000);
 try{
  const response=await fetch(path,{...options,signal:c.signal});
  // Headers alone do not finish a request: the body has the same deadline.
  const body=await response.text();return {response,body};
 }finally{clearTimeout(timer);}
}
function errorText(code){return ({wifi_profile_changed:"Сохранённая сеть изменилась. Выберите её снова.",stale_revision:"Назначения изменились. Перечитайте их перед сохранением.",invalid_pin_fields:"Проверьте поля назначения пина.",invalid_pin_assignment:"Этот датчик нельзя назначить на выбранный пин.",configuration_unavailable:"Назначения временно недоступны. Перечитайте их.",configuration_write_failed:"Сохранение не подтверждено. Перечитайте назначения.",same_origin_required:"Запрос разрешён только из локальной страницы панели",display_not_ready:"Дисплей ещё запускается",pairing_in_progress:"Код уже запрошен в другом браузере. Завершите тот запрос или дождитесь истечения срока.",pairing_rate_limited:"Слишком много попыток. Подождите до двух минут и запросите новый код.",code_invalid_or_expired:"Неверный или просроченный код",pair_required:"Выполните сопряжение",invalid_code:"Введите шесть цифр",ha_configuration_changed_reload:"Настройки изменились. Перечитайте форму",ha_invalid_or_password_required_for_new_broker:"Проверьте адрес брокера и заново введите пароль при смене сервера",update_busy:"Дождитесь завершения обновления панели",update_unavailable:"Обновление пока недоступно",update_state_unavailable:"Состояние обновления временно недоступно. Повторите запрос.",update_confirmation_required:"Подтвердите выбранную версию ещё раз",update_version_changed:"Доступная версия изменилась. Проверьте сведения и подтвердите нужную версию.",update_not_ready:"Обновление не принято. Проверьте состояние панели.",update_cancel_unavailable:"На этом этапе отмена уже недоступна"})[code]||code;}
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
 $("fuel-open").setAttribute("aria-label","Уровень топлива: "+title+". Открыть раздел топлива");
 text("canister-title",title);text("canister-symbol",symbol);
 $("canister-fill").setAttribute("height",height);$("canister-fill").setAttribute("y",136-height);
 $("canister-cross").setAttribute("visibility",height?"visible":"hidden");
 text("fuel-status",title);
}
function markOffline(){
 for(let i=0;i<tnames.length;i++){text("t"+i,"--.- °C");text("tstate"+i,"Нет связи");}
 text("fuel-temp","Нет связи");text("fuel-outlet-temp","Нет связи");text("lower-status","Нет связи");text("upper-status","Нет связи");updateCanister(null);
}
function updateView(){
 const d=current;renderPinout(d&&d.connections);if(!d){renderPinEditor();return;}
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
 for(let i=0;i<tnames.length;i++){
   const t=d.temperatures&&d.temperatures[i]||{};
   const value=t.valid&&typeof t.value==="number"?t.value.toFixed(1)+" °C":"--.- °C";
   const status=t.valid?"":t.ever?"Устарело":"Нет данных";
   text("t"+i,value);text("tstate"+i,status);
   if(i===3)text("fuel-temp",status||value);
   if(i===5)text("fuel-outlet-temp",status||value);
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
    for(const item of (Array.isArray(cfg.items)?cfg.items:[])){
      if(!item||typeof item!=="object")continue;
      const row=document.createElement("div");row.className="row";
      const title=document.createElement("span");title.textContent=inputNames[item.channel]||"Канал";
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
 formButton.disabled=!d.authorized || !!sd.format_attempted || formatActive || !!d.update?.busy || !!d.update?.boot_pending;
 // The last step still requires physically touching the separate panel button.
 if(document.activeElement!==$("zone-select"))$("zone-select").value=String(tm.zone||0);
 if(document.activeElement!==$("screen-select"))$("screen-select").value=String(screen.timeout_s||0);
 for(const b of document.querySelectorAll('[data-write="1"]')){
   const wifiAction=b.dataset.action&&b.dataset.action.startsWith("wifi_");
   const bleAction=b.dataset.action==="ble_scan";
   const needsTouch=b.dataset.action==="screen"&&numeric($("screen-select").value)>0;
   b.disabled=!serverOnline || !d.authorized || !!d.update?.busy || !!d.update?.boot_pending || !!pendingUpdate || updateRequestInFlight || (wifi.busy&&(wifiAction||bleAction||b.dataset.action==="zone"||b.dataset.action==="clock"||b.dataset.action==="screen"))
     || (needsTouch&&!screen.touch_ready) || (!wifi.supported&&wifiAction) || (!ble.supported&&bleAction)
     || (b.dataset.action==="sd_format_prepare" && (!!sd.format_attempted || formatActive));
 }
 renderAP();renderBLE();renderEquipment();renderHA();updateCanister(d.fuel);
 if(!serverOnline)markOffline();
 for(const b of document.querySelectorAll('[data-action="ha_discover"]'))b.disabled=!serverOnline||!d.authorized||!d.home_assistant?.enabled||!!d.update?.busy||!!d.update?.boot_pending||!!pendingUpdate||updateRequestInFlight;
 renderUpdate();
 renderPinEditor();
}
async function sendPost(path,data,secret){
 if(path==="/api/command"&&!String(data.action||"").startsWith("update_")&&(current?.update?.busy||current?.update?.boot_pending||pendingUpdate||updateRequestInFlight))throw Error("Дождитесь завершения обновления панели");
 const headers={"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8","X-Boiler-Pairing":"1"};
 if(secret&&token)headers.Authorization="Bearer "+token;
 const {response:r,body}=await request(path,{method:"POST",headers,body:new URLSearchParams(data),cache:"no-store"});
 let obj;
 try{obj=JSON.parse(body)}catch(_){obj={error:"invalid_response"}}
 if(r.status===401){token="";sessionStorage.removeItem("boiler_panel_token");}
 if(!r.ok){const failure=Error(errorText(obj.error||("HTTP "+r.status)));failure.httpStatus=r.status;failure.code=obj.error;throw failure;}
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
 renderAP();thresholdViews.forEach(render=>render());notice("Веб-управление отключено");
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
function saveUpdatePending(){
 try{if(pendingUpdate)sessionStorage.setItem("boiler_panel_update",JSON.stringify(pendingUpdate));else sessionStorage.removeItem("boiler_panel_update");}catch(_){}
}
function finishUpdate(message,problem=false){
 updateResult=message;updateResultError=problem;pendingUpdate=null;saveUpdatePending();
}
function reconcileUpdate(d){
 const u=d.update||{};
 if(!pendingUpdate){
   // automatic_rollback is a capability, not evidence that a rollback occurred.
   if(u.phase==="success"&&!u.boot_pending)finishUpdate("Обновление завершено. Установлена "+d.version+".");
   else if(u.phase==="rolled_back"&&!u.boot_pending)finishUpdate("Возврат завершён. Панель работает на "+d.version+".");
   return;
 }
 if(d.version===pendingUpdate.target&&!u.boot_pending&&(u.started||pendingUpdate.action==="update_rollback")){
   const prefix=pendingUpdate.connectionLost?"Связь восстановлена. ":"";
   finishUpdate(prefix+(pendingUpdate.action==="update_rollback"?"Возврат завершён. Панель работает на ":"Установлена ")+d.version+".");return;
 }
 if(u.busy||u.boot_pending){
   pendingUpdate.observed=true;
   if(u.phase==="rebooting"||u.boot_pending)pendingUpdate.rebootSeen=true;
   saveUpdatePending();return;
 }
 if(u.phase==="rolled_back"){
   finishUpdate("Панель работает на "+d.version+". Новая версия не прошла проверку; выполнен возврат.",true);return;
 }
 if(u.phase==="error"||u.phase==="cancelled"){
   finishUpdate(u.phase==="cancelled"?"Обновление отменено. Установлена "+d.version+".":"Обновление не завершено. Панель работает на "+d.version+".",u.phase==="error");return;
 }
 if(pendingUpdate.rebootSeen&&pendingUpdate.connectionLost&&d.version!==pendingUpdate.target){
   finishUpdate("Связь восстановлена. Панель работает на "+d.version+"; установка "+pendingUpdate.target+" не подтверждена.",true);return;
 }
 if(Date.now()-pendingUpdate.at>30000&&!pendingUpdate.rebootSeen){
   finishUpdate("Панель доступна с версией "+d.version+". Начало установки не подтверждено. Проверьте сведения перед повторной попыткой.",true);
 }
}
function updatePhaseText(u){
 if(u.boot_pending)return "Проверка запуска";
 return ({idle:"Ожидает проверки",checking:"Проверка обновлений",current:"Версия актуальна",available:"Доступна новая версия",downloading:"Загрузка",verifying:"Проверка файла",installing:"Установка",rebooting:"Перезапуск панели",success:"Обновление завершено",error:"Обновление не завершено",cancelled:"Обновление отменено",rolled_back:"Возврат завершён"})[u.phase]||"Ожидание панели";
}
function renderUpdate(){
 const d=current||{},u=d.update||{},admin=!!d.authorized&&!!token;
 const working=!!u.busy||!!u.boot_pending,ready=serverOnline&&admin&&u.started&&u.supported;
 const waiting=!!pendingUpdate;
 text("update-current",d.version||"—");text("update-latest",u.latest_version||"Ещё не проверено");
 text("update-phase",!serverOnline?"Ожидание связи":!u.started?"Запуск службы":!u.supported?"Недоступно":updatePhaseText(u));
 text("update-message",u.message||(!u.started?"Ожидаем сведения об обновлении.":""));
 text("update-service-summary",u.available?"Доступна "+u.latest_version:working?updatePhaseText(u):"Установлена "+(d.version||"—"));
 const percent=Math.max(0,Math.min(100,numeric(u.percent))),total=Math.max(0,numeric(u.total)),done=Math.max(0,numeric(u.done));
 $("update-progress").value=percent;show("update-progress",total>0||working);
 text("update-progress-text",total?percent+"% · "+done.toLocaleString("ru-RU")+" / "+total.toLocaleString("ru-RU")+" Б":"");
 text("update-error",u.error_code?"Код ошибки: "+u.error_code:"");
 let result=updateResult;
 if(!serverOnline&&(working||waiting))result="Связь с панелью прервалась. Ждём её возвращения для подтверждения результата.";
 else if(waiting&&u.boot_pending)result="Панель запущена с версией "+d.version+". Идёт проверка запуска.";
 else if(waiting&&!working)result="Ожидаем подтверждения установки "+pendingUpdate.target+".";
 text("update-result",result);show("update-result",!!result);
 $("update-result").classList.toggle("warn",updateResultError&&serverOnline&&!waiting);
 text("update-auth",!admin?"Для действий выполните сопряжение с физическим дисплеем.":working?"Другие настройки будут доступны после обновления.":"");
 $("update-check").disabled=!ready||working||waiting||updateRequestInFlight;
 $("update-install").disabled=!ready||working||waiting||updateRequestInFlight||!u.available||!u.latest_version;
 $("update-install").dataset.version=u.latest_version||"";
 $("update-cancel").disabled=!ready||!u.busy||!u.cancel_allowed||updateRequestInFlight;
 show("update-cancel",!!u.busy);
 $("update-rollback").disabled=!ready||working||waiting||updateRequestInFlight||!u.rollback_available||!u.previous_version;
 $("update-rollback").dataset.version=u.previous_version||"";
 show("update-rollback-box",!!u.rollback_available);
 text("update-previous","Сохранена версия "+(u.previous_version||"—")+". Для возврата понадобится подтверждение.");
 const offer=u.available||working||waiting||!!updateResult;
 show("update-offer",page!=="update"&&!!offer);
 text("update-offer-text",!serverOnline&&(working||waiting)?"Обновление: ожидаем связь с панелью":working?updatePhaseText(u)+(total?" · "+percent+"%":""):waiting?"Ожидаем установку "+pendingUpdate.target:u.available?"Доступно обновление "+u.latest_version:updateResult);
}
async function requestUpdate(action,shownVersion=""){
 if(updateRequestInFlight)return;
 if(!token||!current?.authorized){startPairing();return;}
 if(!serverOnline){notice("Дождитесь восстановления связи с панелью",true);return;}
 const u=current.update||{},change=action==="update_install"||action==="update_rollback";
 if(!u.started||!u.supported){notice("Обновление пока недоступно",true);return;}
 if(action==="update_cancel"){
   if(!u.busy||!u.cancel_allowed){notice("На этом этапе отмена уже недоступна",true);return;}
 }else if(u.busy||u.boot_pending||pendingUpdate){notice("Дождитесь завершения текущего обновления",true);return;}
 const values={action};
 if(change){
   const rollback=action==="update_rollback",version=String(shownVersion);
   if(!version||version!==(rollback?u.previous_version:u.latest_version)||!(rollback?u.rollback_available:u.available)){
     notice("Доступная версия изменилась. Проверьте сведения ещё раз.",true);return;
   }
   if(!confirm((rollback?"Вернуть версию ":"Установить версию ")+version+"?\nСейчас установлена "+current.version+". После установки панель перезапустится."))return;
   values.version=version;values.confirm=version;
   pendingUpdate={target:version,from:current.version,action,at:Date.now(),rebootSeen:false,connectionLost:false};
   saveUpdatePending();
 }
 updateResult="";updateResultError=false;updateRequestInFlight=true;updateView();
 try{
   await sendPost("/api/command",values,true);
   notice(action==="update_cancel"?"Отмена запрошена. Ожидаем состояние панели.":action==="update_check"?"Проверка запрошена.":"Установка "+values.version+" запрошена.");
   await poll();
 }catch(e){
   if(change&&e.httpStatus)finishUpdate("Запрос не принят: "+e.message,true);
   else if(change&&pendingUpdate){updateResult="Связь прервалась. Проверяем, приняла ли панель запрос.";pendingUpdate.connectionLost=true;saveUpdatePending();}
   else if(change)return; // A concurrent status response already supplied the final outcome.
   notice("Обновление: "+e.message,true);
 }finally{updateRequestInFlight=false;updateView();}
}
function handleAction(b){
 const action=b.dataset.action;
 if(["update_check","update_install","update_cancel","update_rollback"].includes(action)){requestUpdate(action,b.dataset.version);return;}
 if(action==="settings-root"){tab="";page="settings";drawPage();return;}
 if(action==="ha_reload"){haDirty=false;haFormRevision=null;renderHA();return;}
 if(action==="show-pair"||action==="pair-retry"){startPairing();return;}
 if(action==="hide-pair"){cancelPairing();return;}
 if(action==="pair"){login();return}
 if(action==="logout"){logout();return}
 if(action==="close-detail"){
  detail=-1;page=detailTrigger?.closest(".page")?.id||"home";drawPage();
  detailTrigger?.focus();detailTrigger=null;return;
 }
 if(action==="refresh"){poll();return}
 if(action==="wifi_forget"){
   const n=selectedProfile();
   if(wifiReady()&&n&&confirm("Забыть сеть «"+n.ssid+"» ("+(n.open?"открытая":"защищённая")+")?"))wifiWrite(action,{ssid:n.ssid,open:n.open?"1":"0"});
   return;
 }
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
 const pin=target.closest('#jp1-map button[data-gpio]');
 if(pin){openPinEditor(pin);return;}
 const p=target.closest("[data-page]");
 if(p){page=p.dataset.page;if(page==="settings")tab="";detail=-1;detailTrigger=null;drawPage();return;}
 const t=target.closest("[data-tab]");
 if(t){setTab(t.dataset.tab);return}
 const temperature=target.closest("[data-temp]");
 if(temperature){
  detailTrigger=temperature;detail=Number(temperature.dataset.temp);page="home";drawPage();
  const panel=$("temperature-detail");panel.querySelector("button").focus({preventScroll:true});
  panel.scrollIntoView({behavior:"smooth",block:"nearest"});return;
 }
 const button=target.closest("[data-action]");
 if(button){if(button.disabled)return;handleAction(button)}
});
$("pin-form").addEventListener("submit",e=>{e.preventDefault();savePin();});
$("pin-channel").addEventListener("change",changePinChannel);
for(const id of ["pin-type","pin-debounce"])$(id).addEventListener("input",()=>{if(pinEditor){pinEditor.message="";pinEditor.problem=false;renderPinEditor();}});
$("pin-close").addEventListener("click",closePinEditor);
$("pin-clear").addEventListener("click",()=>savePin(true));
$("pin-reload").addEventListener("click",reloadPinEditor);
$("pin-dialog").addEventListener("cancel",e=>{e.preventDefault();closePinEditor();});
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
$("wifi-profile").addEventListener("change",renderAP);
$("wifi-edit-password").addEventListener("click",()=>{
 const n=selectedProfile();if(!wifiReady()||!n||n.open)return;
 wifiForm(n.ssid,false);notice("Введите новый пароль и нажмите «Подключиться».");
});
$("wifi-open").addEventListener("change",e=>{
 const isOpen=e.target.checked;$("wifi-pass").disabled=isOpen;
 if(isOpen)$("wifi-pass").value="";
});
$("wifi-form").addEventListener("submit",e=>{
 e.preventDefault();
 if(!token){startPairing();return}
 if(!wifiReady()){notice("Сейчас подключение недоступно",true);return;}
 const ssid=$("ssid").value,pass=$("wifi-pass").value,open=$("wifi-open").checked;
 if(!ssid||new TextEncoder().encode(ssid).length>32){notice("SSID до 32 байт",true);return}
 if(!open&&!((pass.length>=8&&pass.length<=63)||/^[0-9a-fA-F]{64}$/.test(pass))){
   notice("Пароль WPA2/WPA3: 8–63 символа или 64 hex",true);return;
 }
 if(!confirm("Подключить панель к выбранной сети? Это может изменить её IP."))return;
 wifiWrite("wifi_connect",{ssid,password:pass,open:open?"1":"0"});
 $("wifi-pass").value="";
});
async function poll(){
 if(refreshInFlight)return;
 refreshInFlight=true;
 const serial=++pinPollStarted,requestToken=token;
 try{
   const {response:r,body}=await request("/api/status",{cache:"no-store",headers:apiHeaders()});
   if(requestToken!==token){pinPollQueued=true;return;}
   if(!r.ok)throw Error("HTTP "+r.status);
   const d=JSON.parse(body);
   if(!d.authorized&&token){token="";sessionStorage.removeItem("boiler_panel_token");if(!pendingUpdate)notice("Сессия истекла. Повторите сопряжение.");}
   reconcileUpdate(d);current=d;serverOnline=true;lastServerError="";pinPollSerial=serial;updateView();
 }catch(e){
   if(requestToken!==token){pinPollQueued=true;return;}
   if(lastServerError!==e.message)notice("Нет связи с панелью: "+e.message,true);
   lastServerError=e.message;serverOnline=false;
   if(pendingUpdate){pendingUpdate.connectionLost=true;saveUpdatePending();}
   pinPollSerial=serial;
   updateView();markOffline();
   text("net","● Нет ответа панели");
   $("net").className="status";
   text("last-update","Сервер не отвечает, повторяем");
 }finally{refreshInFlight=false;if(pinPollQueued){pinPollQueued=false;poll();}}
}
$("pair-form").addEventListener("submit",e=>{e.preventDefault();login();});
$("pairbox").addEventListener("cancel",e=>{e.preventDefault();cancelPairing();});
setInterval(()=>{if(!$("pairbox").open)return;const left=Math.max(0,Math.ceil((pairDeadline-Date.now())/1000));text("pair-expiry",pairDeadline?(left?"Осталось "+left+" с":"Срок кода истёк. Запросите новый."):"");if(!left)$("pair-submit").disabled=true;},500);
drawPage();poll();setInterval(poll,3000);
})();
</script></body></html>)BPHTML";
