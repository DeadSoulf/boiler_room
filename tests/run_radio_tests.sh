#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
bin=$(mktemp /tmp/boiler-radio-test.XXXXXX)
trap 'rm -f "$bin"' EXIT HUP INT TERM
${CXX:-g++} -std=c++17 -Wall -Wextra -Werror -I tests/radio_stubs tests/test_radio.cpp -o "$bin"
"$bin"
${CXX:-g++} -std=c++17 -Wall -Wextra -Werror -DCONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE=1 -DCONFIG_NIMBLE_ENABLED=1 -DCONFIG_BT_NIMBLE_ROLE_OBSERVER=1 -I tests/radio_stubs tests/radio_transition_tests.cpp -o "$bin"
"$bin"
for args in '-DCONFIG_ESP_WIFI_REMOTE_ENABLED=0' '-DCONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE=1 -DCONFIG_NIMBLE_ENABLED=1 -DCONFIG_BT_NIMBLE_ROLE_OBSERVER=1' '-DCONFIG_ESP_WIFI_REMOTE_ENABLED=0 -DCONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE=1 -DCONFIG_NIMBLE_ENABLED=1 -DCONFIG_BT_NIMBLE_ROLE_OBSERVER=1'; do
  ${CXX:-g++} -std=c++17 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable $args -I tests/radio_stubs tests/radio_smoke.cpp -o "$bin"
  "$bin"
done
printf '%s\n' 'PASS: no-radio, Wi-Fi+BLE, BLE-only optional branch smoke (host mocks only)'
