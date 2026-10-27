#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p .build artifacts
checkout() {
 local url=$1 sha=$2 dst=$3
 if [[ ! -d "$dst/.git" ]]; then
  git init "$dst"; git -C "$dst" remote add origin "$url"
  git -C "$dst" fetch --depth=1 origin "$sha"; git -C "$dst" checkout --detach FETCH_HEAD
 fi
 test "$(git -C "$dst" rev-parse HEAD)" = "$sha"
}
case "${1:-}" in
host)
 checkout https://github.com/lvgl/lvgl.git 74d0a816a440eea53e030c4f1af842a94f7ce3d3 .build/lvgl
 g++ -std=c++17 -Wall -Wextra -Isrc tests/model_tests.cpp -o .build/model_tests
 .build/model_tests | tee artifacts/model.log
 bash tests/run_radio_tests.sh | tee artifacts/radio.log
 g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/display_idle_tests.cpp -o .build/display_tests
 .build/display_tests | tee artifacts/display.log
 python3 tests/updater_tests.py 2>&1 | tee artifacts/updater.log
 cmake -S tests/host -B .build/host -DLVGL_DIR="$ROOT/.build/lvgl" -DBOILER_FONTS="$ROOT/src/boiler_fonts.c"
 cmake --build .build/host --parallel 2 2>&1 | tee artifacts/host-build.log
 (cd artifacts && ../.build/host/panel_test) 2>&1 | tee artifacts/host-ui.log
 ;;
firmware)
 checkout https://github.com/ultramcu/guition-jc4880p4-bsp.git 324970bade0d1f4e52880fe8016580368bc1e06e .build/bsp
 curl --fail --location --retry 3 https://github.com/pioarduino/platform-espressif32/releases/download/55.03.36-1/platform-espressif32.zip -o .build/platform-espressif32.zip
 echo '837ff736b9a088186ecb99a96b830a1eaccc7f16bb0ee6c73c4e76c2ef3d936e  .build/platform-espressif32.zip' | sha256sum --check
 python3 -m venv .build/venv
 .build/venv/bin/pip install platformio==6.1.18 2>&1 | tee artifacts/pip.log
 python3 ci/prepare.py
 .build/venv/bin/pio run -d .build/bsp/examples/LvglUi -e jc4880p4 2>&1 | tee artifacts/firmware-build.log
 .build/venv/bin/pio pkg list -d .build/bsp/examples/LvglUi > artifacts/packages.txt
 cp .build/bsp/examples/LvglUi/.pio/build/jc4880p4/{firmware.bin,firmware.elf,bootloader.bin,partitions.bin} artifacts/
 python3 ci/verify_binary.py
 ;;
*) echo 'Usage: ci/bootstrap.sh host|firmware' >&2; exit 2;;
esac
python3 ci/manifest.py
