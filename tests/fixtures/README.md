# Exact updater fixtures (not firmware payload)

- `v03/`: recovered BoilerPanel_Swipe_v0_3_0 application.
- `v031/`: recovered **user-installed** BoilerPanel_Screen_v0_3_1 application.
  Original recovered ZIP SHA256:
  `52bacda35839ef447f3ded73ce3586adb03ac4c296d571fdd40e357dd6a781db`.
- `v04_early/`: previously published pre-reconciliation v0.4 application; not
  asserted installed by the user. Its display_idle.h is retired only for this
  complete exact set, with backup.
- `v04_before_fuel/`: exact `main.cpp`, `panel_types.h` and `panel_radio.cpp` from
  `DeadSoulf/boiler_room` commit `b700ef6f837f5fc1dbd0bad91a569159123c3851`,
  before adding the two reed-switch fuel-level inputs. The remaining four
  managed files are unchanged and reused from `src/` when assembling this
  fixture; their pinned hashes are checked before use. This snapshot requires
  an already-patched `screen031.c` or `ci_screen031.c` adapter, preserved as-is.
- `v04_fuel/`: all seven managed source files from
  `DeadSoulf/boiler_room` commit `339073c935b75a925f8356f50b26175e204beb0d`,
  before the audited UI/radio fixes. Their bytes match the GitHub snapshot.
  This complete baseline is accepted for upgrade and requires an already-patched
  screen adapter. It is never copied into the firmware payload.
- `v04_audited/`: all seven managed source files from
  `DeadSoulf/boiler_room` commit `e09980a21f55565d1c0ba5174eeb35be96143997`.
  This source set is unchanged from the firmware validated at `73639bd` and
  precedes the native equipment visualization. The v0.5 updater accepts only
  this complete exact set with its already-patched screen adapter, then adds
  `equipment_demo.h`. Restore removes that added file and restores the original
  source bytes. This fixture is not firmware payload.
- `v01/`: historical negative test, not an accepted direct upgrade source.
- `bsp/`: upstream ultramcu/guition-jc4880p4-bsp at commit
  `324970bade0d1f4e52880fe8016580368bc1e06e`, retrieved via GitHub connector.
  lvgl_glue.c, platformio.ini, board_p4.h, jc4880p4.json are exact upstream bytes.
  portrait.c is the recovered v0.1 installer transform of pinned lvgl_glue.c.
  ci_portrait.c is the existing CI equivalent (one comment differs).
  screen031.c / ci_screen031.c are the respective outputs of the recovered
  v0.3.1 installer's guarded patch_glue; exact output hashes are pinned in updater.
  screen031.ini is upstream config transformed by recovered v0.3.1 patch_ini
  to CPU360/debug0, used only as a test fixture. The new updater NEVER writes ini.

Hashes in apply_update.py use UTF-8 without BOM and LF line endings. Source
recognition requires a complete exact version set, never independent mixed files.
Tests copy fixtures to temporary directories; no user's BSP or device is touched.
The firmware package uses only src/, never these historical fixture files.
