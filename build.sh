#!/bin/sh
# sh build.sh <1.1.1.13 install folder>
# Builds everything an install needs into dist/: Kernel33.dll, the patched client exe, the zones and the match
# server's pack. On Windows build.bat runs this in MSYS2; on Linux run it directly (the cross compiler is described
# in scripts/toolchain.inc.sh). Copy the contents of dist/ onto the game folder. See README.md.
#
# The install is only read: its client_shipRetail/chinese stock zones (the build edits some of them) and
# codoMP_client_shipRetail.exe (or .exe.bak), which is copied with its Kernel32.dll import renamed to Kernel33.dll.
set -e
ROOT=$(cd "$(dirname "$0")" && pwd)
cd "$ROOT"
INSTALL=${1:-${K33_GAME:-}}
[ -n "$INSTALL" ] || { echo "usage: sh build.sh <Call of Duty Online 1.1.1.13 folder>" >&2; exit 1; }
EXE="$INSTALL/codoMP_client_shipRetail.exe.bak"
[ -f "$EXE" ] || EXE="$INSTALL/codoMP_client_shipRetail.exe"
[ -f "$EXE" ] && [ -d "$INSTALL/client_shipRetail/chinese" ] || {
    echo "$INSTALL is not a 1.1.1.13 install (no codoMP_client_shipRetail.exe or client_shipRetail/chinese)" >&2; exit 1; }
. scripts/toolchain.inc.sh
$PYTHON -c "import pefile, Crypto, PIL" 2>/dev/null || {
    echo "$PYTHON lacks a module the build needs: $PYTHON -m pip install pefile pycryptodome pillow" >&2; exit 1; }
DIST=$ROOT/dist
rm -rf "$DIST"
mkdir -p "$DIST/main"

echo "=== 1/4 match server pack"
# The GSC ships inside the pack (scripts/), which the host reads through its "fs:scripts" root (src/p2p/p2p.c);
# a loose main/sh1x_rawfiles folder in an install overrides single files of it.
$PYTHON scripts/stage_server_data.py --out "$DIST/main"

echo "=== 2/4 Kernel33.dll"
K33_NO_COPY=1 sh scripts/build.sh
cp build/Kernel33.dll "$DIST/"

echo "=== 3/4 zones"
$PYTHON scripts/build_zones.py --game "$INSTALL" --out "$DIST"

echo "=== 4/4 codoMP_client_shipRetail.exe"
$PYTHON scripts/patch_exe.py "$EXE" "$DIST/codoMP_client_shipRetail.exe"

echo
echo "Done: $DIST"
echo "Copy everything in it onto your 1.1.1.13 game folder and replace the files there."
