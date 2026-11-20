#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
inc=(-Isrc)
for area in src/online/*; do [[ ! -d "$area" ]] || inc+=("-I$area"); done
gcc -m32 -O1 -g -DOL_SERVER -D_GNU_SOURCE -fsanitize=address,undefined -fno-sanitize=alignment -fno-omit-frame-pointer -no-pie "${inc[@]}" tests/test_category_patch.c src/online/assets/category_patch.c -o "$out/policy"
"$out/policy"
