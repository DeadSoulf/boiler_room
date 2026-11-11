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
fixture={"version":version,"authorized":True,"equipment":[],"screen":{},"service":{},
         "time":{},"wifi":{"ready":True,"supported":True},"ble":{},"storage":{},
         "fuel":{},"temperatures":[],"home_assistant":{},"boiler_settings":snap,
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
        assert cmd["action"]=="boiler_thresholds" and int(cmd["revision"])==snap["revision"]
        snap.update(configured=True,revision=snap["revision"]+1,on_c=int(cmd["on_c"]),off_c=int(cmd["off_c"]))
        if flags["lose_ack"]:return route.abort()
        return route.fulfill(json={"ok":True,"saved":True,"revision":snap["revision"]})
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
    node=off.element_handle()
    # A normal status poll with the same revision must keep the draft, details and focus.
    with page.expect_response(lambda r:r.url.endswith("/api/status")):
        off.focus()
    expect(off).to_be_focused();assert node.evaluate("el=>el.isConnected");expect(off).to_have_value("65")
    expect(card).to_have_js_property("open",True)
    page.screenshot(path=str(out/"boiler-320.png"))
    snap.update(configured=True,revision=1,on_c=10,off_c=50)
    expect(info).to_contain_text("изменились",timeout=10000);expect(save).to_be_disabled()
    expect(on).to_have_value("60");assert not post
    reset.click();expect(on).to_have_value("10");expect(off).to_have_value("50")
    on.fill("40");off.fill("70")
    fixture["update"]["busy"]=True;expect(save).to_be_disabled(timeout=10000)
    fixture["update"]["busy"]=False;expect(save).to_be_enabled(timeout=10000)
    flags["offline"]=True;expect(save).to_be_disabled(timeout=10000)
    expect(on).to_have_value("40");flags["offline"]=False;expect(save).to_be_enabled(timeout=10000)
    save.click();expect(info).to_have_text("Оба порога сохранены на панели.",timeout=10000)
    assert post==[dict(action="boiler_thresholds",revision="1",on_c="40",off_c="70")]
    page.set_viewport_size({"width":390,"height":844})
    assert page.evaluate("document.documentElement.scrollWidth<=innerWidth")
    on.fill("42");flags["lose_ack"]=True;save.click()
    expect(info).to_contain_text("не подтверждено",timeout=10000);expect(save).to_be_disabled()
    expect(on).to_have_value("42");reset.click();expect(info).to_contain_text("Показаны сохранённые")
    expect(on).to_have_value("42");expect(save).to_be_disabled()
    fixture["boiler_settings"]={"configured":True,"revision":3,"on_c":53,"off_c":22}
    expect(info).to_have_text("Настройки бойлера недоступны.",timeout=10000)
    expect(on).to_be_disabled();expect(enable).to_be_disabled()
    fixture["authorized"]=False;fixture["boiler_settings"]=snap
    expect(info).to_contain_text("сопряжение",timeout=10000);expect(on).to_be_disabled()
    assert len(post)==2 and not errors,(post,errors)
    browser.close()
(out/"report.json").write_text(json.dumps(dict(version=version,commit=os.getenv("GITHUB_SHA"),
 fixture_posts=2,hardware_contacted=False,widths=[320,390]),indent=2))
print("PASS: real boiler card, draft/poll, strict ordering, stale revision, unavailable heat, OTA/offline/auth guards, explicit save/readback, lost ACK without replay")
