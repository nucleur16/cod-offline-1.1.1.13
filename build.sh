#!/bin/sh
# Builds Kernel33.dll (32-bit) with MSYS2 mingw32 and copies it next to the game exe.
# unpacked.z (embedded as a resource) comes from `python make_exe.py`, which needs C1.exe.
#
# SDL2 2.32.10 (the official SDL2-devel-2.32.10-mingw.tar.gz, i686 static libSDL2.a) is linked statically
# for the gamepad backend (controller.c). It is fetched into third_party/ (not committed) on the first build
# and checked against its SHA-256. libopus (voice.c) comes from MSYS2: pacman -S mingw-w64-i686-opus.
#
# K33_NO_COPY=1 sh build.sh builds Kernel33.dll here without copying it to ../Kernel33.dll.
set -e
cd "$(dirname "$0")"
BIN=/c/msys64/mingw32/bin
SDL_VER=2.32.10
SDL_SHA256=83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2
SDL_DIR=third_party/SDL2-$SDL_VER/i686-w64-mingw32
[ -f unpacked.z ] || { echo "unpacked.z missing: run python make_exe.py first"; exit 1; }
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
"$BIN/gcc.exe" -O2 -Wall -shared -s -static -static-libgcc -I"$SDL_DIR/include" -o Kernel33.dll \
    kernel33.c kernel33_res.o kernel33.def \
    -L"$SDL_DIR/lib" -lSDL2 -lsetupapi -lcfgmgr32 -lversion -loleaut32 -luuid \
    -lopus -lz -limm32 -lgdi32 -lshlwapi -lole32 -lwinmm -Wl,--enable-stdcall-fixup -Wl,--dynamicbase -Wl,--nxcompat
if [ "${K33_NO_COPY:-0}" = 1 ]; then
    echo "built Kernel33.dll (SDL2 $SDL_VER static; not copied)"
else
    cp Kernel33.dll ../Kernel33.dll
    echo "built Kernel33.dll (SDL2 $SDL_VER static)"
fi
