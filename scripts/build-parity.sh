#!/bin/sh
# Build the same complete dependency closure without changing an installed DLL.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
K33_NO_COPY=1 sh "$ROOT/scripts/build.sh"
cp "$ROOT/build/Kernel33.dll" "$ROOT/build/Kernel33_parity.dll"
echo "built build/Kernel33_parity.dll (not installed)"
