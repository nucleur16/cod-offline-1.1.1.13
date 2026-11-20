#!/usr/bin/env bash
# Production GSC method and captured-state/wire regressions. No game processes.
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
inc=(-Isrc)
for area in src/online/*; do [[ ! -d "$area" ]] || inc+=("-I$area"); done
flags=(-m32 -O1 -g -DOL_SERVER -D_GNU_SOURCE -ffunction-sections -fdata-sections)
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-sanitize=alignment -fno-omit-frame-pointer -no-pie); fi
g++ "${flags[@]}" "${inc[@]}" tests/test_vehicle_compass.cpp src/online/vehicle/vehicle_compass.cpp -Wl,--gc-sections -o "$out/compass"
"$out/compass"
gcc "${flags[@]}" "${inc[@]}" tests/test_minimap_registration.c src/online/script/script_minimap.c -Wl,--gc-sections -o "$out/minimap"
"$out/minimap"
make run-test_cached_snapshot run-test_frame_entities
node tests/survival_helicopter_test.js
