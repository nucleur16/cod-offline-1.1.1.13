#!/bin/sh
# Builds the parity-log branch's DLL as Kernel33_parity.dll (32-bit, MSYS2 mingw32) and leaves
# ../Kernel33.dll, the live client DLL, alone. Install it by hand:
#   cp ../Kernel33.dll ../Kernel33.dll.pre-paritylog && cp Kernel33_parity.dll ../Kernel33.dll
set -e
cd "$(dirname "$0")"
BIN=/c/msys64/mingw32/bin
[ -f unpacked.z ] || { echo "unpacked.z missing: run python make_exe.py first"; exit 1; }
"$BIN/windres.exe" -O coff -i kernel33.rc -o kernel33_res.o
"$BIN/gcc.exe" -O2 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -Wno-missing-field-initializers -shared -s -static -static-libgcc \
    -o Kernel33_parity.dll kernel33.c kernel33_res.o kernel33.def \
    -lz -limm32 -lshlwapi -Wl,--enable-stdcall-fixup -Wl,--dynamicbase -Wl,--nxcompat
echo "built Kernel33_parity.dll (not installed)"
