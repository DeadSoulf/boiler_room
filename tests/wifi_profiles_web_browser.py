"""Use Chromium to exercise real Wi-Fi controls with isolated fixture responses."""
import copy
import hashlib
import json
import os
from pathlib import Path
from urllib.parse import parse_qsl, urlparse

from playwright.sync_api import expect, sync_playwright

root = Path(__file__).resolve().parents[1]
source = (root / "src/panel_web_page.h").read_bytes()
html = source[source.index(b"<!doctype html>"):source.index(b"</html>") + 7]
version = (root / "VERSION").read_text().strip()
out = root / "artifacts/wifi-web-review"
out.mkdir(parents=True, exist_ok=True)
token = "fixture-only-wifi-browser"
known = "Котельная & <A>"
aps = [
    {"ssid": known, "auth": 3, "rssi": -77, "supported": True, "saved": True},
    {"ssid": known, "auth": 0, "rssi": -48, "supported": True, "saved": False},
    {"ssid": "New protected", "auth": 3, "rssi": -50, "supported": True, "saved": False},
    {"ssid": "Unsupported", "auth": 5, "rssi": -74, "supported": False, "saved": False},
]
fixture = {
    "authorized": True, "version": version, "clock": "11.11.2026 12:45:00",
    "service": {}, "screen": {}, "time": {}, "ble": {}, "storage": {}, "equipment": [],
    "temperatures": [], "fuel": {"lower": {}, "upper": {}}, "home_assistant": {},
    "wifi": {"connected": True, "ready": True, "supported": True, "busy": False,
             "aps": aps, "has_saved": True, "savedNetworks": [
                 {"ssid": known, "open": False}, {"ssid": "Absent profile", "open": True}]},
    "update": {"started": True, "supported": True, "busy": False, "phase": "current"},
}
commands, unexpected, errors = [], [], []
def serve(route):
    req = route.request
    url = urlparse(req.url)
    if url.hostname != "wifi.test":
        unexpected.append(req.url)
        return route.abort()
    if req.method == "GET" and url.path == "/":
        return route.fulfill(content_type="text/html; charset=utf-8", body=html)
    if req.method == "GET" and url.path == "/api/status":
        status = copy.deepcopy(fixture)
        if not status["authorized"]:
            status["wifi"].update(aps=[], savedNetworks=[])
        return route.fulfill(json=status)
    if req.method == "POST" and url.path == "/api/command":
        assert req.headers.get("authorization") == "Bearer " + token
        assert req.headers.get("x-boiler-pairing") == "1"
        commands.append(dict(parse_qsl(req.post_data, keep_blank_values=True)))
        return route.fulfill(status=202, json={"ok": True, "queued": True})
    if req.method != "GET":
        unexpected.append(req.method + " " + url.path)
    return route.fulfill(status=404, body="")

with sync_playwright() as p:
    browser = p.chromium.launch()
    ctx = browser.new_context(viewport={"width": 390, "height": 844})
    ctx.add_init_script("sessionStorage.setItem('boiler_panel_token', " + json.dumps(token) + ")")
    ctx.route("**/*", serve)
    page = ctx.new_page()
    page.on("pageerror", lambda err: errors.append(str(err)))
    page.goto("http://wifi.test/")
    page.locator('.topnav [data-page="settings"]').click()
    page.locator('#settings-menu [data-tab="wifi"]').click()
    rows = page.locator("#ap-list button")
    saved = page.locator('#ap-list button[data-saved="1"]')
    expect(rows).to_have_count(4)
    expect(saved).to_have_count(1)
    expect(saved).to_contain_text(known)
    expect(saved).to_contain_text("Сохранена", ignore_case=True)
    expect(rows.filter(has_text="Unsupported")).to_be_disabled()
    page.screenshot(path=str(out / "wifi-390.png"))
    with page.expect_response(lambda res: res.request.method == "POST" and res.url.endswith("/api/command")):
        saved.click()
    assert commands == [{"action": "wifi_connect", "ssid": known, "open": "0", "saved": "1"}], commands
    # An open AP sharing the saved SSID must never reuse the protected profile.
    rows.filter(has_text=known).locator('xpath=self::*[not(@data-saved="1")]').click()
    expect(page.locator("#wifi-open")).to_be_checked()
    rows.filter(has_text="New protected").click()
    expect(page.locator("#ssid")).to_have_value("New protected")
    expect(page.locator("#wifi-open")).not_to_be_checked()
    expect(page.locator("#wifi-pass")).to_have_value("")
    assert len(commands) == 1
    fixture["update"]["busy"] = True
    expect(saved).to_be_disabled(timeout=12000)
    fixture["update"]["busy"] = False
    expect(saved).to_be_enabled(timeout=12000)
    # Remove a profile that isn't in the scan. A cancelled confirmation writes nothing.
    profile = page.locator("#wifi-profile")
    details = profile.locator("xpath=ancestor::details[1]")
    if details.count() and not details.evaluate("(el)=>el.open"):
        details.locator("summary").click()
    values = profile.locator("option").evaluate_all("(xs)=>xs.filter(x=>x.textContent.includes('Absent profile')).map(x=>x.value)")
    assert len(values) == 1
    profile.select_option(value=values[0])
    forget = page.locator('[data-action="wifi_forget"]')
    page.once("dialog", lambda dialog: dialog.dismiss())
    forget.click()
    assert len(commands) == 1
    page.once("dialog", lambda dialog: dialog.accept())
    with page.expect_response(lambda res: res.request.method == "POST"):
        forget.click()
    assert commands[-1] == {"action": "wifi_forget", "ssid": "Absent profile", "open": "1"}
    fixture["authorized"] = False
    expect(rows).to_have_count(0, timeout=12000)
    expect(profile.locator("option").filter(has_text=known)).to_have_count(0)
    assert len(commands) == 2 and not unexpected and not errors, (unexpected, errors)
    browser.close()
report = {"version": version, "git_sha": os.getenv("GITHUB_SHA"), "header_sha256": hashlib.sha256(source).hexdigest(),
          "fixture_commands": [c["action"] for c in commands], "physical_panel_contacted": False}
(out / "report.json").write_text(json.dumps(report, indent=2))
print("PASS: Wi-Fi saved/open identity, one-tap login, scoped forget, OTA lock and unpaired status", report)
