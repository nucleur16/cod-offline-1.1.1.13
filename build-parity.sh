#!/bin/sh
# Build the same complete dependency closure without changing an installed DLL.
set -e
cd "$(dirname "$0")"
K33_NO_COPY=1 sh ./build.sh
cp Kernel33.dll Kernel33_parity.dll
echo "built Kernel33_parity.dll (not installed)"
