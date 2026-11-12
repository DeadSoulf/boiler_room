"""Exercise the heating page in real Chromium with isolated panel responses."""
import json
import os
from pathlib import Path
from urllib.parse import parse_qsl, urlparse

from playwright.sync_api import expect, sync_playwright

root = Path(__file__).resolve().parents[1]
source = (root / "src/panel_web_page.h").read_bytes()
html = source[source.index(b"<!doctype html>"):source.index(b"</html>") + 7]
out = root / "artifacts/heating-web-review"
out.mkdir(parents=True, exist_ok=True)
fields = ("room_on_c", "room_radiator_off_c", "room_off_c", "supply_target_c", "return_target_c",
          "return_restart_c", "tank_on_c", "tank_off_c", "coil_start_delta_c", "coil_stop_delta_c")
settings = dict.fromkeys(fields)
settings.update(room_on_c=19, room_radiator_off_c=22, room_off_c=23, rooms=[0] * 14, season=0,
                reheat=0, reheat_step_s=0, away_diesel=False, revision=0)
rooms_c = [18.5] + [None] * 13
calc = dict(presence=1, circulation=2, source=2, demand=2, alarm=1, diesel=1, electric=1,
            plant_heat=2, tank_power=2, coil_pump=0, stages=None, supply_c=52.5, return_c=41.5,
            tank_c=None, coil_supply_c=None, room_c=rooms_c, loops=[2] + [0] * 13)
status = {"authorized": True, "update": {"started": True, "busy": False, "boot_pending": False},
          "time": {}, "wifi": {}, "ble": {}, "screen": {}, "fuel": {}, "service": {}, "storage": {},
          "equipment": [], "temperatures": []}
calls, page_errors = [], []
state = {"offline": False, "drop_ack": False, "calculation": None}


def serve(route):
    request = route.request
    path = urlparse(request.url).path
    if path == "/":
        return route.fulfill(content_type="text/html; charset=utf-8", body=html)
    if path == "/api/status":
        return route.fulfill(status=503 if state["offline"] else 200, json=status)
    if path == "/api/heating":
        if not status["authorized"] or request.headers.get("authorization") != "Bearer heating-fixture":
            return route.fulfill(status=401, json={"error": "pair_required"})
        return route.fulfill(json={"settings": settings, "calculation": state["calculation"]})
    if path == "/api/command":
        form = dict(parse_qsl(request.post_data, keep_blank_values=True))
        assert request.headers.get("x-boiler-pairing") == "1" and status["authorized"]
        assert request.headers.get("authorization") == "Bearer heating-fixture"
        assert form["action"] == "heating_settings" and int(form["revision"]) == settings["revision"]
        calls.append(form)
        settings.update({key: float(form[key]) if form[key] else None for key in fields})
        settings.update(season=int(form["season"]), rooms=list(map(int, form["rooms"].split(","))),
                        reheat=int(form["reheat"]), reheat_step_s=int(form["reheat_step_s"]),
                        away_diesel=form["away_diesel"] == "1", revision=settings["revision"] + 1)
        if state["drop_ack"]:
            return route.abort()
        return route.fulfill(json={"ok": True, "saved": True, "revision": settings["revision"]})
    return route.fulfill(status=404, body="")


with sync_playwright() as pw:
    browser = pw.chromium.launch(headless=True, executable_path=os.getenv("BP_BROWSER"))
    context = browser.new_context(viewport={"width": 390, "height": 844})
    context.add_init_script("sessionStorage.setItem('boiler_panel_token','heating-fixture')")
    context.route("**/*", serve)
    page = context.new_page()
    page.on("pageerror", lambda error: page_errors.append(str(error)))
    page.goto("http://panel.test/")
    outdoor = page.locator(".card.temp").filter(has=page.locator("#t1"))
    expect(outdoor).to_have_count(1)
    assert outdoor.evaluate("(node) => node.tagName === 'DIV' && !node.hasAttribute('data-temp')")
    expect(outdoor).to_contain_text("На улице")
    outdoor.click()
    expect(page.locator("#temperature-target")).not_to_be_visible()
    page.locator('[data-temp="2"]').click()
    expect(page.locator("#temperature-target")).to_be_visible()
    page.locator("#target-close").click()
    assert not calls  # Reading outdoor weather must not initiate a settings write.
    page.get_by_role("button", name="Оборудование", exact=True).click()
    entry = page.get_by_role("button", name="Автоматика отопления", exact=True)
    entry.focus()
    page.keyboard.press("Enter")
    expect(page.locator("#heat-status")).to_have_text("Текущий расчёт недоступен.")
    page.locator("#heat-config summary").first.click()
    expect(page.locator("#heat-room_on_c")).to_have_value("19")
    target = page.get_by_role("slider", name="Температура в доме")
    expect(target).to_have_value("22")
    expect(target).to_have_attribute("min", "-52")
    expect(target).to_have_attribute("max", "124")
    target.press("ArrowRight")
    for key, value in zip(fields[:3], ["19.1", "22.1", "23.1"]):
        expect(page.locator("#heat-" + key)).to_have_value(value)
    target.press("End")
    expect(page.locator("#heat-room_off_c")).to_have_value("125")
    expect(page.locator("#heat-warmer")).to_be_disabled()
    for key, value in zip(fields[:3], ["19", "22", "23"]):
        page.locator("#heat-" + key).fill(value)
    expect(target).to_have_value("22")
    page.locator("#heat-warmer").click()
    expect(page.locator("#heat-room_on_c")).to_have_value("20")
    expect(page.locator("#heat-room_radiator_off_c")).to_have_value("23")
    expect(page.locator("#heat-room_off_c")).to_have_value("24")
    assert not calls
    page.locator("#heat-colder").click()
    expect(page.locator("#heat-room_off_c")).to_have_value("23")
    page.locator("#heat-warmer").click()
    page.locator("#heat-season").select_option("2")
    page.locator("#heat-config .details summary").click()
    expect(page.locator("#heat-loops select")).to_have_count(14)
    page.locator("#heat-loop-0").select_option("1")
    page.locator("#heat-loop-8").select_option("1")
    page.locator("#heat-loop-13").select_option("14")
    page.locator("#heat-supply_target_c").fill("60.5")
    page.locator("#heat-return_target_c").fill("48")
    page.locator("#heat-reheat").select_option("2")
    page.locator("#heat-reheat-seconds").fill("300")
    save = page.locator("#heat-save")
    save.click()
    expect(page.locator("#heat-feedback")).to_have_text("Настройки подтверждены панелью.")
    assert calls[-1]["rooms"].split(",")[13] == "14" and calls[-1]["reheat_step_s"] == "300"
    assert [settings[field] for field in fields[:3]] == [20, 23, 24]
    state["calculation"] = calc
    expect(page.locator("#heat-readings")).to_contain_text("Помещение 14", timeout=10000)
    expect(page.locator("#heat-readings")).to_contain_text("Запрос открытия")
    expect(page.locator("#heat-readings")).to_contain_text("Нет данных")
    calc.update(source=1, demand=2)
    expect(page.locator("#heat-readings")).to_contain_text("Нет источника", timeout=10000)
    expect(page.locator("#heat-readings")).to_contain_text("Нужен обогрев")
    page.locator("#heat-config summary").first.click()
    page.locator("#heating").screenshot(path=str(out / "heating-requests.png"))
    assert page.evaluate("document.documentElement.scrollWidth<=innerWidth")
    page.locator("#heat-config summary").first.click()
    page.locator("#heat-return_target_c").fill("51")
    settings.update(return_target_c=49, revision=settings["revision"] + 1)
    expect(page.locator("#heat-feedback")).to_contain_text("другом экране", timeout=10000)
    expect(save).to_be_disabled()
    expect(page.locator("#heat-return_target_c")).to_have_value("51")
    page.locator("#heat-reload").click()
    expect(page.locator("#heat-return_target_c")).to_have_value("49")
    page.locator("#heat-return_target_c").fill("52")
    status["update"]["busy"] = True
    expect(save).to_be_disabled(timeout=10000)
    status["update"]["busy"] = False
    expect(save).to_be_enabled(timeout=10000)
    state["drop_ack"] = True
    save.click()
    expect(page.locator("#heat-feedback")).to_contain_text("не подтверждена", timeout=10000)
    expect(save).to_be_disabled()
    page.locator("#heat-reload").click()
    expect(page.locator("#heat-return_target_c")).to_have_value("52")
    page.get_by_role("button", name="Главная", exact=True).click()
    page.locator("#home-heating").press("Enter")
    expect(page.locator("#temperature-target")).not_to_be_visible()
    expect(target).to_be_visible()
    expect(target).to_be_focused()
    expect(target).to_have_value("23")
    expect(page.locator("#heat-room_off_c")).to_have_value("24")
    page.locator("#heat-legacy-house").click()
    expect(page.locator("#target-heating-link")).to_contain_text("Пороги управления")
    page.locator("#target-heating-open").click()
    expect(page.locator("#temperature-target")).not_to_be_visible()
    expect(target).to_be_focused()
    assert len(calls) == 2
    state["offline"] = True
    expect(page.locator("#heat-status")).to_contain_text("Нет связи", timeout=10000)
    expect(page.locator("#heat-readings")).to_be_empty()
    state["offline"] = False
    status["authorized"] = False
    expect(page.locator("#heat-pair")).to_be_visible(timeout=10000)
    expect(save).to_be_disabled()
    assert not page_errors, page_errors
    browser.close()
(out / "result.json").write_text(json.dumps({"writes": len(calls), "hardware_contacted": False}))
print("PASS: paired heating calculations, all 14 loop mappings, nullable sources, config save/readback, conflicts/OTA/offline, lost ACK without retry")
