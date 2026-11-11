"""Per-pin menus and assignment requests in the actual embedded HTML/Chromium.

Only this test's isolated HTTP fixture receives POST requests. The real C++
configuration/HTTP adapter has separate host tests; this suite verifies the
browser contract, dialogs, draft preservation and confirmed status handling.
It never contacts a physical panel, serial port, GPIO or external server.
"""
from argparse import ArgumentParser
from copy import deepcopy
from hashlib import sha256
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from threading import Event, Lock, Thread
from urllib.parse import parse_qs
import json
import os
import re

from playwright.sync_api import sync_playwright, expect

ROOT = Path(__file__).resolve().parents[1]
parser = ArgumentParser(description=__doc__)
parser.add_argument("--source-header", type=Path, default=ROOT / "src/panel_web_page.h")
parser.add_argument("--output-dir", type=Path, default=ROOT / "artifacts/pin-actions-web-review")
args = parser.parse_args()
header_path = args.source_header.resolve()
header_bytes = header_path.read_bytes()
source = header_bytes.decode("utf-8")
html = source[source.index("<!doctype html>"):source.index("</html>") + 7].encode("utf-8")
profile_bytes = (ROOT / "src/panel_pin_profile.h").read_bytes()
profile = re.search(r"constexpr Pin HEADER\[\]=\{(.*?)\};", profile_bytes.decode("utf-8"), re.S)
pins = {int(gpio): int(pin) for gpio, pin in re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*\}", profile[1])}
assert len(pins) == 11
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
version = (ROOT / "VERSION").read_text(encoding="ascii").strip()
FIXTURE_TOKEN = "isolated-pin-actions-fixture"


def empty_item(channel):
    return {"channel": channel, "type": "none", "gpio": -1, "jp1_pin": 0,
            "address": 0, "debounce_ms": 50, "active": False, "online": False}


def local_item(channel, kind, gpio, debounce=50):
    return {"channel": channel, "type": kind, "gpio": gpio, "jp1_pin": pins[gpio],
            "address": 0, "debounce_ms": debounce, "active": False, "online": False,
            "status": "Assigned" if kind == "relay" else "Sensor not found"}


def empty_connections(revision):
    return {"available": True, "revision": revision, "drivers_active": False,
            "items": [empty_item(channel) for channel in range(12)]}


fixture = {
    "version": version, "clock": "06.11.2026 12:40:00", "uptime_s": 1000,
    "authorized": True, "pair_available": False, "pair_remaining_s": 0,
    "physical_control": False, "data_origin": "isolated_fixture",
    "wifi": {"connected": True, "ready": True, "busy": False, "supported": True,
             "has_saved": False, "ip": "192.0.2.10", "aps": []},
    "ble": {"supported": True, "devices": []},
    "screen": {"timeout_s": 60, "touch_ready": True},
    "time": {"zone": 0, "zone_name": "UTC"},
    "service": {"free_heap": 180000, "first_frame_ms": 480, "display_ms": 320,
                "lvgl_ms": 2, "touch_ms": 80},
    "temperatures": [{"valid": False, "ever": False, "history": []} for _ in range(6)],
    "fuel": {"lower": {"valid": False}, "upper": {"valid": False}},
    "fuel_level_text": "Нет данных", "equipment": [],
    "connections": empty_connections(100),
    "storage": {"started": True, "mounted": True, "logging": True,
                "format_attempted": False, "format_state": 0},
    "home_assistant": {"ready": True, "supported": True, "enabled": False,
                       "connected": False, "revision": 0, "port": 1883},
    "update": {"started": True, "supported": True, "busy": False, "available": False,
               "automatic_rollback": True, "phase": "current", "latest_version": version,
               "boot_pending": False},
}
guard = Lock()
release_ack = Event()
post_seen = Event()
post_headers_seen = Event()
status_gates = []
state = {"status_polls": 0, "status_code": 200, "next_mode": "success",
         "commands": [], "other_posts": [], "status_override": None,
         "status_holds": [], "status_trace": [], "status_body_hold": None,
         "unauthorized_status_hold": None, "session_token": FIXTURE_TOKEN, "pairings": 0}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *values):
        pass

    def reply(self, status, value, mime="application/json", body_hold=None):
        data = value if isinstance(value, bytes) else json.dumps(value, ensure_ascii=False).encode("utf-8")
        try:
            self.send_response(status)
            self.send_header("Content-Type", mime + ("; charset=utf-8" if mime in ("application/json", "text/html") else ""))
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            if body_hold:
                self.wfile.write(data[:1])
                self.wfile.flush()
                body_hold[1].set()
                body_hold[0].wait(30)
                self.wfile.write(data[1:])
            else:
                self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            # The timeout scenario deliberately aborts a response after commit.
            pass

    def do_GET(self):
        if self.path == "/":
            return self.reply(200, html, "text/html")
        if self.path == "/assets/jp1-pinout.jpg":
            return self.reply(200, (ROOT / "src/web_assets/jp1-pinout.jpg").read_bytes(), "image/jpeg")
        if self.path == "/api/status":
            with guard:
                state["status_polls"] += 1
                data = deepcopy(fixture)
                data["authorized"] = bool(fixture["authorized"] and
                    self.headers.get("Authorization") == "Bearer " + state["session_token"])
                if state["status_override"] is not None:
                    data["connections"] = deepcopy(state["status_override"])
                data["uptime_s"] = 1000 + state["status_polls"]
                status = state["status_code"]
                hold = state["status_holds"].pop(0) if state["status_holds"] else None
                if not data["authorized"] and state["unauthorized_status_hold"] is not None:
                    hold = state["unauthorized_status_hold"]
                    state["unauthorized_status_hold"] = None
                body_hold = state["status_body_hold"]
                state["status_body_hold"] = None
                state["status_trace"].append({"sequence": state["status_polls"],
                                              "revision": data["connections"]["revision"],
                                              "authorized": data["authorized"],
                                              "held": hold is not None,
                                              "body_held": body_hold is not None})
                if hold:
                    hold[1].set()
            if hold:
                hold[0].wait(30)
            return self.reply(status, data if status == 200 else {"error": "fixture_offline"}, body_hold=body_hold)
        self.reply(404, {})

    def do_POST(self):
        raw = self.rfile.read(int(self.headers.get("Content-Length", "0"))).decode("utf-8")
        parsed = parse_qs(raw, keep_blank_values=True)
        data = {key: values[0] for key, values in parsed.items()}
        if self.path != "/api/command":
            with guard:
                state["other_posts"].append({"path": self.path, "fields": sorted(data)})
            if self.path == "/api/pair/start":
                return self.reply(200, {"ok": True, "expires_in_s": 120})
            if self.path == "/api/pair" and data.get("code") == "123456":
                with guard:
                    fixture["authorized"] = True
                    state["pairings"] += 1
                    state["session_token"] = FIXTURE_TOKEN + "-" + str(state["pairings"])
                    session_token = state["session_token"]
                return self.reply(200, {"ok": True, "token": session_token})
            if self.path == "/api/pair/cancel":
                return self.reply(200, {"ok": True})
            return self.reply(400, {"error": "unexpected_fixture_route"})
        with guard:
            mode = state["next_mode"]
            state["next_mode"] = "success"
            before = deepcopy(fixture["connections"])
            entry = {"path": self.path, "data": data, "mode": mode,
                     "revision_before": before["revision"], "status": None}
            state["commands"].append(entry)
            if mode == "unauthorized":
                fixture["authorized"] = False
            if mode == "stale":
                fixture["connections"]["revision"] += 1
                fixture["connections"]["items"][5].update(type="mqtt", gpio=-1, jp1_pin=0)
            if mode == "ota_race":
                fixture["update"]["busy"] = True
            status, response = self.apply_command(data, parsed, mode)
            if mode == "ack_before_snapshot" and status == 200:
                state["status_override"] = before
            entry.update(status=status, revision_after=fixture["connections"]["revision"],
                         items_after=deepcopy(fixture["connections"]["items"]))
            post_seen.set()
        if mode == "hold_ack" and status == 200:
            release_ack.wait(30)
        self.reply(status, response, body_hold=(release_ack, post_headers_seen)
                   if mode == "hold_body" and status == 200 else None)

    def apply_command(self, data, parsed, mode):
        # Explicit fixture implementation of the published HTTP contract.
        # C++/NVS correctness is proved separately, not by this simulator.
        if self.headers.get("Authorization") != "Bearer " + state["session_token"] or not fixture["authorized"]:
            return 401, {"error": "pair_required"}
        if self.headers.get("X-Boiler-Pairing") != "1" or self.headers.get("Origin") != base:
            return 403, {"error": "same_origin_required"}
        if fixture["update"]["busy"] or fixture["update"]["boot_pending"]:
            return 409, {"error": "update_busy"}
        action = data.get("action")
        required = {"action", "gpio", "revision"}
        if action == "connection_pin_set":
            required |= {"channel", "type", "debounce"}
        elif action != "connection_pin_clear":
            return 400, {"error": "invalid_pin_fields"}
        if set(data) != required or any(len(values) != 1 for values in parsed.values()):
            return 400, {"error": "invalid_pin_fields"}
        if any(not re.fullmatch(r"\d+", value) for key, value in data.items() if key != "action"):
            return 400, {"error": "invalid_pin_fields"}
        gpio, revision = int(data["gpio"]), int(data["revision"])
        if gpio not in pins or revision > 0xffffffff:
            return 400, {"error": "invalid_pin_fields"}
        if revision != fixture["connections"]["revision"]:
            return 409, {"error": "stale_revision"}
        if mode == "invalid":
            return 400, {"error": "invalid_pin_assignment"}
        if mode == "write_fail":
            return 503, {"error": "configuration_write_failed"}
        items = deepcopy(fixture["connections"]["items"])
        for channel, item in enumerate(items):
            if item["gpio"] == gpio:
                items[channel] = empty_item(channel)
        if action == "connection_pin_set":
            channel, kind, debounce = (int(data[key]) for key in ("channel", "type", "debounce"))
            thermometer = channel in (0, 1, 2, 3, 6, 7) and kind == 1 and debounce == 50
            reed = channel in (4, 5) and kind in (2, 3) and 0 <= debounce <= 5000
            relay = channel in (8, 9, 10, 11) and kind == 6 and debounce == 50
            if not (thermometer or reed or relay):
                return 400, {"error": "invalid_pin_assignment"}
            kind_name = {1: "ds18b20", 2: "reed_no", 3: "reed_nc", 6: "relay"}[kind]
            items[channel] = local_item(channel, kind_name, gpio, debounce)
        fields = ("type", "gpio", "address", "debounce_ms")
        changed = any(a[key] != b[key] for a, b in zip(items, fixture["connections"]["items"]) for key in fields)
        if changed:
            fixture["connections"]["revision"] += 1
            fixture["connections"]["items"] = items
        return 200, {"ok": True, "saved": True, "revision": fixture["connections"]["revision"]}


server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
Thread(target=server.serve_forever, daemon=True).start()
base = f"http://127.0.0.1:{server.server_port}"
result = {
    "result": "RUNNING", "version": version, "source_header": str(header_path),
    "source_header_sha256": sha256(header_bytes).hexdigest(),
    "test_script": "tests/pin_actions_web_browser.py",
    "test_script_sha256": sha256(Path(__file__).read_bytes()).hexdigest(),
    "pin_profile_sha256": sha256(profile_bytes).hexdigest(),
    "widths": [], "cases": [], "page_errors": [], "external_requests": [],
    "client_command_requests": [], "mutation_requests": [], "status_polls": 0,
    "physical_panel_contacted": False,
    "data": "isolated fixtures; actual embedded page and unchanged three-second status polling",
}
try:
    with sync_playwright() as playwright:
        executable = os.environ.get("BP_BROWSER")
        if not executable and os.name == "nt":
            executable = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
        browser = playwright.chromium.launch(headless=True, executable_path=executable)
        context = browser.new_context(viewport={"width": 1280, "height": 900}, reduced_motion="reduce")
        context.add_init_script("sessionStorage.setItem('boiler_panel_token', " + json.dumps(FIXTURE_TOKEN) + ")")
        context.on("request", lambda request: result["external_requests"].append(request.url)
                   if not request.url.startswith(base + "/") else None)
        page = context.new_page()
        page.on("pageerror", lambda error: result["page_errors"].append(str(error)))
        page.on("request", lambda request: result["client_command_requests"].append(request.post_data)
                if request.method == "POST" and request.url == base + "/api/command" else None)
        page.goto(base)
        page.click('.topnav [data-page="settings"]')
        page.click('#settings-menu [data-page="connections"]')
        expect(page.locator("#cfg-advanced")).to_have_js_property("open", False)
        dialog = page.locator("#pin-dialog")
        save = page.locator("#pin-save")
        clear = page.locator("#pin-clear")
        status = page.locator("#pin-dialog-status")
        channel = page.locator("#pin-channel")

        def count_commands():
            with guard:
                return len(state["commands"])

        def wait_polls(count=1):
            with guard:
                target = 1000 + state["status_polls"] + count
            page.wait_for_function(
                r"""target => {
                    const match=document.getElementById('uptime').textContent.match(/^(\d+) мин (\d+) сек$/);
                    return !!match&&Number(match[1])*60+Number(match[2])>=target;
                }""", arg=target, timeout=(count + 2) * 3500)

        def record(name, **details):
            result["cases"].append({"name": name, "commands_seen": count_commands(), **details})

        def pin(gpio):
            return page.locator(f'#jp1-map button[data-gpio="{gpio}"]')

        def expect_pin(gpio, state_name, label=None):
            expect(pin(gpio)).to_have_attribute("data-state", state_name)
            if label is not None:
                expect(pin(gpio).locator(".jp1-status")).to_have_text(label)

        def open_pin(gpio):
            pin(gpio).click()
            expect(dialog).to_have_js_property("open", True)
            expect(page.locator("#pin-title")).to_have_text(f"GPIO{gpio}")
            expect(page.locator("#pin-location")).to_have_text(f"JP1 · контакт {pins[gpio]}")

        def close_pin(gpio):
            page.keyboard.press("Escape")
            expect(dialog).to_have_js_property("open", False)
            expect(pin(gpio)).to_be_focused()

        def last_command(expected, status_code=200):
            with guard:
                entry = deepcopy(state["commands"][-1])
            assert entry["data"] == {key: str(value) for key, value in expected.items()}, entry
            assert entry["status"] == status_code, entry
            return entry

        def click_save():
            before = count_commands()
            save.click()
            expect(status).to_contain_text("Сохранено. Назначения подтверждены панелью.")
            assert count_commands() == before + 1

        def revision():
            with guard:
                return fixture["connections"]["revision"]

        def geometry(width):
            metrics = dialog.evaluate("""dialog => {
                const box=r=>({left:r.left,right:r.right,top:r.top,bottom:r.bottom,width:r.width,height:r.height});
                return {dialog:box(dialog.getBoundingClientRect()),scroll_width:dialog.scrollWidth,
                    client_width:dialog.clientWidth,scroll_height:dialog.scrollHeight,client_height:dialog.clientHeight,
                    controls:Array.from(dialog.querySelectorAll('button,input,select')).filter(node=>node.getClientRects().length)
                        .map(node=>({id:node.id,box:box(node.getBoundingClientRect())}))};
            }""")
            bounds = metrics["dialog"]
            assert 0 <= bounds["left"] < bounds["right"] <= width
            assert 0 <= bounds["top"] < bounds["bottom"] <= 900
            assert metrics["scroll_width"] <= metrics["client_width"], metrics
            for control in metrics["controls"]:
                rect = control["box"]
                assert rect["width"] > 0 and rect["height"] >= 44, control
                assert bounds["left"] <= rect["left"] < rect["right"] <= bounds["right"], control
            assert page.evaluate("document.documentElement.scrollWidth <= innerWidth")
            (out / f"pin-actions-{width}-metrics.json").write_text(
                json.dumps(metrics, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            page.screenshot(path=str(out / f"pin-actions-{width}.png"), full_page=True)
            return metrics

        for width in (320, 390, 768, 1280):
            page.set_viewport_size({"width": width, "height": 900})
            with guard:
                fixture["connections"] = empty_connections(width * 10)
            wait_polls()
            expect(page.locator('#jp1-map button')).to_have_count(11)
            expect(page.locator('#jp1-map [data-state="fixed"]')).to_have_count(15)
            assert pin(52).bounding_box()["height"] >= 44
            start_count = count_commands()
            open_pin(52)
            expect(channel).to_have_value("")
            expect(save).to_be_disabled()
            channel.select_option("0")
            expect(save).to_be_enabled()
            expect_pin(52, "free", "Свободен")
            close_pin(52)
            assert count_commands() == start_count
            if width == 1280:
                # Native Tab walks only the 11 assignable GPIO buttons; reserved
                # contacts never become assignment actions.
                for index, gpio in enumerate(pins):
                    if index:
                        page.keyboard.press("Tab")
                    expect(pin(gpio)).to_be_focused()
                    expect(pin(gpio)).to_have_css("outline-style", "solid")
                    page.keyboard.press("Enter" if index % 2 == 0 else "Space")
                    expect(page.locator("#pin-title")).to_have_text(f"GPIO{gpio}")
                    close_pin(gpio)
                assert count_commands() == start_count
                record("native-keyboard-pin-menu", keys=["Tab", "Enter", "Space", "Escape"], gpio_count=11)
            open_pin(52)
            channel.select_option("0")
            saved_revision = revision()
            if width == 320:
                with guard:
                    state["next_mode"] = "ack_before_snapshot"
                save.click()
                expect(status).to_contain_text("Проверяем сохранённые назначения")
                wait_polls()
                expect(save).to_be_disabled()
                expect(status).not_to_contain_text("Сохранено.")
                expect_pin(52, "free", "Свободен")
                assert count_commands() == start_count + 1
                with guard:
                    state["status_override"] = None
                wait_polls()
                expect(status).to_contain_text("Сохранено. Назначения подтверждены панелью.")
                record("ack-is-not-a-confirmed-snapshot", width=width, optimistic_occupancy=False)
            else:
                click_save()
            last_command({"action": "connection_pin_set", "gpio": 52, "channel": 0,
                          "type": 1, "debounce": 50, "revision": saved_revision})
            expect_pin(52, "busy", "Занят · В доме")
            close_pin(52)

            open_pin(33)
            channel.select_option("0")
            expect(page.locator("#pin-impact")).to_contain_text("с GPIO52 на GPIO33")
            click_save()
            expect_pin(52, "free")
            expect_pin(33, "busy", "Занят · В доме")
            close_pin(33)
            open_pin(52)
            channel.select_option("1")
            click_save()
            close_pin(52)
            open_pin(33)
            expect(channel).to_have_value("0")
            channel.select_option("1")
            expect(page.locator("#pin-impact")).to_contain_text("«В доме» будет отключён от GPIO33")
            expect(page.locator("#pin-impact")).to_contain_text("с GPIO52 на GPIO33")
            saved_revision = revision()
            click_save()
            last_command({"action": "connection_pin_set", "gpio": 33, "channel": 1,
                          "type": 1, "debounce": 50, "revision": saved_revision})
            with guard:
                assert fixture["connections"]["items"][0]["type"] == "none"
                assert fixture["connections"]["items"][1]["gpio"] == 33
            expect_pin(52, "free")
            expect_pin(33, "busy", "Занят · На улице")
            close_pin(33)

            open_pin(28)
            channel.select_option("4")
            expect(page.locator("#pin-type")).to_have_value("")
            expect(save).to_be_disabled()
            page.locator("#pin-type").select_option("2")
            page.locator("#pin-debounce").fill("275")
            expect(page.locator("#pin-debounce")).to_be_focused()
            before = count_commands()
            wait_polls()
            expect(channel).to_have_value("4")
            expect(page.locator("#pin-type")).to_have_value("2")
            expect(page.locator("#pin-debounce")).to_have_value("275")
            expect(page.locator("#pin-debounce")).to_be_focused()
            assert count_commands() == before
            metrics = geometry(width)
            saved_revision = revision()
            click_save()
            last_command({"action": "connection_pin_set", "gpio": 28, "channel": 4,
                          "type": 2, "debounce": 275, "revision": saved_revision})
            close_pin(28)
            open_pin(28)
            expect(channel).to_have_value("4")
            expect(page.locator("#pin-type")).to_have_value("2")
            expect(page.locator("#pin-debounce")).to_have_value("275")
            page.locator("#pin-type").select_option("3")
            page.locator("#pin-debounce").fill("300")
            click_save()
            close_pin(28)
            open_pin(28)
            expect(page.locator("#pin-type")).to_have_value("3")
            expect(page.locator("#pin-debounce")).to_have_value("300")
            before = count_commands()
            page.once("dialog", lambda prompt: prompt.dismiss())
            clear.click()
            assert count_commands() == before
            expect_pin(28, "busy", "Занят · Нижний геркон")
            saved_revision = revision()
            page.once("dialog", lambda prompt: prompt.accept())
            clear.click()
            expect(status).to_contain_text("Пин освобождён. Сохранённые назначения подтверждены панелью.")
            assert count_commands() == before + 1
            last_command({"action": "connection_pin_clear", "gpio": 28, "revision": saved_revision})
            expect_pin(28, "free")
            expect(clear).to_be_hidden()
            close_pin(28)
            open_pin(33)
            saved_revision = revision()
            click_save()
            assert revision() == saved_revision, "No-op save changed the fixture revision"
            close_pin(33)
            if width == 1280:
                for source, gpio, label in ((2, 51, "Подача"), (6, 50, "Обратка"),
                                            (3, 29, "Топливо в баке"), (7, 30, "Топливо на выходе")):
                    open_pin(gpio)
                    channel.select_option(str(source))
                    expect(page.locator("#pin-temperature")).to_be_visible()
                    expect(page.locator("#pin-reed")).to_be_hidden()
                    saved_revision = revision()
                    click_save()
                    last_command({"action": "connection_pin_set", "gpio": gpio, "channel": source,
                                  "type": 1, "debounce": 50, "revision": saved_revision})
                    close_pin(gpio)
                    expect_pin(gpio, "busy", "Занят · " + label)
                expect_pin(51, "busy", "Занят · Подача")
                expect_pin(50, "busy", "Занят · Обратка")
                expect_pin(29, "busy", "Занят · Топливо в баке")
                expect_pin(30, "busy", "Занят · Топливо на выходе")
                record("independent-supply-and-return-source-assignments")
                record("independent-fuel-tank-and-outlet-source-assignments")
            result["widths"].append(width)
            record("responsive-assign-move-replace-clear", width=width,
                   posted_commands=count_commands() - start_count, explicit_no_nc=True,
                   draft_and_focus_preserved=True, geometry=metrics)

        # Existing sensor bindings must survive all three electric outputs and the separate DHW relay.
        with guard:
            inputs = deepcopy(fixture["connections"]["items"][:8])
        relay_names = {8: "Реле ТЭН-1", 10: "Реле ТЭН-2", 11: "Реле ТЭН-3", 9: "Реле бойлера"}
        for width in (320, 1280):
            page.set_viewport_size({"width": width, "height": 900})
            relay_pins = ((8, 31), (10, 34), (11, 49), (9, 32))
            for owner, gpio in relay_pins:
                pin(gpio).focus()
                page.keyboard.press("Enter")
                expect(dialog).to_have_js_property("open", True)
                for role, name in relay_names.items():
                    expect(channel.locator(f'option[value="{role}"]')).to_have_text(name)
                channel.select_option(str(owner))
                expect(page.locator("#pin-temperature")).to_be_hidden()
                expect(page.locator("#pin-reed")).to_be_hidden()
                expect(page.locator("#pin-debounce")).to_be_disabled()
                expect_pin(gpio, "free")
                saved_revision = revision()
                click_save()
                last_command({"action": "connection_pin_set", "gpio": gpio, "channel": owner,
                              "type": 6, "debounce": 50, "revision": saved_revision})
                expect_pin(gpio, "busy", "Занят · " + relay_names[owner])
                close_pin(gpio)

            # Discarding a draft leaves both existing owners and all other channels untouched.
            before = count_commands()
            open_pin(49)
            expect(channel).to_have_value("11")
            channel.select_option("10")
            expect(page.locator("#pin-impact")).to_contain_text("с GPIO34 на GPIO49")
            close_pin(49)
            assert count_commands() == before
            for owner, gpio in relay_pins:
                expect_pin(gpio, "busy", "Занят · " + relay_names[owner])

            open_pin(31)
            expect(channel).to_have_value("8")
            expect(page.locator("#pin-current")).to_contain_text(relay_names[8])
            if width == 1280:
                dialog.screenshot(path=str(out / "jp1-relay-assignment.png"))
            close_pin(31)
            if width == 1280:
                page.locator("#jp1-map").screenshot(path=str(out / "jp1-relay-map.png"))

            # Move heater 1 onto the DHW relay's pin. Only the
            # chosen owner survives; the old pin is released in the same write.
            open_pin(32)
            expect(channel).to_have_value("9")
            channel.select_option("8")
            expect(page.locator("#pin-impact")).to_contain_text(relay_names[9])
            expect(page.locator("#pin-impact")).to_contain_text("с GPIO31 на GPIO32")
            saved_revision = revision()
            click_save()
            last_command({"action": "connection_pin_set", "gpio": 32, "channel": 8,
                          "type": 6, "debounce": 50, "revision": saved_revision})
            expect_pin(31, "free")
            expect_pin(32, "busy", "Занят · " + relay_names[8])
            with guard:
                assert fixture["connections"]["items"][9] == empty_item(9)
            page.once("dialog", lambda prompt: prompt.accept())
            saved_revision = revision()
            clear.click()
            expect(status).to_contain_text("Пин освобождён")
            last_command({"action": "connection_pin_clear", "gpio": 32, "revision": saved_revision})
            expect_pin(32, "free")
            close_pin(32)
            for owner, gpio in ((10, 34), (11, 49)):
                expect_pin(gpio, "busy", "Занят · " + relay_names[owner])
                open_pin(gpio)
                expect(channel).to_have_value(str(owner))
                page.once("dialog", lambda prompt: prompt.accept())
                clear.click()
                expect(status).to_contain_text("Пин освобождён")
                expect_pin(gpio, "free")
                close_pin(gpio)
            with guard:
                assert fixture["connections"]["items"][:8] == inputs
            record("independent-relay-output-roles", width=width, electric_outputs=3, existing_inputs_preserved=True)

        # Failure/race scenarios use a saved local owner and complete snapshots.
        with guard:
            fixture["connections"] = empty_connections(20000)
            fixture["connections"]["items"][0] = local_item(0, "ds18b20", 52)
            fixture["connections"]["items"][1] = local_item(1, "ds18b20", 33)
        wait_polls()
        open_pin(52)
        channel.select_option("2")
        before = count_commands()
        with guard:
            fixture["connections"]["revision"] += 1
            fixture["connections"]["items"][5].update(type="mqtt")
        wait_polls()
        expect(status).to_contain_text("Назначения изменились")
        expect(channel).to_have_value("2")
        expect(save).to_be_disabled()
        expect(clear).to_be_disabled()
        assert count_commands() == before
        page.locator("#pin-reload").click()
        expect(status).to_contain_text("Назначения перечитаны")
        expect(channel).to_have_value("0")
        expect(save).to_be_enabled()
        record("external-revision-blocks-until-explicit-reload")

        channel.select_option("5")
        expect(page.locator("#pin-impact")).to_contain_text("Источник MQTT")
        expect(page.locator("#pin-type")).to_have_value("")
        page.locator("#pin-type").select_option("3")
        for value in ("", "-1", "50.5", "5001"):
            page.locator("#pin-debounce").fill(value)
            expect(save).to_be_disabled()
        for value in ("0", "5000"):
            page.locator("#pin-debounce").fill(value)
            expect(save).to_be_enabled()
        page.locator("#pin-debounce").fill("200")
        before = count_commands()
        with guard:
            valid = deepcopy(fixture["connections"])
        invalids = [("unavailable", dict(valid, available=False)),
                    ("partial", dict(valid, items=valid["items"][:-1])),
                    ("missing-debounce", deepcopy(valid)),
                    ("conflict", deepcopy(valid))]
        invalids[2][1]["items"][0].pop("debounce_ms")
        invalids[3][1]["items"][1] = local_item(1, "ds18b20", 52)
        for name, invalid in invalids:
            with guard:
                fixture["connections"] = invalid
            wait_polls()
            expect(save).to_be_disabled()
            expect(clear).to_be_disabled()
            expect(status).to_contain_text("Нет полного достоверного снимка")
            expect(channel).to_have_value("5")
            expect(page.locator("#pin-debounce")).to_have_value("200")
            with guard:
                fixture["connections"] = deepcopy(valid)
            wait_polls()
            expect(save).to_be_enabled()
            record("blocked-" + name, draft_preserved=True)
        assert count_commands() == before

        with guard:
            state["status_code"] = 503
        expect(status).to_contain_text("Нет связи с панелью", timeout=8000)
        expect(save).to_be_disabled()
        expect(clear).to_be_disabled()
        expect_pin(52, "unknown", "Нет связи")
        expect(channel).to_have_value("5")
        expect(page.locator("#pin-debounce")).to_have_value("200")
        with guard:
            state["status_code"] = 200
        wait_polls()
        expect(save).to_be_enabled()
        assert count_commands() == before
        record("offline-recovery-keeps-draft")

        for flag in ("busy", "boot_pending"):
            with guard:
                fixture["update"][flag] = True
            wait_polls()
            expect(status).to_contain_text("Дождитесь завершения обновления")
            expect(save).to_be_disabled()
            expect(clear).to_be_disabled()
            expect(channel).to_have_value("5")
            with guard:
                fixture["update"][flag] = False
            wait_polls()
            expect(save).to_be_enabled()
        assert count_commands() == before
        record("ota-busy-and-boot-validation-block-writes")

        # A 401 from the command must retain the draft and pairing can occur
        # without closing the pin editor. PIN/token below are test fixture data.
        old_auth_gate, old_auth_seen, fresh_auth_gate, fresh_auth_seen = (Event() for _ in range(4))
        status_gates.extend((old_auth_gate, fresh_auth_gate))
        with guard:
            state["next_mode"] = "unauthorized"
            state["unauthorized_status_hold"] = (old_auth_gate, old_auth_seen)
        save.click()
        expect(status).to_contain_text("Для сохранения выполните сопряжение")
        expect(save).to_be_disabled()
        expect(channel).to_have_value("5")
        expect(page.locator("#pin-debounce")).to_have_value("200")
        assert count_commands() == before + 1
        assert old_auth_seen.wait(3), "Read-only GET was not held before pairing"
        page.locator("#pin-pair").click()
        expect(page.locator("#pairbox")).to_have_js_property("open", True)
        expect(page.locator("#pair-submit")).to_be_enabled()
        page.locator("#paircode").fill("123456")
        page.locator("#pair-submit").click()
        expect(page.locator("#pairbox")).to_have_js_property("open", False)
        expect(dialog).to_have_js_property("open", True)
        with guard:
            repaired_token = state["session_token"]
            state["status_holds"] = [(fresh_auth_gate, fresh_auth_seen)]
        old_auth_gate.set()
        assert fresh_auth_seen.wait(3), "The old auth response did not queue a GET with the new token"
        assert page.evaluate("sessionStorage.getItem('boiler_panel_token')") == repaired_token
        fresh_auth_gate.set()
        expect(channel).to_have_value("5")
        expect(page.locator("#pin-type")).to_have_value("3")
        expect(page.locator("#pin-debounce")).to_have_value("200")
        expect(save).to_be_enabled()
        expect(page.locator("#authstate")).to_have_text("Доступ открыт")
        assert page.evaluate("sessionStorage.getItem('boiler_panel_token')") == repaired_token
        record("old-read-only-get-cannot-revoke-repaired-session", command_status=401,
               draft_preserved=True, new_token_preserved=True)

        before = count_commands()
        old_gate, old_seen, fresh_gate, fresh_seen = (Event() for _ in range(4))
        status_gates.extend((old_gate, fresh_gate))
        with guard:
            state["next_mode"] = "stale"
            state["status_holds"] = [(old_gate, old_seen), (fresh_gate, fresh_seen)]
        assert old_seen.wait(4), "Old GET did not start before the conflicting command"
        save.click()
        expect(status).to_contain_text("Назначения изменились")
        expect(save).to_be_disabled()
        expect(clear).to_be_disabled()
        expect(channel).to_have_value("5")
        assert count_commands() == before + 1
        with guard:
            assert state["commands"][-1]["status"] == 409
        page.locator("#pin-reload").click()
        old_gate.set()
        assert fresh_seen.wait(3), "Explicit reload did not queue a fresh GET"
        expect(status).to_have_text("Перечитываем назначения…")
        expect(save).to_be_disabled()
        expect(clear).to_be_disabled()
        expect(channel).to_have_value("5")
        fresh_gate.set()
        expect(status).to_contain_text("Назначения перечитаны")
        record("held-old-get-cannot-satisfy-explicit-reload", command_status=409,
               next_get_started_after_reload=True, stale_response_never_enabled_save=True)

        channel.select_option("2")
        before = count_commands()
        with guard:
            state["next_mode"] = "invalid"
        save.click()
        expect(status).to_contain_text("Это назначение недоступно")
        expect(channel).to_have_value("2")
        expect_pin(52, "busy", "Занят · В доме")
        assert count_commands() == before + 1
        record("invalid-command-keeps-saved-owner", command_status=400)

        before = count_commands()
        with guard:
            state["next_mode"] = "write_fail"
        save.click()
        expect(status).to_contain_text("Сохранение не подтверждено")
        expect(save).to_be_disabled()
        expect(clear).to_be_disabled()
        expect(channel).to_have_value("2")
        expect_pin(52, "busy", "Занят · В доме")
        wait_polls(2)
        assert count_commands() == before + 1, "503 caused an automatic POST retry"
        expect(save).to_be_disabled()
        page.locator("#pin-reload").click()
        expect(status).to_contain_text("Назначения перечитаны")
        expect(channel).to_have_value("0")
        record("nvs-failure-unconfirmed-without-auto-retry", command_status=503)

        # A status/command race is still rejected by the server's OTA guard.
        channel.select_option("2")
        before = count_commands()
        with guard:
            state["next_mode"] = "ota_race"
        save.click()
        expect(status).to_contain_text("Дождитесь завершения обновления")
        expect(save).to_be_disabled()
        expect(channel).to_have_value("2")
        assert count_commands() == before + 1
        with guard:
            assert state["commands"][-1]["status"] == 409
            fixture["update"]["busy"] = False
        wait_polls()
        expect(save).to_be_enabled()
        record("ota-started-between-status-and-command", command_status=409)

        # Store the change, hold the HTTP response beyond the page's real 8s
        # AbortController timeout, and prove no second request is sent.
        before = count_commands()
        with guard:
            state["next_mode"] = "hold_ack"
        release_ack.clear()
        post_seen.clear()
        save.click()
        assert post_seen.wait(3), "Fixture never received the held command"
        expect(status).to_have_text("Сохраняем назначение…")
        expect(save).to_be_disabled()
        expect(clear).to_be_disabled()
        expect(page.locator("#pin-close")).to_be_disabled()
        page.keyboard.press("Escape")
        expect(dialog).to_have_js_property("open", True)
        expect(status).to_contain_text("Сохранение не подтверждено", timeout=12000)
        release_ack.set()
        expect(channel).to_have_value("2")
        expect(save).to_be_disabled()
        expect(page.locator("#pin-close")).to_be_enabled()
        wait_polls(2)
        assert count_commands() == before + 1, "Timeout caused an automatic POST retry"
        expect(save).to_be_disabled()
        page.locator("#pin-reload").click()
        expect(status).to_contain_text("Назначения перечитаны")
        expect(channel).to_have_value("2")
        expect_pin(52, "busy", "Занят · Подача")
        close_pin(52)
        record("timeout-after-storage-no-repeat-and-explicit-reload", automatic_retries=0)

        # The deadline must cover a body after HTTP 200 headers have already
        # arrived. Holding only the headers would miss this regression.
        open_pin(52)
        channel.select_option("3")
        before = count_commands()
        with guard:
            state["next_mode"] = "hold_body"
        release_ack.clear()
        post_headers_seen.clear()
        save.click()
        assert post_headers_seen.wait(3), "Partial POST response headers/body were not sent"
        expect(status).to_have_text("Сохраняем назначение…")
        expect(page.locator("#pin-close")).to_be_disabled()
        page.keyboard.press("Escape")
        expect(dialog).to_have_js_property("open", True)
        expect(status).to_contain_text("Сохранение не подтверждено", timeout=12000)
        expect(page.locator("#pin-close")).to_be_enabled()
        expect(save).to_be_disabled()
        expect(channel).to_have_value("3")
        release_ack.set()
        wait_polls(2)
        assert count_commands() == before + 1, "Stalled response body caused an automatic POST retry"
        page.locator("#pin-reload").click()
        expect(status).to_contain_text("Назначения перечитаны")
        expect(channel).to_have_value("3")
        expect_pin(52, "busy", "Занят · Топливо в баке")
        close_pin(52)
        record("post-headers-then-stalled-body-times-out", automatic_retries=0,
               close_available_after_deadline=True)

        # A partial status body similarly becomes offline, leaving the menu
        # usable and its draft intact; later status polling recovers normally.
        open_pin(52)
        channel.select_option("4")
        page.locator("#pin-type").select_option("3")
        page.locator("#pin-debounce").fill("120")
        before = count_commands()
        status_body_gate, status_body_seen = Event(), Event()
        status_gates.append(status_body_gate)
        with guard:
            state["status_body_hold"] = (status_body_gate, status_body_seen)
        assert status_body_seen.wait(4), "Partial GET response headers/body were not sent"
        with guard:
            # Keep later polls offline until the assertions explicitly recover.
            state["status_code"] = 503
        expect(status).to_contain_text("Нет связи с панелью", timeout=12000)
        expect(save).to_be_disabled()
        expect(clear).to_be_disabled()
        expect(page.locator("#pin-close")).to_be_enabled()
        expect(channel).to_have_value("4")
        expect(page.locator("#pin-type")).to_have_value("3")
        expect(page.locator("#pin-debounce")).to_have_value("120")
        expect(page.locator("#pin-debounce")).to_be_focused()
        status_body_gate.set()
        with guard:
            state["status_code"] = 200
        wait_polls()
        expect(save).to_be_enabled()
        expect(channel).to_have_value("4")
        expect(page.locator("#pin-debounce")).to_have_value("120")
        expect(page.locator("#pin-debounce")).to_be_focused()
        assert count_commands() == before
        close_pin(52)
        record("get-headers-then-stalled-body-offline-and-recovery", draft_preserved=True,
               close_available_during_offline=True, posted_commands=0)

        open_pin(31)
        channel.select_option("5")
        expect(page.locator("#pin-impact")).to_contain_text("Источник MQTT")
        expect(page.locator("#pin-type")).to_have_value("")
        page.locator("#pin-type").select_option("3")
        page.locator("#pin-debounce").fill("0")
        saved_revision = revision()
        click_save()
        last_command({"action": "connection_pin_set", "gpio": 31, "channel": 5,
                      "type": 3, "debounce": 0, "revision": saved_revision})
        expect_pin(31, "busy", "Занят · Верхний геркон")
        expect_pin(52, "busy", "Занят · Топливо в баке")
        close_pin(31)
        record("remote-descriptor-replaced-by-explicit-local-sensor", other_gpio_preserved=True)

        assert not result["page_errors"], result["page_errors"]
        assert not result["external_requests"], result["external_requests"]
        with guard:
            assert len(result["client_command_requests"]) == len(state["commands"])
            assert all(entry["data"]["action"] in ("connection_pin_set", "connection_pin_clear")
                       for entry in state["commands"]), "Configuring a pin sent a non-assignment command"
        result["result"] = "PASS"
        browser.close()
except BaseException as error:
    result["result"] = "FAIL"
    result["failure"] = str(error)
    raise
finally:
    release_ack.set()
    for gate in status_gates:
        gate.set()
    with guard:
        result["status_polls"] = state["status_polls"]
        result["mutation_requests"] = deepcopy(state["commands"])
        result["pairing_fixture_requests"] = deepcopy(state["other_posts"])
        result["status_trace"] = deepcopy(state["status_trace"])
    (out / "browser-result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))
    server.shutdown()
