#!/usr/bin/env bash
# Link the firmware's actual C++ bridge to its Rust crate built for the host.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
archive="$(realpath "${1:?usage: bash tests/rust_heating_parity.sh path/to/libheating_core.a}")"
cd "$ROOT"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
compiler="${CXX:-c++}"
flags=(-std=c++17 -O2 -Wall -Wextra -Werror -pedantic -UNDEBUG -Isrc
       -fsanitize=undefined -fno-sanitize-recover=undefined)
"$compiler" "${flags[@]}" -DHEATING_TEST_RUST tests/heating_logic_tests.cpp "$archive" -o "$out/expected"
"$out/expected"
"$compiler" "${flags[@]}" tests/rust_heating_parity.cpp "$archive" -o "$out/parity"
"$out/parity"
