"""Real Chromium, embedded equipment card and isolated panel responses (no hardware)."""
import json
import os
from pathlib import Path
from urllib.parse import parse_qsl, urlparse
from playwright.sync_api import sync_playwright, expect

root=Path(__file__).resolve().parents[1]
source=(root/"src/panel_web_page.h").read_bytes()
html=source[source.index(b"<!doctype html>"):source.index(b"</html>")+7]
out=root/"artifacts/boiler-web-review"
out.mkdir(parents=True,exist_ok=True)
version=(root/"VERSION").read_text().strip()
snap=dict(revision=0,configured=False,on_c=None,off_c=None)
electric_snap=snap.copy()
pin_items=[dict(channel=i,type="none",gpio=-1,jp1_pin=0,debounce_ms=50,address=0) for i in range(12)]
fixture={"version":version,"authorized":True,"equipment":[],"screen":{},"service":{},
         "time":{},"wifi":{"ready":True,"supported":True},"ble":{},"storage":{},
         "connections":{"available":True,"revision":0,"items":pin_items},
         "fuel":{},"temperatures":[],"home_assistant":{},"boiler_settings":snap,"electric_settings":electric_snap,
         "update":{"started":True,"supported":True,"phase":"current","busy":False,"boot_pending":False}}
post,errors=[],[]
flags={"lose_ack":False,"offline":False}
def serve(route):
    r=route.request
    url=urlparse(r.url)
    if url.hostname!="boiler.test":
        errors.append("external request");return route.abort()
    if r.method=="GET" and url.path=="/":
        return route.fulfill(content_type="text/html; charset=utf-8",body=html)
    if r.method=="GET" and url.path=="/api/status":
        return route.fulfill(status=503 if flags["offline"] else 200,json=fixture)
    if r.method=="POST" and url.path=="/api/command":
        assert r.headers.get("authorization")=="Bearer boiler-test"
        assert r.headers.get("x-boiler-pairing")=="1"
        cmd=dict(parse_qsl(r.post_data,keep_blank_values=True))
        post.append(cmd)
        target={"boiler_thresholds":snap,"electric_thresholds":electric_snap}[cmd["action"]]
        assert int(cmd["revision"])==target["revision"]
        target.update(configured=True,revision=target["revision"]+1,on_c=int(cmd["on_c"]),off_c=int(cmd["off_c"]))
        if flags["lose_ack"]:return route.abort()
        return route.fulfill(json={"ok":True,"saved":True,"revision":target["revision"]})
    if r.method!="GET":errors.append(r.method+" "+url.path)
    return route.fulfill(status=404,body="")
with sync_playwright() as p:
    browser=p.chromium.launch()
    ctx=browser.new_context(viewport={"width":320,"height":760})
    ctx.add_init_script("sessionStorage.setItem('boiler_panel_token','boiler-test')")
    ctx.route("**/*",serve)
    page=ctx.new_page()
    page.on("pageerror",lambda e: errors.append(str(e)))
    page.goto("http://boiler.test/")
    page.locator('.topnav [data-page="equipment"]').click()
    card=page.locator('#equipment-list > details[data-equipment="0"]')
    card.locator("summary").click()
    on,off,save,reset,info=[page.locator("#boiler-"+name) for name in ("on","off","save","reload","feedback")]
    enable=card.get_by_role("button",name="Включить бойлер")
    expect(enable).to_be_disabled();expect(on).to_have_value("");expect(off).to_have_value("")
    expect(save).to_be_disabled();expect(on).to_be_enabled()
    assert page.evaluate("document.documentElement.scrollWidth<=innerWidth")
    on.fill("60");off.fill("55");expect(save).to_be_disabled()
    off.fill("65");expect(save).to_be_enabled()
    electric_card=page.locator('#equipment-list > details[data-equipment="1"]')
    electric_card.locator("summary").click()
    electric_on=electric_card.get_by_label("Включать при температуре",exact=False)
    electric_off=electric_card.get_by_label("Выключать при температуре",exact=False)
    electric_save=electric_card.get_by_role("button",name="Сохранить пороги")
    electric_reload=electric_card.get_by_role("button",name="Перечитать с панели")
    electric_info=electric_card.get_by_role("status")
    expect(electric_card.locator("#electric-outputs button")).to_have_count(3)
    expect(electric_card.get_by_role("button",name="Включить ТЭНы")).to_have_count(0)
    for number in (1,2,3):
        button=electric_card.get_by_role("button",name=f"ТЭН-{number}",exact=True)
        expect(button).to_be_disabled()
        expect(button).to_have_attribute("aria-describedby",f"electric-pin-{number} electric-hardware")
        expect(electric_card.locator(f"#electric-pin-{number}")).to_have_text("Выход не назначен")
        bounds=button.bounding_box()
        assert bounds and bounds["width"]>=44 and bounds["height"]>=44 and bounds["x"]+bounds["width"]<=320
    expect(electric_on).to_have_value("");expect(electric_off).to_have_value("")
    expect(electric_card.locator("[data-circuit]")).to_have_count(3)
    electric_on.fill("75");electric_off.fill("75");expect(electric_save).to_be_disabled()
    electric_on.fill("35");expect(electric_save).to_be_enabled()
    expect(save).to_be_enabled();expect(on).to_have_value("60")
    node=off.element_handle()
    # A normal status poll with the same revision must keep the draft, details and focus.
    with page.expect_response(lambda r:r.url.endswith("/api/status")):
        off.focus()
    expect(off).to_be_focused();assert node.evaluate("el=>el.isConnected");expect(off).to_have_value("65")
    expect(card).to_have_js_property("open",True)
    expect(info).to_contain_text("Изменения не сохранены")
    page.screenshot(path=str(out/"boiler-320.png"))
    snap.update(configured=True,revision=1,on_c=10,off_c=50)
    expect(info).to_contain_text("изменились",timeout=10000);expect(save).to_be_disabled()
    expect(on).to_have_value("60");assert not post
    expect(electric_save).to_be_enabled();expect(electric_on).to_have_value("35")
    electric_save.click();expect(electric_info).to_have_text("Оба порога сохранены на панели.",timeout=10000)
    assert post==[dict(action="electric_thresholds",revision="0",on_c="35",off_c="75")]
    assert snap["on_c"]==10 and snap["off_c"]==50 and snap["revision"]==1
    expect(on).to_have_value("60");expect(save).to_be_disabled()
    # Confirm every numbered button reads its own relay slot, excluding the independent DHW relay.
    for role,gpio,jp1 in ((8,31,10),(10,34,17),(11,49,13),(9,32,19)):
        pin_items[role].update(type="relay",gpio=gpio,jp1_pin=jp1)
    fixture["connections"]["revision"]=1
    for number,gpio in ((1,31),(2,34),(3,49)):
        expect(electric_card.locator(f"#electric-pin-{number}")).to_have_text(f"GPIO {gpio}",timeout=10000)
        expect(electric_card.get_by_role("button",name=f"ТЭН-{number}",exact=True)).to_be_disabled()
    expect(electric_card).to_have_js_property("open",True)
    electric_card.scroll_into_view_if_needed()
    electric_card.screenshot(path=str(out/"electric-320.png"))
    electric_card.locator("#electric-outputs").screenshot(path=str(out/"three-heater-buttons.png"))
    assert page.evaluate("document.documentElement.scrollWidth<=innerWidth")
    reset.click();expect(on).to_have_value("10");expect(off).to_have_value("50")
    on.fill("40");off.fill("70")
    fixture["update"]["busy"]=True;expect(save).to_be_disabled(timeout=10000)
    fixture["update"]["busy"]=False;expect(save).to_be_enabled(timeout=10000)
    flags["offline"]=True;expect(save).to_be_disabled(timeout=10000)
    expect(electric_card.locator("#electric-pin-2")).to_have_text("Нет данных")
    pin_items[10].update(type="none",gpio=-1,jp1_pin=0)
    fixture["connections"]["revision"]=2
    expect(on).to_have_value("40");flags["offline"]=False;expect(save).to_be_enabled(timeout=10000)
    expect(electric_card.locator("#electric-pin-2")).to_have_text("Выход не назначен")
    expect(electric_card.locator("#electric-pin-1")).to_have_text("GPIO 31")
    expect(electric_card.locator("#electric-pin-3")).to_have_text("GPIO 49")
    save.click();expect(info).to_have_text("Оба порога сохранены на панели.",timeout=10000)
    assert len(post)==2 and post[-1]==dict(action="boiler_thresholds",revision="1",on_c="40",off_c="70")
    expect(electric_on).to_have_value("35");expect(electric_off).to_have_value("75")
    page.set_viewport_size({"width":390,"height":844})
    assert page.evaluate("document.documentElement.scrollWidth<=innerWidth")
    on.fill("42");flags["lose_ack"]=True;save.click()
    expect(info).to_contain_text("не подтверждено",timeout=10000);expect(save).to_be_disabled()
    expect(on).to_have_value("42");reset.click();expect(info).to_contain_text("Показаны сохранённые")
    expect(on).to_have_value("42");expect(save).to_be_disabled()
    fixture["boiler_settings"]={"configured":True,"revision":3,"on_c":53,"off_c":22}
    expect(info).to_have_text("Настройки бойлера недоступны.",timeout=10000)
    expect(on).to_be_disabled();expect(enable).to_be_disabled()
    expect(electric_on).to_be_enabled()
    electric_on.fill("36")
    electric_snap.update(configured=True,revision=2,on_c=32,off_c=72)
    expect(electric_info).to_contain_text("изменились",timeout=10000);expect(electric_save).to_be_disabled()
    expect(electric_on).to_have_value("36");electric_reload.click()
    expect(electric_on).to_have_value("32");expect(electric_off).to_have_value("72")
    fixture["authorized"]=False;fixture["boiler_settings"]=snap
    expect(info).to_contain_text("сопряжение",timeout=10000);expect(on).to_be_disabled()
    expect(electric_info).to_contain_text("сопряжение");expect(electric_on).to_be_disabled()
    assert len(post)==3 and not errors,(post,errors)
    browser.close()
(out/"report.json").write_text(json.dumps(dict(version=version,commit=os.getenv("GITHUB_SHA"),
 fixture_posts=len(post),hardware_contacted=False,widths=[320,390]),indent=2))
print("PASS: independent DHW and electric boiler cards, scoped save and readback, three heater controls with independent GPIO bindings, three circuits and no heat command, draft/poll, strict ordering, stale revision, unavailable heat, OTA/offline/auth guards, explicit save/readback, lost ACK without replay")
