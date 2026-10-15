#!/bin/sh
# Builds Kernel33.dll (32-bit) with MSYS2 mingw32 and copies it next to the game exe.
# unpacked.z (embedded as a resource) comes from `python make_exe.py`, which needs C1.exe.
set -e
cd "$(dirname "$0")"
BIN=/c/msys64/mingw32/bin
[ -f unpacked.z ] || { echo "unpacked.z missing: run python make_exe.py first"; exit 1; }
[ -f /c/msys64/mingw32/lib/libopus.a ] || { echo "libopus.a missing: pacman -S mingw-w64-i686-opus"; exit 1; }
"$BIN/windres.exe" -O coff -i kernel33.rc -o kernel33_res.o
"$BIN/gcc.exe" -O2 -Wall -shared -s -static -static-libgcc -o Kernel33.dll kernel33.c kernel33_res.o kernel33.def \
    -lopus -lz -limm32 -lgdi32 -lshlwapi -lole32 -lwinmm -Wl,--enable-stdcall-fixup -Wl,--dynamicbase -Wl,--nxcompat
cp Kernel33.dll ../Kernel33.dll
echo "built Kernel33.dll"
