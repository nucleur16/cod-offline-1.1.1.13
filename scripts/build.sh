#!/bin/sh
# Builds build/Kernel33.dll (32-bit) with MSYS2 mingw32 and copies it next to the game exe (the folder above the
# repository): sh scripts/build.sh
# src/unpacked.z (embedded as a resource) comes from `python scripts/make_exe.py`, which needs C1.exe.
#
# SDL2 2.32.10 (the official SDL2-devel-2.32.10-mingw.tar.gz, i686 static libSDL2.a) is linked statically
# for the gamepad backend (src/controller/controller.c). It is fetched into third_party/ (not committed) on the first build
# and checked against its SHA-256. libopus (src/audio/voice.c) comes from MSYS2: pacman -S mingw-w64-i686-opus.
#
# K33_NO_COPY=1 sh scripts/build.sh builds build/Kernel33.dll without copying it to ../Kernel33.dll (next to the
# repository folder).
set -e
# Works from any directory: the sources are compiled inside src/ (so the paths below are what they were when the
# sources lived in the repository root), outputs go to build/ (not tracked), third_party/ stays in the root.
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT/src"
OUT=../build
mkdir -p "$OUT"
# The sh1x server that src/p2p/p2p.c hosts is the submodule server/sh1x (docs/P2P_HOSTING.md); the DLL itself does
# not compile it, scripts/build-server.sh does, and this script embeds its output (below). Init it unless
# K33_SERVER_DIR names another checkout (not --recursive: the server has a URL-less gitlink).
[ -n "${K33_SERVER_DIR:-}" ] || [ -f "$ROOT/server/sh1x/makefile" ] || git -C "$ROOT" submodule update --init server/sh1x || echo "warning: server/sh1x submodule not initialised (only scripts/build-server.sh needs it)"
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
# The lobby (docs/OFFLINE_LOBBY.md): the C++ sources in src/offline/ are compiled with g++ into
# build/obj_offline/ and linked statically (libstdc++ included). Every build has it: this client is offline only.
. ../scripts/build-offline.inc.sh
k33_offline_objects "$OUT/obj_offline"
OFFLINE_LINK="$OFFLINE_OBJS -static-libstdc++ -lstdc++"
# The match server travels inside Kernel33.dll (src/p2p/p2p_embedded.c; docs/P2P_HOSTING.md, "The server inside
# Kernel33.dll"): the server repo's host-dll output, stripped and zlib-packed, as RCDATA 2. scripts/build-server.sh
# builds it. K33_EMBED_SERVER=1 requires it, 0 leaves it out (the client then loads p2p\sh1x_host.dll), auto (the
# default) embeds it when it exists. K33_SERVER_DIR names the server checkout (default server/sh1x), K33_SERVER_DLL
# the DLL itself (default $K33_SERVER_DIR/bin/sh1x_host.dll).
EMBED=${K33_EMBED_SERVER:-auto}
case "$EMBED" in 0|1|auto) ;; *) echo "K33_EMBED_SERVER must be 0, 1 or auto" >&2; exit 1;; esac
SRVDIR=${K33_SERVER_DIR:-$ROOT/server/sh1x}
SRVDLL=${K33_SERVER_DLL:-$SRVDIR/bin/sh1x_host.dll}
RC=kernel33.rc
rm -f "$OUT/sh1x_host.k33s" "$OUT/kernel33_server.rc"
if [ "$EMBED" != 0 ] && [ -f "$SRVDLL" ]; then
    "$BIN/strip.exe" -o "$OUT/sh1x_host.stripped.dll" "$SRVDLL"
    SRVREV=$(git -C "$SRVDIR" rev-parse HEAD 2>/dev/null || echo unknown)
    python ../scripts/pack_server.py "$OUT/sh1x_host.stripped.dll" "$OUT/sh1x_host.k33s" "$SRVREV"
    rm -f "$OUT/sh1x_host.stripped.dll"
    printf '#include "kernel33.rc"\n// the sh1x match server (scripts/pack_server.py)\n2 RCDATA "%s/sh1x_host.k33s"\n' "$OUT" > "$OUT/kernel33_server.rc"
    RC=$OUT/kernel33_server.rc
    echo "embedding the sh1x server $SRVREV from $SRVDLL"
elif [ "$EMBED" = 1 ]; then
    echo "K33_EMBED_SERVER=1 but $SRVDLL does not exist: run scripts/build-server.sh first" >&2
    exit 1
else
    echo "warning: no sh1x_host.dll at $SRVDLL; Kernel33.dll is built without the match server (hosting then needs p2p\\sh1x_host.dll)"
fi
"$BIN/windres.exe" -O coff -I. -i "$RC" -o "$OUT/kernel33_res.o"
# ld derives a DLL's default image base from the output file name as it is written on the command line. 0x69100000
# is the one "Kernel33.dll" always got; it is pinned so the DLL does not change with the output folder.
"$BIN/gcc.exe" "$@" -O2 -Wall -shared -s -static -static-libgcc -I"$SDL_DIR/include" -o "$OUT/Kernel33.dll" \
    -I. kernel33.c "$OUT/kernel33_res.o" kernel33.def $OFFLINE_LINK \
    -L"$SDL_DIR/lib" -lSDL2 -lsetupapi -lcfgmgr32 -lversion -loleaut32 -luuid \
    -lopus -lz -limm32 -lgdi32 -lshlwapi -lole32 -lwinmm -lws2_32 -liphlpapi -lpsapi -ladvapi32 -lshell32 -Wl,--enable-stdcall-fixup -Wl,--dynamicbase -Wl,--nxcompat -Wl,--image-base,0x69100000
if [ "${K33_NO_COPY:-0}" = 1 ]; then
    echo "built build/Kernel33.dll (SDL2 $SDL_VER static; not copied)"
else
    cp "$OUT/Kernel33.dll" "$ROOT/../Kernel33.dll"
    echo "built build/Kernel33.dll (SDL2 $SDL_VER static) and copied it to ../Kernel33.dll"
fi
