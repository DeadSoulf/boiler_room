"""Equipment disclosure regression in the actual embedded HTML and Chromium.

The HTTP server supplies isolated fixtures only. No panel, serial port or GPIO
is contacted. Real mouse/keyboard input and the unchanged three-second status
poll exercise the page. --source-header selects an exact baseline/candidate.
"""
from argparse import ArgumentParser
from copy import deepcopy
from hashlib import sha256
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from threading import Lock, Thread
import json
import os
from playwright.sync_api import sync_playwright, expect

ROOT = Path(__file__).resolve().parents[1]
parser = ArgumentParser(description=__doc__)
parser.add_argument("--source-header", type=Path, default=ROOT / "src/panel_web_page.h")
parser.add_argument("--output-dir", type=Path, default=ROOT / "artifacts/equipment-web-review")
parser.add_argument("--expect-reset", action="store_true", help="Reproduce the old reset on a baseline header")
args = parser.parse_args()
header_path = args.source_header.resolve()
header_bytes = header_path.read_bytes()
source = header_bytes.decode("utf-8")
html = source[source.index("<!doctype html>"):source.index("</html>") + 7].encode("utf-8")
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
version = (ROOT / "VERSION").read_text(encoding="ascii").strip()
names = ["Бойлер косвенного нагрева", "Электрокотёл", "Контур 1", "Контур 2",
         "Контур 3", "Дизельный отопитель", "Подогрев топлива"]
inventory = [{"name": name, "connected": False, "state": "not_connected"} for name in names]
fixture = {
    "version": version, "clock": "05.11.2026 12:40:00", "uptime_s": 600,
    "authorized": False, "pair_available": False, "pair_remaining_s": 0,
    "physical_control": False, "data_origin": "isolated_fixture",
    "wifi": {"connected": True, "ready": True, "busy": False, "supported": True,
             "has_saved": False, "ip": "192.0.2.10", "aps": []},
    "ble": {"supported": True, "devices": []},
    "screen": {"timeout_s": 60, "touch_ready": True},
    "time": {"zone": 0, "zone_name": "UTC"},
    "service": {"free_heap": 180000, "first_frame_ms": 480, "display_ms": 320,
                "lvgl_ms": 2, "touch_ms": 80},
    "temperatures": [{"valid": False, "ever": False, "history": []} for _ in range(4)],
    "fuel": {"lower": {"valid": False}, "upper": {"valid": False}},
    "fuel_level_text": "Нет данных", "equipment": deepcopy(inventory),
    "connections": {"revision": 0, "drivers_active": False, "items": []},
    "storage": {"started": True, "mounted": True, "logging": True,
                "format_attempted": False, "format_state": 0},
    "home_assistant": {"ready": True, "supported": True, "enabled": False,
                       "connected": False, "revision": 0, "port": 1883},
    "update": {"started": True, "supported": True, "busy": False, "available": False,
               "automatic_rollback": True, "phase": "current", "latest_version": version},
}
state = {"status_polls": 0, "mutation_requests": []}
guard = Lock()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *values):
        pass

    def reply(self, status, value, mime="application/json"):
        data = value if isinstance(value, bytes) else json.dumps(value, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", mime + "; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path == "/":
            return self.reply(200, html, "text/html")
        if self.path == "/api/status":
            with guard:
                state["status_polls"] += 1
                data = deepcopy(fixture)
                # The visible uptime confirms each HTTP response reached updateView.
                data["uptime_s"] = 600 + state["status_polls"]
            return self.reply(200, data)
        self.reply(404, {})

    def do_POST(self):
        self.rfile.read(int(self.headers.get("Content-Length", "0")))
        with guard:
            state["mutation_requests"].append({"method": "POST", "path": self.path})
        self.reply(405, {"error": "read_only_fixture"})


server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
Thread(target=server.serve_forever, daemon=True).start()
base = f"http://127.0.0.1:{server.server_port}"
result = {
    "result": "RUNNING",
    "source_header": str(header_path),
    "source_header_sha256": sha256(header_bytes).hexdigest(),
    "test_script": "tests/equipment_web_browser.py",
    "test_script_sha256": sha256(Path(__file__).read_bytes()).hexdigest(),
    "widths": [],
    "page_errors": [],
    "mutation_requests": [],
    "status_polls": 0,
    "coverage": [],
    "physical_panel_contacted": False,
    "data": "isolated fixtures; status polling interval unchanged",
}
try:
    with sync_playwright() as playwright:
        executable = os.environ.get("BP_BROWSER")
        if not executable and os.name == "nt":
            executable = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
        browser = playwright.chromium.launch(headless=True, executable_path=executable)
        context = browser.new_context(viewport={"width": 1280, "height": 900}, reduced_motion="reduce")
        page = context.new_page()
        page.on("pageerror", lambda error: result["page_errors"].append(str(error)))
        page.goto(base)
        page.click('.topnav [data-page="equipment"]')
        cards = page.locator("#equipment-list > details")
        expect(cards).to_have_count(4)
        summaries = page.locator("#equipment-list > details > summary")
        headings = ["Бойлер", "Электрокотёл", "Дизельный отопитель", "Подогрев топлива"]

        def wait_polls(count=1):
            with guard:
                target = 600 + state["status_polls"] + count
            page.wait_for_function(
                r"""target => {
                    const match = document.getElementById("uptime").textContent.match(/^(\d+) мин (\d+) сек$/);
                    return !!match && Number(match[1])*60+Number(match[2]) >= target;
                }""",
                arg=target, timeout=(count + 2) * 3500,
            )

        if args.expect_reset:
            original = cards.nth(1).element_handle()
            summaries.nth(1).click()
            expect(cards.nth(1)).to_have_js_property("open", True)
            expect(summaries.nth(1)).to_be_focused()
            page.screenshot(path=str(out / "baseline-open.png"), full_page=True)
            wait_polls(2)
            expect(cards.nth(1)).to_have_js_property("open", False)
            assert not original.evaluate("node => node.isConnected")
            expect(summaries.nth(1)).not_to_be_focused()
            page.screenshot(path=str(out / "baseline-reset.png"), full_page=True)
            result.update(result="REPRODUCED", widths=[1280],
                          coverage=["mouse opens equipment", "two actual polls close it",
                                    "original details removed", "keyboard focus lost"])
        else:
            original_cards = cards.element_handles()
            original_summaries = summaries.element_handles()

            def same_cards():
                expect(cards).to_have_count(4)
                assert page.locator("#equipment-list h3").all_text_contents() == headings
                for handle in original_cards + original_summaries:
                    assert handle.evaluate("node => node.isConnected"), "Polling replaced an equipment control"

            for width in (320, 390, 768, 1280):
                page.set_viewport_size({"width": width, "height": 900})
                connected = width in (320, 768)
                with guard:
                    fixture["equipment"] = deepcopy(inventory)
                    fixture["equipment"][1]["connected"] = connected
                    fixture["equipment"][2].update(name="Контур 1 · гостевая", connected=True)
                for index in range(4):
                    expect(cards.nth(index)).to_have_js_property("open", False)
                # Mouse opens the first card. Native Tab/Enter/Space open the others.
                summaries.nth(0).click()
                for index, key in ((1, "Enter"), (2, "Space"), (3, "Enter")):
                    page.keyboard.press("Tab")
                    expect(summaries.nth(index)).to_be_focused()
                    page.keyboard.press(key)
                for index in range(4):
                    expect(cards.nth(index)).to_have_js_property("open", True)
                wait_polls(2)
                same_cards()
                expect(summaries.nth(3)).to_be_focused()
                for index in range(4):
                    expect(cards.nth(index)).to_have_js_property("open", True)
                    assert summaries.nth(index).bounding_box()["height"] >= 44
                expect(summaries.nth(1).locator("p")).to_have_text("Подключён" if connected else "Не подключён")
                rows = cards.nth(1).locator(".row")
                expect(rows.nth(0).locator("span").nth(0)).to_have_text("Контур 1 · гостевая")
                expect(rows.nth(0).locator("span").nth(1)).to_have_text("Подключён")
                assert page.evaluate("document.documentElement.scrollWidth <= innerWidth"), width
                page.screenshot(path=str(out / f"equipment-open-{width}.png"), full_page=True)
                # Explicit close wins and must survive the next incoming status too.
                page.keyboard.press("Enter")
                for index, key in ((2, "Space"), (1, "Enter"), (0, "Space")):
                    page.keyboard.press("Shift+Tab")
                    expect(summaries.nth(index)).to_be_focused()
                    page.keyboard.press(key)
                # Null placeholders and a missing tail do not shift the API's fixed types.
                with guard:
                    fixture["equipment"] = [deepcopy(inventory[0]), None, None,
                                            deepcopy(inventory[3]), deepcopy(inventory[4])]
                wait_polls()
                same_cards()
                expect(summaries.nth(0)).to_be_focused()
                for index in range(4):
                    expect(cards.nth(index)).to_have_js_property("open", False)
                for index in (1, 2, 3):
                    expect(summaries.nth(index).locator("p")).to_have_text("Нет данных")
                expect(rows.nth(0).locator("span").nth(0)).to_have_text("Контур 1")
                expect(rows.nth(0).locator("span").nth(1)).to_have_text("Нет данных")
                result["widths"].append(width)

            # Restoring the complete inventory updates the same still-open DOM card.
            summaries.nth(1).click()
            with guard:
                fixture["equipment"] = deepcopy(inventory)
                fixture["equipment"][5]["connected"] = True
                fixture["equipment"][6]["connected"] = True
            wait_polls()
            same_cards()
            expect(summaries.nth(1).locator("p")).to_have_text("Не подключён")
            expect(summaries.nth(2).locator("p")).to_have_text("Подключён")
            expect(summaries.nth(3).locator("p")).to_have_text("Подключён")
            expect(cards.nth(1)).to_have_js_property("open", True)
            expect(summaries.nth(1)).to_be_focused()

            page.click('.topnav [data-page="home"]')
            wait_polls()
            page.click('.topnav [data-page="equipment"]')
            same_cards()
            expect(cards.nth(1)).to_have_js_property("open", True)
            summaries.nth(1).click()
            wait_polls()
            expect(cards.nth(1)).to_have_js_property("open", False)
            same_cards()
            result.update(result="PASS", coverage=[
                "actual embedded HTML and three-second HTTP polls",
                "mouse opens and closes details", "native Tab/Shift+Tab/Enter/Space",
                "four independently expanded cards over multiple polls at each width",
                "user close survives polling", "details and summary DOM identity retained",
                "keyboard focus retained", "live connected states and circuit names refreshed",
                "null entries and missing tail preserve stable API types and unknown data",
                "restored inventory refreshes values", "page navigation retains user disclosure",
                "four responsive widths and minimum 44px summary targets",
            ])
        assert not result["page_errors"], result["page_errors"]
        assert not state["mutation_requests"], state["mutation_requests"]
        browser.close()
except BaseException as error:
    result["result"] = "FAIL"
    result["failure"] = str(error)
    raise
finally:
    with guard:
        result["status_polls"] = state["status_polls"]
        result["mutation_requests"] = deepcopy(state["mutation_requests"])
    (out / "browser-result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))
    server.shutdown()
