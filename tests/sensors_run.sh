#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
${CXX:-c++} -std=c++17 -Wall -Wextra -Werror -Itests/sensors_stubs -Isrc tests/sensors_adapter_tests.cpp -o "$tmp/sensors"
"$tmp/sensors"
