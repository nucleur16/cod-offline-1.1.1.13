#!/bin/sh
# Builds the feat/controller Kernel33.dll (32-bit, MSYS2 mingw32) as build/Kernel33_controller.dll and copies it
# nowhere: put it next to codoMP_client_shipRetail.exe yourself (keep a backup of the one there).
# src/unpacked.z (embedded as a resource) comes from `python scripts/make_exe.py`, which needs C1.exe.
#
# SDL2 2.32.10 (the official SDL2-devel-2.32.10-mingw.tar.gz, i686 static libSDL2.a) is linked statically
# for the gamepad backend. It is fetched into third_party/ (not committed) and checked against its SHA-256.
set -e
# Works from any directory: the sources are compiled inside src/ (so the paths below are what they were when the
# sources lived in the repository root), outputs go to build/ (not tracked), third_party/ stays in the root.
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT/src"
OUT=../build
mkdir -p "$OUT"
. ../scripts/build-profile.sh
BIN=/c/msys64/mingw32/bin
SDL_VER=2.32.10
SDL_SHA256=83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2
TP=../third_party
SDL_DIR=$TP/SDL2-$SDL_VER/i686-w64-mingw32
[ -f unpacked.z ] || { echo "src/unpacked.z missing: run python scripts/make_exe.py first"; exit 1; }
[ -f /c/msys64/mingw32/lib/libopus.a ] || { echo "libopus.a missing: pacman -S mingw-w64-i686-opus"; exit 1; }
if [ ! -f "$SDL_DIR/lib/libSDL2.a" ]; then
    mkdir -p "$TP"
    TGZ=$TP/SDL2-devel-$SDL_VER-mingw.tar.gz
    curl -sfL -o "$TGZ" "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-devel-$SDL_VER-mingw.tar.gz"
    echo "$SDL_SHA256  $TGZ" | sha256sum -c -
    tar xzf "$TGZ" -C "$TP" "SDL2-$SDL_VER/i686-w64-mingw32/include" "SDL2-$SDL_VER/i686-w64-mingw32/lib/libSDL2.a"
    rm -f "$TGZ"
fi
"$BIN/windres.exe" -O coff -i kernel33.rc -o "$OUT/kernel33_res.o"
"$BIN/gcc.exe" "$@" -O2 -Wall -shared -s -static -static-libgcc -I"$SDL_DIR/include" -o "$OUT/Kernel33_controller.dll" \
    kernel33.c "$OUT/kernel33_res.o" kernel33.def \
    -L"$SDL_DIR/lib" -lSDL2 -lsetupapi -lcfgmgr32 -lversion -loleaut32 -luuid \
    -lopus -lz -lwinhttp -limm32 -lgdi32 -lshlwapi -lole32 -lwinmm -lws2_32 -liphlpapi -lpsapi -ladvapi32 -lshell32 -Wl,--enable-stdcall-fixup -Wl,--dynamicbase -Wl,--nxcompat
echo "built build/Kernel33_controller.dll (SDL2 $SDL_VER static; not copied anywhere)"
