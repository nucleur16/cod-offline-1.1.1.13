#!/bin/sh
# Builds the feat/voicechat DLL (dev console + Opus voice chat) as Kernel33_voice.dll (32-bit, MSYS2
# mingw32, libopus linked statically: pacman -S mingw-w64-i686-opus) and leaves ../Kernel33.dll, the
# live client DLL, alone. Install it by hand:
#   cp ../Kernel33.dll ../Kernel33.dll.pre-voice && cp Kernel33_voice.dll ../Kernel33.dll
# It also links a variant without the developer console (-DK33_NO_CONSOLE, Kernel33_noconsole.dll) and
# fails when that does not build: voice.c must only need c1game.c.
set -e
cd "$(dirname "$0")"
BIN=/c/msys64/mingw32/bin
[ -f unpacked.z ] || { echo "unpacked.z missing: run python make_exe.py first"; exit 1; }
[ -f c1_dvars.h ] || { echo "c1_dvars.h missing: run python gen_dvars.py first"; exit 1; }
[ -f /c/msys64/mingw32/lib/libopus.a ] || { echo "libopus.a missing: pacman -S mingw-w64-i686-opus"; exit 1; }
"$BIN/windres.exe" -O coff -i kernel33.rc -o kernel33_res.o
CFLAGS="-O2 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -Wno-missing-field-initializers -shared -s -static -static-libgcc"
LIBS="-lopus -lz -limm32 -lgdi32 -lshlwapi -lole32 -lwinmm -Wl,--enable-stdcall-fixup -Wl,--dynamicbase -Wl,--nxcompat"
"$BIN/gcc.exe" $CFLAGS -o Kernel33_voice.dll kernel33.c kernel33_res.o kernel33.def $LIBS
"$BIN/gcc.exe" $CFLAGS -DK33_NO_CONSOLE -o Kernel33_noconsole.dll kernel33.c kernel33_res.o kernel33.def $LIBS \
    || { echo "FAILED: the build without console.c (-DK33_NO_CONSOLE) does not link"; exit 1; }
echo "built Kernel33_voice.dll and Kernel33_noconsole.dll (not installed)"
