#!/bin/sh
# Builds the dev branch's DLL as Kernel33_dev.dll (32-bit, MSYS2 mingw32): everything build.sh builds
# (controller, voice chat, sound voices, usermaps, parity logger, frame cap) plus the developer console
# (console.c). It leaves ../Kernel33.dll, the live client DLL, alone. Install it by hand:
#   cp ../Kernel33.dll ../Kernel33.dll.pre-dev && cp Kernel33_dev.dll ../Kernel33.dll
# Regenerate the dvar name table first when gen_dvars.py changes: python gen_dvars.py
#
# SDL2 2.32.10 (static libSDL2.a, controller.c) is fetched into third_party/ and checked as in build.sh;
# libopus (voice.c) comes from MSYS2: pacman -S mingw-w64-i686-opus.
# It also links a variant without the developer console (-DK33_NO_CONSOLE, Kernel33_noconsole.dll) and
# fails when that does not build: everything but console.c must only need c1game.c.
set -e
cd "$(dirname "$0")"
BIN=/c/msys64/mingw32/bin
SDL_VER=2.32.10
SDL_SHA256=83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2
SDL_DIR=third_party/SDL2-$SDL_VER/i686-w64-mingw32
[ -f unpacked.z ] || { echo "unpacked.z missing: run python make_exe.py first"; exit 1; }
[ -f c1_dvars.h ] || { echo "c1_dvars.h missing: run python gen_dvars.py first"; exit 1; }
[ -f /c/msys64/mingw32/lib/libopus.a ] || { echo "libopus.a missing: pacman -S mingw-w64-i686-opus"; exit 1; }
if [ ! -f "$SDL_DIR/lib/libSDL2.a" ]; then
    mkdir -p third_party
    TGZ=third_party/SDL2-devel-$SDL_VER-mingw.tar.gz
    curl -sfL -o "$TGZ" "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-devel-$SDL_VER-mingw.tar.gz"
    echo "$SDL_SHA256  $TGZ" | sha256sum -c -
    tar xzf "$TGZ" -C third_party "SDL2-$SDL_VER/i686-w64-mingw32/include" "SDL2-$SDL_VER/i686-w64-mingw32/lib/libSDL2.a"
    rm -f "$TGZ"
fi
"$BIN/windres.exe" -O coff -i kernel33.rc -o kernel33_res.o
CFLAGS="-O2 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -Wno-missing-field-initializers -shared -s -static -static-libgcc -I$SDL_DIR/include"
LIBS="-L$SDL_DIR/lib -lSDL2 -lsetupapi -lcfgmgr32 -lversion -loleaut32 -luuid \
-lopus -lz -limm32 -lgdi32 -lshlwapi -lole32 -lwinmm -lws2_32 -Wl,--enable-stdcall-fixup -Wl,--dynamicbase -Wl,--nxcompat"
"$BIN/gcc.exe" $CFLAGS -o Kernel33_dev.dll kernel33.c kernel33_res.o kernel33.def $LIBS
"$BIN/gcc.exe" $CFLAGS -DK33_NO_CONSOLE -o Kernel33_noconsole.dll kernel33.c kernel33_res.o kernel33.def $LIBS \
    || { echo "FAILED: the build without console.c (-DK33_NO_CONSOLE) does not link"; exit 1; }
echo "built Kernel33_dev.dll and Kernel33_noconsole.dll (SDL2 $SDL_VER static; not installed)"
