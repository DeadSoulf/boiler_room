# Boiler Room Touch Panel — v0.10.11

## v0.10.11 — Fuel tank and outlet temperatures

The **Fuel** card on the panel and in the browser shows two readings, **In tank** and **At outlet**. Select a row to see that sensor’s minimum, maximum and history. The canister displays the fuel level from the reed switches. [Panel home screen with test readings](docs/verification/assets/v0.10.11-fuel-card.png).

The previous fuel temperature input keeps its assignment as the tank sensor. To add the second DS18B20, open **Settings → Sensors** in the paired browser, select its GPIO, assign the fuel outlet temperature and save.

## v0.10.10 — One coolant card for supply and return

The panel and browser home screens show **Supply** and **Return** together in the **Coolant** card. Select either row to open that sensor’s reading, minimum, maximum and history. [Home screen with test readings](docs/verification/assets/v0.10.10-coolant-card.png).

## v0.10.9 — Coolant supply and return

**Supply** and **Return** appear separately on the home screen and in the paired browser. Tap either reading for its own history. On upgrade, the former coolant input becomes supply, with its GPIO and Home Assistant identity preserved. To add return, open **Settings → Sensors** in the paired browser, select the GPIO wired to the second DS18B20 and assign **Return**. Until a sensor reports, the panel shows that reading as unavailable. Tank water temperature is a separate measurement.

[Home screen with test readings](docs/verification/assets/v0.10.9-coolant-home.png).

## v0.10.8: Indirect hot-water tank thresholds

Open **Equipment → Hot-water tank** on the panel or in a paired browser. Enter the lower switch-on and upper switch-off temperature, then save both together. They are stored on the panel and survive a restart. Neither value is preset. The heat button remains unavailable until the dedicated water sensor and output controller are connected and verified. Saving a pair does not operate any equipment or queue a future start.

## v0.10.7: saved Wi-Fi networks

The panel remembers up to eight networks and tries other saved profiles if a connection is lost. Open **Settings → Connections → Wi-Fi** on the panel, or **Settings → Wi-Fi** in a paired browser. Scan, then tap a network marked **Saved** to connect without re-entering its password. Unknown protected networks still need a password. A manual disconnect pauses automatic connections without erasing any profiles.

An existing v0.10.6 credential works after upgrading. Downgrading after changing profiles may require entering the password on the older firmware. [Verification and migration notes](docs/verification/wifi-profiles-2026-11-11.md).

## v0.10.6: assign sensors by selecting a pin

In web **Settings → Sensors**, select any of the 11 usable JP1 GPIOs to open its action menu. Choose a temperature channel or fuel-level reed input, review the affected assignments, then save. Occupied pins can be reassigned or released. A new reed input requires an explicit NO/NC choice. Moving a channel and releasing its former GPIO is one verified configuration change.

The dialog keeps your draft during status polls and refuses to overwrite a configuration changed in another window. Lost connection, expired authorization and an update in progress block writes. A completed command is confirmed by reading the saved configuration again. The existing source configuration form, including remote descriptors, remains under **Additional source settings**. All sensor and GPIO configuration stays in the web interface; the physical panel menu is unchanged.

This version includes the 6 November reliability fixes, the Home date and equal-size temperature readings, and previous OTA/equipment UI corrections. Publication and hardware acceptance are tracked separately in the [v0.10.6 verification report](docs/verification/v0.10.6-pin-actions-2026-11-06.md). Final builds and browser tests run only in GitHub Actions; do not use the user’s laptop. Release preparation uses a single explicit version-increasing release commit on `main`, without a development or release branch. The last accepted installation remains v0.10.1 until real OTA acceptance is recorded.

## v0.10.5: live connector diagram in web sensor settings

**Settings → Sensors** shows a schematic of all 26 JP1 contacts beside GPIO selection. Each usable GPIO shows whether it is free or assigned, with the assigned sensor channel named. Occupancy comes from saved panel settings and remains shown when a sensor is not responding. Unknown configuration or lost connection is labelled explicitly. Power, ground, BOOT, C6 and I2C contacts retain their fixed roles.

The supplied connector photograph remains available below the diagram as a reference and opens in another tab. Its exact JPEG is embedded in the firmware and served by the panel without Internet or microSD. The diagram updates without changing the settings form or assigning any GPIO.

**[v0.10.5 is published for GitHub OTA](https://github.com/DeadSoulf/boiler_room/releases/tag/v0.10.5).** The final release browser run passed 13 state-transition scenarios; diagram and inline-photo geometry was checked at main-page widths 320/390/768/1280. All 26 contacts and 11 assignable GPIOs were checked; the exact supplied JPEG occurs once in the target BIN. The final GitHub native run passed 408 layout checks with zero clipping in each rendering mode. All four public assets were downloaded without authentication and hash-checked. [Verification report](docs/verification/v0.10.5-sensor-pinout-2026-11-05.md).

Select **Settings → Service → Update → Check**, then confirm **v0.10.5** on the panel or in the paired browser. No intermediate v0.10.4 installation is required. Installation, live panel HTTP and the physical display remain unverified: Asus was **Offline** at `2026-11-05T11:21:21Z`; the last accepted installed version is v0.10.1.

Browser captures: [390 px](docs/verification/assets/v0.10.5-jp1-390.png) · [1280 px](docs/verification/assets/v0.10.5-jp1-1280.png).

## v0.10.4: date below the Home clock

The physical Home header shows the local date as **DD.MM.YYYY** below the time, inside the existing area at the right. The six-digit pairing code keeps its separate area to the left. Both lines use the same current time and configured timezone; an unset clock shows placeholders. Other pages retain their Home button.

**[v0.10.4 is published for GitHub OTA](https://github.com/DeadSoulf/boiler_room/releases/tag/v0.10.4).** Native LVGL and independent checks passed 408 layout checks with zero clipping in both rendering modes. The final OTA/browser/ESP32-P4 release workflow passed; all four public assets were downloaded and hash-checked. [Verification report](docs/verification/v0.10.4-home-date-2026-11-05.md).

Installation of v0.10.4 and the physical display remain unverified. Its Home date display is also included in v0.10.5.

## v0.10.3: stable update screen

The physical Update page keeps its existing widgets when Wi-Fi signal information changes. Download text and button state are changed only when their displayed values change, so the unchanged page no longer redraws or restarts disabled-button animations every 100 ms. Progress, cancellation, connection availability and confirmation of a specific version remain live.

**[v0.10.3 is published for GitHub OTA](https://github.com/DeadSoulf/boiler_room/releases/tag/v0.10.3).** Real LVGL 8.3.11 passed 392 checks with zero clipped elements in both partial rendering and full-refresh with two full-screen buffers. The final OTA/browser/native/ESP32-P4 release workflow passed; all four public assets were downloaded without authentication and hash-checked. [Release and verification report](docs/verification/v0.10.3-ota-display-2026-11-05.md).

Installation of v0.10.3 and physical LCD behaviour have not been independently verified. Its redraw correction is also included in v0.10.4. The previous firmware still renders the download of the correction; the new UI behaviour starts after booting a version that contains it.

## v0.10.2: equal Home temperature sizes

All four Home readings use the same large number size on the 480×800 panel and in the embedded web UI. The fuel temperature now matches the other three; its canister remains below the value and unit. Web values retain the existing responsive size at each viewport, with the unit wrapping when needed. Equipment disclosure state from v0.10.1 is preserved.

[v0.10.2 was published for GitHub OTA](https://github.com/DeadSoulf/boiler_room/releases/tag/v0.10.2) with successful final build, native LVGL, browser and dedicated OTA/target checks. Its installation was not independently confirmed before the v0.10.3 release. The same temperature-size correction is included in v0.10.3. [Historical release report](docs/verification/v0.10.2-ota-candidate-2026-11-05.md).

## v0.10.1: keep equipment details open

Equipment cards retain their expanded or collapsed state and keyboard focus while live status is refreshed. The existing cards are updated in place, so new values still arrive without closing the section being read. No equipment API, GPIO or control behaviour changes.

**v0.10.1 is installed through GitHub OTA and its healthy boot is confirmed.** Three HTTP observations on 5 November 2026, 10:21 Moscow time, show stable operation, no update error and v0.10.0 available for rollback. The live HTML exactly matches the released source. [Network acceptance report](docs/verification/v0.10.1-ota-network-2026-11-05.md) · [Release](https://github.com/DeadSoulf/boiler_room/releases/tag/v0.10.1) · [OTA instructions](docs/github-ota.md) · [installed.json](docs/verification/installed.json).

## v0.10.0: GitHub updates on the panel

The new update page discovers stable releases, offers a specific version for confirmation and shows download/verification/install progress. The same workflow is available in the paired embedded web UI. The image goes to the inactive Flash slot; SHA-256, size, board/revision, partition layout and the rollback-enabled bootloader are checked before boot selection. A healthy first start is required to retain the new application. The inserted microSD records update events through the existing log service; no image staging or formatting is required.

[Using GitHub OTA and preparing releases](docs/github-ota.md). **v0.10.0 was installed and verified on 5 November 2026.** The first OTA-capable version was installed through the checked USB package. Presence wake remains a separate pending task.

## v0.9.1: home temperature and matching icons

Channel 0 is now labelled **«В доме»** (home temperature). House, sun/cloud and radiator line icons match the existing fuel canister, without shrinking the large temperature readings. Native LVGL and embedded web UI are updated together; MQTT retains the `room` key and unique IDs. [Screenshots and checks](docs/home-icons-v091/README.md).

The earlier **v0.9.1** installation on 5 November 2026 introduced these home-screen changes. Its historical hardware report is retained; GitHub updates are now implemented in v0.10.0.

Standalone GUITION JC4880P443C_I_W / ESP32-P4 + ESP32-C6, 480×800 LVGL panel. [Russian documentation](README_RU.md).

## v0.9.0

The home fuel tile contains a canister with a small temperature above it. The separate bottom fuel-level button is removed. Unknown/stale inputs do not appear as a full tank. With the configured NC mapping: both contacts open = full; upper closed only = half; both closed = empty with an exclamation mark.

Pairing now starts from the web **Pair** button. A one-use six-digit code appears on the physical panel, to the left of the clock. A successful submission, cancellation or a two-minute timeout clears the code. The display wakes for the request. The physical Web access menu is removed. Codes are never returned by HTTP/MQTT; session access lasts one hour. Requests are origin-checked, ticket-bound and rate-limited; failed guesses are bounded.

The self-contained web UI has four primary sections and nested settings, grouped equipment, a canister, collapsible diagnostics and retained safety confirmations. No external fonts/CDN or cloud dependency. HTTP and MQTT are **unencrypted trusted-LAN-only** in this version.

## Installation and release discipline

**Last verified installation: v0.10.1, delivered through GitHub OTA.** Wi-Fi, touch readiness, microSD logging, settings and the exact embedded web page are confirmed over HTTP. The device validates the written image and boot metadata internally; this acceptance did not use an independent external Flash read. [Network report](docs/verification/v0.10.1-ota-network-2026-11-05.md) · [installed.json](docs/verification/installed.json). The earlier USB installation and full read-back of v0.10.0 remain a [historical report](docs/verification/v0.10.0-hardware-2026-11-05.md). Physical power-loss, visual/touch checks, the exact user confirmation action and a real MQTT broker were not remotely verified. Future updates use GitHub OTA; USB initial provisioning or recovery requires a separate explicit request.

Every requested successful device update must also publish its source into **main**, a matching version tag, a GitHub Release with the checked installation ZIP and SHA-256, updated README/CHANGELOG and handoff. Documentation-only updates while main stays on older firmware do not satisfy this rule. [Release process](docs/RELEASE_PROCESS_RU.md).

[Pairing and design](docs/web-v090/README.md) · [Home Assistant](docs/home-assistant.md) · [safe COM3 flash](docs/flash-panel.md) · [handoff](docs/CHAT_HANDOFF_RU.md) · [AGENTS](AGENTS.md).

No physical actuator commands are implemented. Real sensors are still unconnected/unverified. MQTT sensor-source import and Modbus adapters are not active. General CI was repaired and passed with the [6 November reliability changes](docs/verification/reliability-2026-11-06.md); results for this version are recorded separately.
