#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
UPDATE_TEST_DIR="$(mktemp -d)"
trap 'rm -rf "$UPDATE_TEST_DIR"' EXIT
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc tests/update_policy_tests.cpp -o "$UPDATE_TEST_DIR/policy"
"$UPDATE_TEST_DIR/policy"
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Itests/update_stubs -Isrc tests/update_runtime_tests.cpp -o "$UPDATE_TEST_DIR/runtime"
"$UPDATE_TEST_DIR/runtime"
