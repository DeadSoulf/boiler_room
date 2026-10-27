# Build only — no flashing

Merge these files into repository root alongside current src/, tests/ and
apply_update.py. tests/update_fixture/main.cpp is the recovered historical updater
test fixture only: it is NEVER copied into firmware. Font fixture is copied from
current src/boiler_fonts.c during host testing.

GitHub-hosted Ubuntu runs native tests, then firmware compilation. Permissions
contents:read; no secrets, self-hosted runners, upload/flash commands or C6 changes.
Artifacts inherit repository visibility (PUBLIC for a public repository), expire
in 14 days, and contain test images, logs, hashes and firmware. No credentials
should be placed in sources or test fixtures.

Pinned inputs verified through GitHub connector earlier in this task:
- ultramcu/guition-jc4880p4-bsp 324970bade0d1f4e52880fe8016580368bc1e06e
- LVGL8.3.11 native commit 74d0a816a440eea53e030c4f1af842a94f7ce3d3;
  firmware exact registry version lvgl@8.3.11
- pioarduino55.03.36-1 release archive SHA256
  837ff736b9a088186ecb99a96b830a1eaccc7f16bb0ee6c73c4e76c2ef3d936e
- official checkout/upload-artifact v4 immutable action commits in workflow
- PlatformIO core6.1.18; transitive Python dependencies resolved during install.
This is not a fully hermetic or bit-for-bit reproducibility claim.

Guarded portrait transform only; never runs the historical app installer. Board
jc4880p4 / esp32p4_es, portrait480x800. User-requested overrides CPU360MHz and
CORE_DEBUG_LEVEL0 are explicit. All other original BSP settings preserved except
LVGL exact pin and hash-checked local platform archive URI. Firmware artifacts
include ELF/BIN, bootloader and partitions; offline checks report minimum image
revision but do not establish actual board compatibility or hardware behavior.
