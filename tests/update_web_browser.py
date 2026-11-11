"""Actual embedded updater in Chromium, isolated HTTP fixtures, no device access.

Run with the same Playwright environment as tests/web_v090_browser.py.
BP_BROWSER may select an already installed Chromium/Chrome executable.
"""
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
from urllib.parse import parse_qs
import copy
import json
import os
from playwright.sync_api import sync_playwright, expect

ROOT = Path(__file__).resolve().parents[1]
BASE_VERSION = (ROOT / "VERSION").read_text(encoding="ascii").strip()
SOURCE = (ROOT / "src/panel_web_page.h").read_text(encoding="utf-8")
HTML = SOURCE[SOURCE.index("<!doctype html>"):SOURCE.index("</html>") + 7].encode()
OUT = ROOT / "artifacts/update-web-review"
OUT.mkdir(parents=True, exist_ok=True)
update = {
    "started": True, "supported": True, "busy": False, "available": True,
    "cancel_allowed": False, "rollback_available": False, "boot_pending": False,
    "automatic_rollback": True, "phase": "available", "done": 0, "total": 0,
    "percent": 0, "last_check": 1000, "latest_version": "v9.8.1",
    "previous_version": "", "message": "Доступна новая версия панели.", "error_code": 0,
}
fixture = {
    "version": BASE_VERSION, "clock": "05.11.2026 12:40:00", "uptime_s": 3600,
    "physical_control": False, "data_origin": "fixture",
    "wifi": {"connected": True, "ready": True, "busy": False, "supported": True,
             "has_saved": True, "ssid": "Fixture-WiFi", "ip": "192.0.2.10", "aps": []},
    "ble": {"supported": True, "devices": []},
    "screen": {"timeout_s": 60, "touch_ready": True},
    "time": {"zone": 0, "zone_name": "UTC"},
    "service": {"free_heap": 180000, "first_frame_ms": 480, "display_ms": 320,
                "lvgl_ms": 2, "touch_ms": 80},
    "temperatures": [{"valid": False, "ever": False, "history": []} for _ in range(5)],
    "fuel": {"lower": {"valid": False}, "upper": {"valid": False}},
    "fuel_level_text": "Нет данных", "equipment": [],
    "connections": {"revision": 0, "drivers_active": False, "items": []},
    "storage": {"started": True, "mounted": True, "logging": True,
                "format_attempted": False, "format_state": 0},
    "home_assistant": {"ready": True, "supported": True, "enabled": False,
                       "connected": False, "revision": 0, "port": 1883},
    "update": update,
}
state = {"token": "", "ticket": "", "pair_active": False, "offline": False,
         "commands": [], "rejected": [], "pairings": 0}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def reply(self, status, value, mime="application/json"):
        data = value if isinstance(value, bytes) else json.dumps(value, ensure_ascii=False).encode()
        self.send_response(status)
        self.send_header("Content-Type", mime + "; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path == "/":
            return self.reply(200, HTML, "text/html")
        if self.path == "/api/status":
            if state["offline"]:
                return self.reply(503, {"error": "fixture_offline"})
            data = copy.deepcopy(fixture)
            data["authorized"] = bool(state["token"]) and self.headers.get("Authorization") == "Bearer " + state["token"]
            data["pair_available"] = state["pair_active"]
            data["pair_remaining_s"] = 120 if state["pair_active"] else 0
            return self.reply(200, data)
        self.reply(404, {})

    def do_POST(self):
        data = parse_qs(self.rfile.read(int(self.headers.get("Content-Length", "0"))).decode(), keep_blank_values=True)
        values = {key: value[0] for key, value in data.items()}
        get = lambda key: values.get(key, "")
        if self.path.startswith("/api/pair") and self.headers.get("X-Boiler-Pairing") != "1":
            return self.reply(403, {"error": "same_origin_required"})
        if self.path == "/api/pair/start":
            state["ticket"] = get("request_id")
            assert len(state["ticket"]) == 32
            state["pair_active"] = True
            return self.reply(202, {"ok": True, "display_only": True, "expires_in_s": 120})
        if self.path == "/api/pair/cancel":
            state["pair_active"] = False
            return self.reply(200, {"ok": True})
        if self.path == "/api/pair":
            if not state["pair_active"] or get("request_id") != state["ticket"] or get("code") != "123456":
                return self.reply(403, {"error": "code_invalid_or_expired"})
            state["token"] = "f" * 32
            state["pair_active"] = False
            state["pairings"] += 1
            return self.reply(200, {"ok": True, "token": state["token"], "expires_in_s": 3600})
        if self.path == "/api/command":
            if not state["token"] or self.headers.get("Authorization") != "Bearer " + state["token"]:
                return self.reply(401, {"error": "pair_required"})
            if self.headers.get("X-Boiler-Pairing") != "1" or self.headers.get("Origin") != base:
                return self.reply(403, {"error": "same_origin_required"})
            action = get("action")
            assert action in ("update_check", "update_install", "update_cancel", "update_rollback"), values
            if action == "update_cancel":
                if not update["busy"] or not update["cancel_allowed"]:
                    return self.reply(409, {"error": "update_cancel_unavailable"})
                update.update(busy=False, cancel_allowed=False, phase="cancelled", message="Обновление отменено.")
            elif update["busy"] or update["boot_pending"]:
                return self.reply(409, {"error": "update_busy"})
            elif action == "update_check":
                update.update(busy=True, phase="checking", message="Проверяем новые версии.")
            else:
                expected = update["previous_version"] if action == "update_rollback" else update["latest_version"]
                if not get("version") or get("confirm") != get("version"):
                    return self.reply(400, {"error": "update_confirmation_required"})
                if get("version") != expected:
                    state["rejected"].append(values)
                    return self.reply(409, {"error": "update_version_changed"})
                update.update(busy=True, cancel_allowed=action == "update_install",
                              phase="downloading" if action == "update_install" else "rebooting",
                              done=1048576, total=10485760, percent=10, error_code=0,
                              message="Загружаем новую версию." if action == "update_install" else "Возвращаем предыдущую версию.")
            state["commands"].append(values)
            return self.reply(202, {"ok": True, "queued": True})
        self.reply(404, {})


server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
Thread(target=server.serve_forever, daemon=True).start()
base = f"http://127.0.0.1:{server.server_port}"
try:
    with sync_playwright() as playwright:
        executable = os.environ.get("BP_BROWSER")
        if not executable and os.name == "nt":
            executable = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
        browser = playwright.chromium.launch(headless=True, executable_path=executable)
        context = browser.new_context(viewport={"width": 1280, "height": 900}, reduced_motion="reduce")
        page = context.new_page()
        errors = []
        page.on("pageerror", lambda error: errors.append(str(error)))

        def paired():
            page.click("#pairbutton")
            expect(page.locator("#pair-submit")).to_be_enabled()
            assert "123456" not in page.content()
            page.fill("#paircode", "123456")
            page.click("#pair-submit")
            expect(page.locator("#pairbox")).not_to_be_visible()
            expect(page.locator("#logout")).to_be_visible()

        def accept_version(selector, version, mutate=None):
            def answer(dialog):
                assert version in dialog.message
                if mutate:
                    mutate()
                dialog.accept()
            page.once("dialog", answer)
            page.click(selector)

        page.goto(base)
        expect(page.locator("#update-offer")).to_be_visible()
        assert page.locator(".topnav button").count() == 4
        page.click("#update-offer [data-page=update]")
        expect(page.locator("#update-current")).to_have_text(BASE_VERSION)
        expect(page.locator("#update-result")).not_to_be_visible()
        expect(page.locator("#update-install")).to_be_disabled()
        expect(page.locator("#update-check")).to_be_disabled()
        assert not state["commands"]
        paired()
        page.click(".topnav [data-page=settings]")
        page.click("#settings-menu [data-page=service]")
        page.click("#service [data-page=update]")
        expect(page.locator("#update-latest")).to_have_text("v9.8.1")

        # A rejected native browser confirmation sends no mutation.
        page.once("dialog", lambda dialog: dialog.dismiss())
        page.click("#update-install")
        assert not state["commands"]
        assert page.evaluate("sessionStorage.getItem('boiler_panel_update')") is None

        # A release change while the confirmation is open must not silently install it.
        accept_version("#update-install", "v9.8.1", lambda: update.update(latest_version="v9.8.2"))
        expect(page.locator("#update-result")).to_contain_text("Запрос не принят")
        assert state["rejected"][-1] == {"action": "update_install", "version": "v9.8.1", "confirm": "v9.8.1"}
        assert not state["commands"]
        expect(page.locator("#update-latest")).to_have_text("v9.8.2", timeout=6000)

        # Keyboard activation sends a check only; an up-to-date response has no install.
        page.locator("#update-check").focus()
        page.keyboard.press("Enter")
        expect(page.locator("#update-phase")).to_have_text("Проверка обновлений", timeout=6000)
        expect(page.locator("#update-check")).to_be_disabled()
        update.update(busy=False, phase="current", available=False, message="Установлена последняя версия.")
        expect(page.locator("#update-phase")).to_have_text("Версия актуальна", timeout=6000)
        expect(page.locator("#update-result")).not_to_be_visible()
        expect(page.locator("#update-install")).to_be_disabled()
        update.update(phase="available", available=True, message="Доступна новая версия.")
        expect(page.locator("#update-install")).to_be_enabled(timeout=6000)

        accept_version("#update-install", "v9.8.2")
        expect(page.locator("#update-progress-text")).to_contain_text("10%", timeout=6000)
        assert state["commands"][-1] == {"action": "update_install", "version": "v9.8.2", "confirm": "v9.8.2"}
        assert "1\u00a0048\u00a0576" in page.locator("#update-progress-text").inner_text()
        expect(page.locator("#update-check")).to_be_disabled()
        expect(page.locator("#update-install")).to_be_disabled()
        page.screenshot(path=str(OUT / "update-progress-desktop.png"), full_page=True)
        page.click(".topnav [data-page=settings]")
        page.click("#settings-menu [data-tab=wifi]")
        expect(page.locator('[data-action="wifi_scan"]')).to_be_disabled()
        expect(page.locator("#wifi-form button[type=submit]")).to_be_disabled()
        page.click("#settings-back [data-action=settings-root]")
        page.click("#settings-menu [data-page=ha]")
        expect(page.locator("#ha-host")).to_be_disabled()
        page.click("#ha [data-action=settings-root]")
        page.click("#settings-menu [data-page=service]")
        expect(page.locator("#sd-format-button")).to_be_disabled()
        page.click("#service [data-page=update]")
        page.click("#update-cancel")
        expect(page.locator("#update-phase")).to_have_text("Обновление отменено", timeout=6000)
        assert state["commands"][-1] == {"action": "update_cancel"}
        expect(page.locator("#update-install")).to_be_enabled()

        accept_version("#update-install", "v9.8.2")
        expect(page.locator("#update-cancel")).to_be_enabled(timeout=6000)
        update.update(phase="verifying", cancel_allowed=False, done=10485760, percent=100)
        expect(page.locator("#update-phase")).to_have_text("Проверка файла", timeout=6000)
        expect(page.locator("#update-cancel")).to_be_disabled()
        count = len(state["commands"])
        box = page.locator("#update-cancel").bounding_box()
        page.mouse.click(box["x"] + box["width"] / 2, box["y"] + box["height"] / 2)
        assert len(state["commands"]) == count
        update.update(phase="rebooting", message="Перезапуск панели.")
        expect(page.locator("#update-phase")).to_have_text("Перезапуск панели", timeout=6000)
        state["offline"] = True
        expect(page.locator("#update-result")).to_contain_text("Ждём её возвращения", timeout=12000)
        expect(page.locator("#update-install")).to_be_disabled()
        assert json.loads(page.evaluate("sessionStorage.getItem('boiler_panel_update')"))["target"] == "v9.8.2"
        page.screenshot(path=str(OUT / "update-reconnect.png"), full_page=True)

        # Reload the page after a firmware restart. Session expired, monitoring continues.
        fixture["version"] = "v9.8.2"
        fixture["uptime_s"] = 3
        state["offline"] = False
        state["token"] = ""
        update.update(boot_pending=True, phase="installing", available=False, previous_version=BASE_VERSION,
                      rollback_available=True, message="Проверяем запуск новой версии.")
        page.reload()
        page.click(".topnav [data-page=settings]")
        page.click("#settings-menu [data-page=service]")
        page.click("#service [data-page=update]")
        expect(page.locator("#update-current")).to_have_text("v9.8.2")
        expect(page.locator("#update-phase")).to_have_text("Проверка запуска")
        expect(page.locator("#update-result")).to_contain_text("Идёт проверка запуска")
        update.update(boot_pending=False, busy=False, phase="success", message="Новая версия работает.")
        expect(page.locator("#update-result")).to_contain_text("Установлена v9.8.2", timeout=6000)
        assert "возврат" not in page.locator("#update-result").inner_text().lower()
        assert page.evaluate("sessionStorage.getItem('boiler_panel_update')") is None
        assert page.evaluate("sessionStorage.getItem('boiler_panel_token')") is None
        assert len(state["commands"]) == count

        # Manual rollback has its own pinned confirmation and a new pairing.
        paired()
        page.click("#update-rollback-box summary")
        accept_version("#update-rollback", BASE_VERSION)
        expect(page.locator("#update-phase")).to_have_text("Перезапуск панели", timeout=6000)
        assert state["commands"][-1] == {"action": "update_rollback", "version": BASE_VERSION, "confirm": BASE_VERSION}
        state["offline"] = True
        expect(page.locator("#update-result")).to_contain_text("Ждём её возвращения", timeout=12000)
        fixture["version"] = BASE_VERSION
        state["offline"] = False
        update.update(busy=False, phase="rolled_back", message="Возврат завершён.", available=False)
        expect(page.locator("#update-result")).to_contain_text("Возврат завершён", timeout=6000)
        expect(page.locator("#update-current")).to_have_text(BASE_VERSION)
        update.update(automatic_rollback=True, message="Восстановлена рабочая версия.")
        expect(page.locator("#update-result")).to_contain_text("Возврат завершён", timeout=6000)
        assert "не прошла" not in page.locator("#update-result").inner_text()
        update.update(automatic_rollback=False, phase="error", error_code=21,
                      message="Не удалось проверить файл обновления. Установленная версия сохранена.")
        expect(page.locator("#update-error")).to_have_text("Код ошибки: 21", timeout=6000)
        for width in (320, 390, 768, 1280):
            page.set_viewport_size({"width": width, "height": 900})
            assert page.evaluate("document.documentElement.scrollWidth <= innerWidth"), width
            for selector in ("#update-check", "#update-install"):
                box = page.locator(selector).bounding_box()
                assert box["height"] >= 44, (width, selector, box)
            if width == 390:
                page.screenshot(path=str(OUT / "update-error-mobile.png"), full_page=True)
        assert not errors, errors
        result = {
            "result": "PASS", "page_errors": errors, "widths": [320, 390, 768, 1280],
            "pairings": state["pairings"], "accepted_commands": state["commands"],
            "rejected_pinned_commands": state["rejected"], "physical_panel_contacted": False,
            "coverage": ["real clicks and keyboard", "explicit pinned confirmation", "busy guards",
                         "cancel availability", "bytes and progress", "restart and expired session",
                         "reload persistence", "successful reconnect", "manual and automatic rollback"],
        }
        (OUT / "browser-result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, ensure_ascii=False))
        browser.close()
finally:
    server.shutdown()
