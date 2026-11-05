"""Equipment, Home typography and sensor pinout in embedded HTML/Chromium.

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
import re
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
pinout_source = ROOT / "src/web_assets/jp1-pinout.jpg"
pinout_header = ROOT / "src/panel_web_pinout.h"
pinout_header_bytes = pinout_header.read_bytes()
pinout_array = re.search(r"kJp1PinoutJpeg\[\]\s+PROGMEM\s*=\s*\{(.*?)\};",
                         pinout_header_bytes.decode("ascii"), re.S)
assert pinout_array, "Embedded JP1 byte array missing"
assert not re.sub(r"0x[0-9a-fA-F]{2}|[\s,]", "", pinout_array[1]), "Unexpected embedded image initializer"
pinout_bytes = bytes.fromhex("".join(re.findall(r"0x([0-9a-fA-F]{2})", pinout_array[1])))
assert pinout_bytes == pinout_source.read_bytes(), "Embedded image differs from the supplied JPEG"
pinout_sha256 = sha256(pinout_bytes).hexdigest()
assert len(pinout_bytes) == 114530 and pinout_sha256 == "bbd8c4af43121d48d94854e91782a15e6a59227a6c49be709562fe2a90ca8b38"
profile_bytes = (ROOT / "src/panel_pin_profile.h").read_bytes()
profile_pins = re.search(r"constexpr Pin HEADER\[\]=\{(.*?)\};", profile_bytes.decode("utf-8"), re.S)
sensor_pins = {int(gpio): int(pin) for gpio, pin in re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*\}", profile_pins[1])}
assert len(sensor_pins) == 11
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
version = (ROOT / "VERSION").read_text(encoding="ascii").strip()
names = ["Бойлер косвенного нагрева", "Электрокотёл", "Контур 1", "Контур 2",
         "Контур 3", "Дизельный отопитель", "Подогрев топлива"]
inventory = [{"name": name, "connected": False, "state": "not_connected"} for name in names]


def empty_connections(revision=0):
    return {"available": True, "revision": revision, "drivers_active": False,
            "items": [{"channel": channel, "type": "none", "gpio": -1, "jp1_pin": 0,
                       "active": False, "online": False} for channel in range(6)]}


def local_assignment(channel, kind, gpio):
    # A saved assignment reserves its GPIO even while no sensor can be read.
    return {"channel": channel, "type": kind, "gpio": gpio, "jp1_pin": sensor_pins[gpio],
            "active": False, "online": False, "status": "Sensor not found"}


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
    "connections": empty_connections(),
    "storage": {"started": True, "mounted": True, "logging": True,
                "format_attempted": False, "format_state": 0},
    "home_assistant": {"ready": True, "supported": True, "enabled": False,
                       "connected": False, "revision": 0, "port": 1883},
    "update": {"started": True, "supported": True, "busy": False, "available": False,
               "automatic_rollback": True, "phase": "current", "latest_version": version},
}
state = {"status_polls": 0, "mutation_requests": [], "pinout_requests": 0, "status_code": 200}
guard = Lock()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *values):
        pass

    def reply(self, status, value, mime="application/json"):
        data = value if isinstance(value, bytes) else json.dumps(value, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", mime + ("; charset=utf-8" if mime in ("application/json", "text/html") else ""))
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path == "/":
            return self.reply(200, html, "text/html")
        if self.path == "/assets/jp1-pinout.jpg":
            with guard:
                state["pinout_requests"] += 1
            return self.reply(200, pinout_bytes, "image/jpeg")
        if self.path == "/api/status":
            with guard:
                state["status_polls"] += 1
                data = deepcopy(fixture)
                status_code = state["status_code"]
                # The visible uptime confirms each HTTP response reached updateView.
                data["uptime_s"] = 600 + state["status_polls"]
            return self.reply(status_code, data if status_code == 200 else {"error": "fixture_offline"})
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
    "home_temperature_checks": [],
    "pinout_asset": {"source": "src/web_assets/jp1-pinout.jpg", "bytes": len(pinout_bytes),
                     "sha256": pinout_sha256, "embedded_header": "src/panel_web_pinout.h",
                     "embedded_header_sha256": sha256(pinout_header_bytes).hexdigest()},
    "pinout_checks": [],
    "pinout_transitions": [],
    "pin_profile_sha256": sha256(profile_bytes).hexdigest(),
    "external_requests": [],
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
        context.on("request", lambda request: result["external_requests"].append(request.url)
                   if not request.url.startswith(base + "/") else None)
        page = context.new_page()
        # Only this page owns the inline image reused across all four widths.
        # Original-image popup navigation can arrive before its frame exists.
        pinout_responses = []
        page.on("response", lambda response: pinout_responses.append(response)
                if response.url == base + "/assets/jp1-pinout.jpg" else None)
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

            def home_temperatures(width, case, values):
                for index, value in enumerate(values):
                    expected = "--.- °C" if value is None else f"{value:.1f} °C"
                    expect(page.locator(f"#t{index}")).to_have_text(expected)
                metrics = page.evaluate("""() => {
                    const box = r => ({left:r.left,right:r.right,top:r.top,bottom:r.bottom,width:r.width,height:r.height});
                    return Array.from(document.querySelectorAll("#home .temp")).map(card => {
                        const value=card.querySelector(".temp-value"),style=getComputedStyle(value);
                        const text=document.createRange();text.selectNodeContents(value);
                        const number=document.createRange();number.setStart(value.firstChild,0);
                        number.setEnd(value.firstChild,value.textContent.length-3);
                        const clipping=[];
                        for(let node=value;node&&node!==document.body;node=node.parentElement){
                            const css=getComputedStyle(node);
                            const x=["hidden","clip","auto","scroll"].includes(css.overflowX);
                            const y=["hidden","clip","auto","scroll"].includes(css.overflowY);
                            if(x||y){
                                const r=node.getBoundingClientRect(),left=r.left+node.clientLeft,top=r.top+node.clientTop;
                                clipping.push({element:node.id||node.className,x,y,
                                    bounds:{left,right:left+node.clientWidth,top,bottom:top+node.clientHeight}});
                            }
                        }
                        return {
                            text:value.textContent,font_family:style.fontFamily,line_height:style.lineHeight,
                            font_px:parseFloat(style.fontSize),weight:style.fontWeight,spacing:style.letterSpacing,
                            overflow_x:style.overflowX,overflow_y:style.overflowY,clipping,
                            card:box(card.getBoundingClientRect()),
                            name:box(card.querySelector(".temp-name").getBoundingClientRect()),
                            value:box(value.getBoundingClientRect()),
                            icon:box(card.querySelector("svg").getBoundingClientRect()),
                            status:box(card.querySelector(".temp-detail").getBoundingClientRect()),
                            text_rects:Array.from(text.getClientRects(),box),
                            number_rects:Array.from(number.getClientRects(),box)
                        };
                    });
                }""")
                # Preserve evidence even when an assertion below fails.
                (out / f"home-font-{case}-{width}-metrics.json").write_text(
                    json.dumps(metrics, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
                page.screenshot(path=str(out / f"home-font-{case}-{width}.png"), full_page=True)
                assert len(metrics) == 4
                # Preserve the existing large sizes of the first three cards.
                expected_font = {320: 28, 390: 28, 768: 42, 1280: 39.68}[width]
                for item in metrics:
                    assert abs(item["font_px"] - expected_font) < 0.02, (width, case, item["font_px"])
                    assert item["weight"] == metrics[0]["weight"] and item["spacing"] == metrics[0]["spacing"]
                    assert len(item["number_rects"]) == 1, (width, case, "number wrapped")
                    assert item["text_rects"]
                    for rect in item["text_rects"]:
                        assert rect["left"] >= item["value"]["left"] - 1 and rect["right"] <= item["value"]["right"] + 1, (width, case, "text clipped")
                        # A font's em-box can exceed line-height with overflow:visible.
                        # Check real clipping boundaries and neighbouring elements.
                        assert rect["top"] >= item["name"]["bottom"] - 1, (width, case, "text/name overlap")
                        assert rect["bottom"] <= item["icon"]["top"] + 1, (width, case, "text/icon overlap")
                        assert rect["top"] >= item["card"]["top"] - 1 and rect["bottom"] <= item["card"]["bottom"] + 1, (width, case, "text outside card")
                        for clip in item["clipping"]:
                            if clip["x"]:
                                assert rect["left"] >= clip["bounds"]["left"] - 1 and rect["right"] <= clip["bounds"]["right"] + 1, (width, case, "text clipped horizontally", clip["element"])
                            if clip["y"]:
                                assert rect["top"] >= clip["bounds"]["top"] - 1 and rect["bottom"] <= clip["bounds"]["bottom"] + 1, (width, case, "text clipped vertically", clip["element"])
                    assert item["name"]["bottom"] <= item["value"]["top"] + 1
                    assert item["value"]["bottom"] <= item["icon"]["top"] + 1, (width, case, "value/icon overlap")
                    assert item["icon"]["bottom"] <= item["status"]["top"] + 1, (width, case, "icon/status overlap")
                    for part in ("value", "icon", "status"):
                        assert item[part]["left"] >= item["card"]["left"] - 1 and item[part]["right"] <= item["card"]["right"] + 1, (width, case, part)
                        assert item[part]["bottom"] <= item["card"]["bottom"] + 1, (width, case, part)
                assert page.evaluate("document.documentElement.scrollWidth <= innerWidth"), (width, case)
                result["home_temperature_checks"].append({
                    "width": width, "case": case, "values": values,
                    "font_px": [item["font_px"] for item in metrics],
                    "text_lines": [len(item["text_rects"]) for item in metrics],
                    "number_single_line": True, "overlap_or_clipping": False,
                })

            pin_draft = {"cfg-channel": "5", "cfg-type": "3", "cfg-gpio": "33", "cfg-debounce": "275"}

            def assert_pin_draft():
                for name, value in pin_draft.items():
                    expect(page.locator("#" + name)).to_have_value(value)

            def expect_pin(gpio, state_name, label):
                pin = page.locator(f'#jp1-map [data-gpio="{gpio}"]')
                expect(pin).to_have_attribute("data-state", state_name)
                expect(pin.locator(".jp1-status")).to_have_text(label)

            def pinout_geometry(width, label):
                metrics = page.locator("#jp1-map").evaluate("""map => {
                    const box=r=>({left:r.left,right:r.right,top:r.top,bottom:r.bottom,width:r.width,height:r.height});
                    return {rows:Array.from(map.children,row=>Array.from(row.children,pin=>{
                        const parts=Array.from(pin.children,part=>{const range=document.createRange();range.selectNodeContents(part);
                            return {text:part.textContent,rects:Array.from(range.getClientRects(),box)};});
                        return {pin:Number(pin.dataset.pin),gpio:pin.dataset.gpio?Number(pin.dataset.gpio):null,
                            state:pin.dataset.state,box:box(pin.getBoundingClientRect()),parts};
                    }))};
                }""")
                (out / f"sensor-pinout-{label}-{width}-metrics.json").write_text(
                    json.dumps(metrics, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
                assert len(metrics["rows"]) == 13
                pins = [pin for row in metrics["rows"] for pin in row]
                assert [pin["pin"] for pin in pins] == list(range(1, 27))
                assert {pin["gpio"]: pin["pin"] for pin in pins if pin["gpio"] is not None} == sensor_pins
                for row in metrics["rows"]:
                    assert len(row) == 2 and row[0]["box"]["right"] < row[1]["box"]["left"]
                    for pin in row:
                        bounds = pin["box"]
                        assert bounds["width"] > 0 and 0 <= bounds["left"] < bounds["right"] <= width
                        for part in pin["parts"]:
                            for rect in part["rects"]:
                                assert rect["left"] >= bounds["left"] - 1 and rect["right"] <= bounds["right"] + 1, (width, pin, "horizontal clipping")
                                assert rect["top"] >= bounds["top"] - 1 and rect["bottom"] <= bounds["bottom"] + 1, (width, pin, "vertical clipping")
                assert page.evaluate("document.documentElement.scrollWidth <= innerWidth"), width
                expect(page.locator('#jp1-map button, #jp1-map input, #jp1-map select, #jp1-map [data-action]')).to_have_count(0)
                expect(page.locator('#jp1-map [data-state="fixed"]')).to_have_count(15)
                expect(page.locator('#jp1-map [data-pin="15"] .jp1-status')).to_have_text("BOOT · резерв")
                return metrics

            def sensor_pinout(width):
                saved = empty_connections(width)
                saved["items"][0] = local_assignment(0, "ds18b20", 52)
                saved["items"][4] = local_assignment(4, "reed_nc", 28)
                saved["items"][2]["type"] = "mqtt"
                saved["items"][5].update(type="modbus", address=1)
                with guard:
                    fixture["connections"] = saved
                page.click('.topnav [data-page="settings"]')
                page.click('#settings-menu [data-page="connections"]')
                expect(page.locator("#connections")).to_be_visible()
                wait_polls()
                expect_pin(52, "busy", "Занят · В доме")
                expect_pin(28, "busy", "Занят · Нижний геркон")
                expect(page.locator('#jp1-map [data-state="free"]')).to_have_count(9)
                original_pins = page.locator("#jp1-map .jp1-pin").element_handles()
                for name in ("cfg-channel", "cfg-type", "cfg-gpio"):
                    page.locator("#" + name).select_option(pin_draft[name])
                page.locator("#cfg-debounce").fill(pin_draft["cfg-debounce"])
                # Selecting a GPIO in the form does not reserve it until saved.
                expect_pin(33, "free", "Свободен")
                page.locator('#jp1-map [data-gpio="33"]').click()
                assert_pin_draft()
                map_metrics = pinout_geometry(width, "map")
                page.screenshot(path=str(out / f"sensor-pinout-map-{width}.png"), full_page=True)
                photo = page.locator("#jp1-photo")
                expect(photo).to_have_js_property("open", False)
                photo.locator("summary").click()
                picture = page.locator("#jp1-pinout")
                link = page.locator("#jp1-pinout-link")
                picture.scroll_into_view_if_needed()
                page.wait_for_function("() => {const image=document.getElementById('jp1-pinout');return image.complete&&image.naturalWidth===800&&image.naturalHeight===800;}")
                original_picture = picture.element_handle()
                original_link = link.element_handle()
                response = pinout_responses[-1]
                assert response.status == 200 and response.headers["content-type"] == "image/jpeg"
                assert sha256(response.body()).hexdigest() == pinout_sha256
                metrics = picture.evaluate("""image => {
                    const rect=image.getBoundingClientRect(),card=image.closest('.card').getBoundingClientRect();
                    return {left:rect.left,right:rect.right,width:rect.width,height:rect.height,
                            card_left:card.left,card_right:card.right,natural_width:image.naturalWidth,
                            natural_height:image.naturalHeight};
                }""")
                assert 0 < metrics["width"] <= 800 and abs(metrics["width"] - metrics["height"]) < 0.5
                assert metrics["left"] >= metrics["card_left"] and metrics["right"] <= metrics["card_right"]
                assert page.evaluate("document.documentElement.scrollWidth <= innerWidth"), width
                # Native Tab from the disclosure summary reaches the original-image link.
                expect(photo.locator("summary")).to_be_focused()
                page.keyboard.press("Tab")
                expect(link).to_be_focused()
                expect(link).to_have_css("outline-style", "solid")
                expect(link).to_have_css("outline-width", "2px")
                with guard:
                    requests_before_poll = state["pinout_requests"]
                wait_polls()
                expect(link).to_be_focused()
                expect(photo).to_have_js_property("open", True)
                with guard:
                    assert state["pinout_requests"] == requests_before_poll, "Polling fetched the image again"
                assert_pin_draft()
                expect_pin(33, "free", "Свободен")
                assert all(handle.evaluate("node => node.isConnected") for handle in original_pins)
                assert original_picture.evaluate("node => node===document.getElementById('jp1-pinout')")
                assert original_link.evaluate("node => node===document.getElementById('jp1-pinout-link')")
                page.screenshot(path=str(out / f"sensor-pinout-photo-{width}.png"), full_page=True)
                method = "keyboard" if width in (390, 1280) else "mouse"
                with context.expect_event("response", predicate=lambda response:
                        response.url == base + "/assets/jp1-pinout.jpg"
                        and response.request.is_navigation_request()) as original_response:
                    with context.expect_page() as opened:
                        if method == "keyboard":
                            page.keyboard.press("Enter")
                        else:
                            link.click()
                popup = opened.value
                popup.wait_for_load_state()
                assert popup.url == base + "/assets/jp1-pinout.jpg"
                popup.wait_for_function("() => {const image=document.querySelector('img');return image&&image.complete&&image.naturalWidth===800&&image.naturalHeight===800;}")
                popup_response = original_response.value
                assert popup_response.status == 200 and popup_response.headers["content-type"] == "image/jpeg"
                assert sha256(popup_response.body()).hexdigest() == pinout_sha256
                popup.screenshot(path=str(out / f"sensor-pinout-original-{width}.png"))
                popup.close()
                expect(page.locator("#connections")).to_be_visible()
                assert_pin_draft()
                assert original_picture.evaluate("node => node===document.getElementById('jp1-pinout')")
                photo.locator("summary").click()
                expect(photo).to_have_js_property("open", False)
                result["pinout_checks"].append({"width": width, "map": map_metrics, "image": metrics, "open_method": method,
                    "original_sha256": pinout_sha256, "draft_preserved": True, "draft_does_not_reserve_gpio": True,
                    "keyboard_focus_visible": True, "poll_image_reloads": 0, "overlap_or_clipping": False})

            def pinout_transitions():
                page.click('.topnav [data-page="settings"]')
                page.click('#settings-menu [data-page="connections"]')
                original_pins = page.locator("#jp1-map .jp1-pin").element_handles()
                with guard:
                    requests_before = state["pinout_requests"]

                def record(name, expected):
                    for state_name in ("free", "busy", "unknown", "conflict"):
                        expect(page.locator(f'#jp1-map [data-kind="gpio"][data-state="{state_name}"]')).to_have_count(expected.get(state_name, 0))
                    assert_pin_draft()
                    assert all(handle.evaluate("node => node.isConnected") for handle in original_pins)
                    result["pinout_transitions"].append({"name": name, "states": page.locator('#jp1-map [data-kind="gpio"]').evaluate_all(
                        "pins=>pins.map(pin=>({gpio:Number(pin.dataset.gpio),state:pin.dataset.state,text:pin.querySelector('.jp1-status').textContent}))"), "draft_preserved": True})

                def change(name, config, expected):
                    with guard:
                        if config is None:
                            fixture.pop("connections", None)
                        else:
                            fixture["connections"] = deepcopy(config)
                    wait_polls()
                    record(name, expected)

                change("empty-saved-configuration", empty_connections(1000), {"free": 11})
                assigned = empty_connections(1001)
                assigned["items"][0] = local_assignment(0, "ds18b20", 52)
                change("saved-sensor-offline-still-occupied", assigned, {"free": 10, "busy": 1})
                expect_pin(52, "busy", "Занят · В доме")
                moved = empty_connections(1002)
                moved["items"][0] = local_assignment(0, "ds18b20", 33)
                change("saved-reassignment", moved, {"free": 10, "busy": 1})
                expect_pin(52, "free", "Свободен")
                expect_pin(33, "busy", "Занят · В доме")
                change("saved-disconnection", empty_connections(1003), {"free": 11})
                unavailable = dict(assigned, available=False)
                change("snapshot-unavailable", unavailable, {"unknown": 11})
                for width in (320, 390, 1280):
                    page.set_viewport_size({"width": width, "height": 900})
                    pinout_geometry(width, "unknown")
                    page.screenshot(path=str(out / f"sensor-pinout-unknown-{width}.png"), full_page=True)
                partial = empty_connections(1004)
                partial["items"].pop()
                change("partial-snapshot", partial, {"unknown": 11})
                malformed = empty_connections(1005)
                malformed["items"][0] = None
                change("null-assignment", malformed, {"unknown": 11})
                invalid = deepcopy(assigned)
                invalid["items"][0].update(gpio=35, jp1_pin=15)
                change("reserved-gpio-in-invalid-snapshot", invalid, {"unknown": 11})
                expect(page.locator('#jp1-map [data-pin="15"] .jp1-status')).to_have_text("BOOT · резерв")
                duplicate = deepcopy(assigned)
                duplicate["items"][1] = local_assignment(1, "ds18b20", 52)
                change("duplicate-gpio-conflict", duplicate, {"conflict": 1, "unknown": 10})
                expect_pin(52, "conflict", "Конфликт · В доме / На улице")
                page.screenshot(path=str(out / "sensor-pinout-conflict-1280.png"), full_page=True)
                change("missing-connections", None, {"unknown": 11})
                change("valid-snapshot-recovery", assigned, {"free": 10, "busy": 1})
                with guard:
                    state["status_code"] = 503
                expect(page.locator("#jp1-state")).to_contain_text("Нет связи", timeout=10000)
                record("transport-offline", {"unknown": 11})
                for gpio in sensor_pins:
                    expect_pin(gpio, "unknown", "Нет связи")
                page.screenshot(path=str(out / "sensor-pinout-offline-1280.png"), full_page=True)
                with guard:
                    state["status_code"] = 200
                wait_polls()
                record("transport-recovery", {"free": 10, "busy": 1})
                expect_pin(52, "busy", "Занят · В доме")
                with guard:
                    assert state["pinout_requests"] == requests_before, "State changes fetched the image again"

            for width in (320, 390, 768, 1280):
                page.set_viewport_size({"width": width, "height": 900})
                connected = width in (320, 768)
                with guard:
                    fixture["equipment"] = deepcopy(inventory)
                    fixture["equipment"][1]["connected"] = connected
                    fixture["equipment"][2].update(name="Контур 1 · гостевая", connected=True)
                    fixture["temperatures"] = [{"valid": False, "ever": False, "history": []} for _ in range(4)]
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
                page.click('.topnav [data-page="home"]')
                home_temperatures(width, "no-data", [None] * 4)
                for case, values in (("negative", [-55.0] * 4), ("three-digit", [125.0] * 4),
                                     ("mixed", [-55.0, 0.0, 125.0, 34.8])):
                    with guard:
                        fixture["temperatures"] = [
                            {"valid": True, "ever": True, "value": value, "minimum": value,
                             "maximum": value, "age_s": 0, "history": []} for value in values
                        ]
                    wait_polls()
                    home_temperatures(width, case, values)
                sensor_pinout(width)
                page.click('.topnav [data-page="equipment"]')
                same_cards()
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
            pinout_transitions()
            result.update(result="PASS", coverage=[
                "actual embedded HTML and three-second HTTP polls",
                "mouse opens and closes details", "native Tab/Shift+Tab/Enter/Space",
                "four independently expanded cards over multiple polls at each width",
                "user close survives polling", "details and summary DOM identity retained",
                "keyboard focus retained", "live connected states and circuit names refreshed",
                "null entries and missing tail preserve stable API types and unknown data",
                "restored inventory refreshes values", "page navigation retains user disclosure",
                "four responsive widths and minimum 44px summary targets",
                "four equal Home temperature font sizes without reducing the original large sizes",
                "no-data, negative, three-digit and mixed readings on all four widths",
                "single-line numbers, wrapping units when necessary, no text clipping or icon overlap",
                "supplied JP1 JPEG served from the actual embedded bytes without external requests",
                "sensor pinout decoded at 800x800 and fits all four widths without cropping",
                "26-contact JP1 schematic matches the firmware pin profile and fits all four widths",
                "saved assignment and reassignment reserve GPIO even when the sensor is offline",
                "draft choices do not reserve GPIO; disconnection releases only after a valid snapshot",
                "unavailable, partial, missing, invalid and duplicate snapshots never claim free contacts",
                "transport loss clears occupancy certainty and a fresh snapshot restores it",
                "mouse and keyboard open the original image in a separate tab with visible focus",
                "unsaved sensor selections survive status polling and closing the original image",
                "status polling retains image/link DOM and does not reload the JPEG",
            ])
        assert not result["page_errors"], result["page_errors"]
        assert not state["mutation_requests"], state["mutation_requests"]
        assert not result["external_requests"], result["external_requests"]
        browser.close()
except BaseException as error:
    result["result"] = "FAIL"
    result["failure"] = str(error)
    raise
finally:
    with guard:
        result["status_polls"] = state["status_polls"]
        result["mutation_requests"] = deepcopy(state["mutation_requests"])
        result["pinout_requests"] = state["pinout_requests"]
    (out / "browser-result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))
    server.shutdown()
