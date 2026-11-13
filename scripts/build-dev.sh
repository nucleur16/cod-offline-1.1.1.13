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
# The sh1x server that p2p.c hosts is the submodule server/sh1x (docs/P2P_HOSTING.md); the DLL itself does
# not compile it, build-server.sh does. Init it (not --recursive: the server has a URL-less gitlink).
[ -f server/sh1x/makefile ] || git submodule update --init server/sh1x || echo "warning: server/sh1x submodule not initialised (only build-server.sh needs it)"
# build-profile.sh (CODOL_LOCAL_TESTING, the launcher pin): the dev build defaults to the local-testing profile, whose
# second client still needs +set codol_local_test_multi 1 and a loopback dwauth.ini (local_multi.c).
K33_LOCAL_TESTING=${K33_LOCAL_TESTING:-1}
. ./build-profile.sh
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
# Offline mode (K33_OFFLINE=1, docs/OFFLINE_LOBBY.md): the C++ lobby in offline/ is compiled with g++ into
# obj_offline/ and linked statically (libstdc++ included). Without the flag none of it is built or linked.
OFFLINE_LINK=""
if [ "${K33_OFFLINE:-0}" = 1 ]; then
    . ./build-offline.inc.sh
    k33_offline_objects obj_offline
    OFFLINE_LINK="$OFFLINE_OBJS -static-libstdc++ -lstdc++"
fi
"$BIN/windres.exe" -O coff -i kernel33.rc -o kernel33_res.o
CFLAGS="-O2 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -Wno-missing-field-initializers -shared -s -static -static-libgcc -I$SDL_DIR/include"
LIBS="-L$SDL_DIR/lib -lSDL2 -lsetupapi -lcfgmgr32 -lversion -loleaut32 -luuid \
-lopus -lz -limm32 -lgdi32 -lshlwapi -lole32 -lwinmm -lws2_32 -liphlpapi -lpsapi -ladvapi32 -lshell32 -Wl,--enable-stdcall-fixup -Wl,--dynamicbase -Wl,--nxcompat"
"$BIN/gcc.exe" "$@" $CFLAGS -o Kernel33_dev.dll kernel33.c kernel33_res.o kernel33.def $OFFLINE_LINK $LIBS
"$BIN/gcc.exe" "$@" $CFLAGS -DK33_NO_CONSOLE -o Kernel33_noconsole.dll kernel33.c kernel33_res.o kernel33.def $OFFLINE_LINK $LIBS \
    || { echo "FAILED: the build without console.c (-DK33_NO_CONSOLE) does not link"; exit 1; }
echo "built Kernel33_dev.dll and Kernel33_noconsole.dll (SDL2 $SDL_VER static; not installed)"
