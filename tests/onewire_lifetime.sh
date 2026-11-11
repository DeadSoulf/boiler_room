#!/usr/bin/env bash
set -euo pipefail
driver_dir=${1:?Path to the prepared OneWire32 library is required}
test_dir=$(cd "$(dirname "$0")" && pwd)
stub_dir=$(mktemp -d)
trap 'rm -rf "$stub_dir"' EXIT
mkdir -p "$stub_dir/freertos" "$stub_dir/driver"
for header in Arduino.h sdkconfig.h freertos/{FreeRTOS,queue,task}.h driver/{rmt_tx,rmt_rx}.h; do
  touch "$stub_dir/$header"
done
"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter \
  -I"$stub_dir" -I"$driver_dir" "$test_dir/onewire_lifetime.cpp" -o "$stub_dir/test"
"$stub_dir/test"
echo "PASS: partial OneWire32 construction and pending TX cleanup on the pinned driver"
