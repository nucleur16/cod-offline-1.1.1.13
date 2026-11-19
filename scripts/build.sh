#!/bin/sh
# Builds build/Kernel33.dll (32-bit) with MSYS2 mingw32 on Windows or the i686-w64-mingw32 cross compiler on Linux
# (scripts/toolchain.inc.sh) and copies it next to the game exe (the folder above the repository): sh scripts/build.sh
# src/unpacked.z (the game's unpacked code section, committed) is embedded as a resource.
#
# SDL2 2.32.10 (the official SDL2-devel-2.32.10-mingw.tar.gz, i686 static libSDL2.a) is linked statically
# for the gamepad backend (src/controller/controller.c). It is fetched into third_party/ (not committed) on the first build
# and checked against its SHA-256. libopus (src/audio/voice.c) and zlib come from third_party/mingw32.
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
. ../scripts/build-profile.sh
. ../scripts/toolchain.inc.sh
MW=../third_party/mingw32
SDL_VER=2.32.10
SDL_SHA256=83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2
TP=../third_party
SDL_DIR=$TP/SDL2-$SDL_VER/i686-w64-mingw32
[ -f unpacked.z ] || { echo "src/unpacked.z missing"; exit 1; }
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
k33_offline_objects "$OUT/obj_offline${K33_HOST_TOOLS:+_cross}"   # a cross build keeps its own objects
OFFLINE_LINK="$OFFLINE_OBJS -static-libstdc++ -lstdc++"
# The match server is linked into Kernel33.dll (src/p2p/p2p_inproc.c) as one static library, libsh1x_host.a
# (third_party/sh1x/README.md). Its source is in third_party/sh1x/src: it is built there with the same 32-bit MinGW
# toolchain (make, nasm), into third_party/sh1x/src/bin/libsh1x_host.a. Without that folder the prebuilt
# third_party/sh1x/libsh1x_host.a is linked. K33_SERVER=1 requires the server, 0 leaves it out (hosting then fails
# with "built without the match server"), auto (the default) links it when it exists. K33_SERVER_LIB names another
# build of it (nothing is compiled then).
WITH_SERVER=${K33_SERVER:-auto}
case "$WITH_SERVER" in 0|1|auto) ;; *) echo "K33_SERVER must be 0, 1 or auto" >&2; exit 1;; esac
SRVSRC=$ROOT/third_party/sh1x/src
if [ -z "${K33_SERVER_LIB:-}" ] && [ "$WITH_SERVER" != 0 ] && [ -f "$SRVSRC/makefile" ]; then
    # a cross build keeps its objects apart from the MSYS2 ones; -pipe: gcc's temporary files would otherwise go to
    # C:\WINDOWS under make
    SRVOBJ=obj/k33-msys
    [ -n "$K33_HOST_TOOLS" ] && SRVOBJ=obj/k33-cross
    mkdir -p "$SRVSRC/bin"
    echo "building the match server in $SRVSRC"
    make -C "$SRVSRC" -j"$(nproc 2>/dev/null || echo 4)" OS=Windows_NT "CC=$CC -pipe" "CPP=$CXX -pipe" \
        "AR=${K33_HOST_TOOLS}ar" "HOST_TOOLS=$K33_HOST_TOOLS" "WINDRES=$WINDRES" "PYTHON=$PYTHON" OBJ_DIR=$SRVOBJ \
        host-lib
    K33_SERVER_LIB=$SRVSRC/bin/libsh1x_host.a
fi
SRVLIB=${K33_SERVER_LIB:-$ROOT/third_party/sh1x/libsh1x_host.a}
SERVER_LINK=
if [ "$WITH_SERVER" != 0 ] && [ -f "$SRVLIB" ]; then
    # the server's imports beyond Kernel33's own: crypt32 (its certificate store) and wsock32
    SERVER_LINK="$SRVLIB -lcrypt32 -lwsock32"
    echo "linking the match server from $SRVLIB"
elif [ "$WITH_SERVER" = 1 ]; then
    echo "K33_SERVER=1 but $SRVLIB does not exist" >&2
    exit 1
else
    set -- "$@" -DK33_NO_SERVER=1
    [ "$WITH_SERVER" = 0 ] || echo "warning: no $SRVLIB"
    echo "Kernel33.dll is built without the match server"
fi
"$WINDRES" -O coff -i kernel33.rc -o "$OUT/kernel33_res.o"
# ld derives a DLL's default image base from the output file name as it is written on the command line. 0x69100000
# is the one "Kernel33.dll" always got; it is pinned so the DLL does not change with the output folder.
"$CC" "$@" -O2 -Wall -shared -s -static -static-libgcc -I"$SDL_DIR/include" -I"$MW/include" -o "$OUT/Kernel33.dll" \
    -I. kernel33.c "$OUT/kernel33_res.o" kernel33.def $SERVER_LINK $OFFLINE_LINK \
    -L"$SDL_DIR/lib" -L"$MW/lib" -lSDL2 -lsetupapi -lcfgmgr32 -lversion -loleaut32 -luuid \
    -lopus -lz -limm32 -lgdi32 -lshlwapi -lole32 -lwinmm -lws2_32 -liphlpapi -lpsapi -ladvapi32 -lshell32 -Wl,--enable-stdcall-fixup -Wl,--dynamicbase -Wl,--nxcompat -Wl,--image-base,0x69100000
if [ "${K33_NO_COPY:-0}" = 1 ]; then
    echo "built build/Kernel33.dll (SDL2 $SDL_VER static; not copied)"
else
    cp "$OUT/Kernel33.dll" "$ROOT/../Kernel33.dll"
    echo "built build/Kernel33.dll (SDL2 $SDL_VER static) and copied it to ../Kernel33.dll"
fi
