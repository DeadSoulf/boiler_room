# Boiler Room Touch Panel — v0.10.0

## v0.10.0: GitHub updates on the panel

The new update page discovers stable releases, offers a specific version for confirmation and shows download/verification/install progress. The same workflow is available in the paired embedded web UI. The image goes to the inactive Flash slot; SHA-256, size, board/revision, partition layout and the rollback-enabled bootloader are checked before boot selection. A healthy first start is required to retain the new application. The inserted microSD records update events through the existing log service; no image staging or formatting is required.

[Using GitHub OTA and preparing releases](docs/github-ota.md). **Candidate under verification; the currently verified installed version is still v0.9.1 until the hardware report is updated.** Presence wake remains a separate pending task.

## v0.9.1: home temperature and matching icons

Channel 0 is now labelled **«В доме»** (home temperature). House, sun/cloud and radiator line icons match the existing fuel canister, without shrinking the large temperature readings. Native LVGL and embedded web UI are updated together; MQTT retains the `room` key and unique IDs. [Screenshots and checks](docs/home-icons-v091/README.md).

**v0.9.1 was installed on 5 November 2026.** A new full backup, independent stock-esptool read-back and live HTTP checks confirmed the update. GitHub update discovery and presence wake remain pending.

Standalone GUITION JC4880P443C_I_W / ESP32-P4 + ESP32-C6, 480×800 LVGL panel. [Russian documentation](README_RU.md).

## v0.9.0

The home fuel tile contains a canister with a small temperature above it. The separate bottom fuel-level button is removed. Unknown/stale inputs do not appear as a full tank. With the configured NC mapping: both contacts open = full; upper closed only = half; both closed = empty with an exclamation mark.

Pairing now starts from the web **Pair** button. A one-use six-digit code appears on the physical panel, to the left of the clock. A successful submission, cancellation or a two-minute timeout clears the code. The display wakes for the request. The physical Web access menu is removed. Codes are never returned by HTTP/MQTT; session access lasts one hour. Requests are origin-checked, ticket-bound and rate-limited; failed guesses are bounded.

The self-contained web UI has four primary sections and nested settings, grouped equipment, a canister, collapsible diagnostics and retained safety confirmations. No external fonts/CDN or cloud dependency. HTTP and MQTT are **unencrypted trusted-LAN-only** in this version.

## Installation and release discipline

**v0.9.1 is installed and verified by full Flash read-back and HTTP.** Wi-Fi, microSD and the new embedded web page respond; existing settings are preserved. [Hardware report](docs/verification/v0.9.1-hardware-2026-11-05.md) · [installed.json](docs/verification/installed.json) · [v0.9.1 release](https://github.com/DeadSoulf/boiler_room/releases/tag/v0.9.1). Physical visual/touch checks, real PIN entry by a person and a real MQTT broker were not remotely verified.

Every requested successful device update must also publish its source into **main**, a matching version tag, a GitHub Release with the checked installation ZIP and SHA-256, updated README/CHANGELOG and handoff. Documentation-only updates while main stays on older firmware do not satisfy this rule. [Release process](docs/RELEASE_PROCESS_RU.md).

[Pairing and design](docs/web-v090/README.md) · [Home Assistant](docs/home-assistant.md) · [safe COM3 flash](docs/flash-panel.md) · [handoff](docs/CHAT_HANDOFF_RU.md) · [AGENTS](AGENTS.md).

No physical actuator commands are implemented. Real sensors are still unconnected/unverified. MQTT sensor-source import and Modbus adapters are not active. Legacy general CI issue #15 remains separate from successful dedicated checks.
