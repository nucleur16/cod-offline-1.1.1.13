#!/bin/sh
# Every Kernel33 source test, on Windows (MSYS2 mingw32; the Quick fixture is an x86 Windows program):
#   sh tests/run_tests.sh
# CI (.github/workflows/shared-ui-baseline.yml) runs the first two on Linux.
set -e
cd "$(dirname "$0")/.."
BIN=${K33_MINGW:-/c/msys64/mingw32/bin}
OUT=${TMP:-/tmp}/k33-tests
mkdir -p "$OUT"
python tests/test_ui_baseline.py
python tests/test_quick_ready_gate.py
"$BIN/gcc.exe" -Wall -Wextra -Werror tests/patch_locale_test.c -o "$OUT/patch_locale_test.exe"
"$OUT/patch_locale_test.exe"
# managed_quick_windows.c needs its .qa section at 0x700000, where C1's room code is: as the first section of an
# image based at 0x6F0000 with 64 KB section alignment (.init is placed first in .text).
"$BIN/gcc.exe" -O0 -Wall -Wextra -I. -c tests/managed_quick_windows.c -o "$OUT/managed_quick.o"
"$BIN/objcopy.exe" --rename-section .qa=.init "$OUT/managed_quick.o" "$OUT/managed_quick_init.o"
"$BIN/gcc.exe" -s -o "$OUT/managed_quick_windows.exe" "$OUT/managed_quick_init.o" \
    -Wl,--image-base=0x6f0000 -Wl,--section-alignment=0x10000 -Wl,--disable-dynamicbase
"$OUT/managed_quick_windows.exe"
echo "all Kernel33 source tests passed"
