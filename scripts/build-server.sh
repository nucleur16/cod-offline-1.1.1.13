#!/bin/sh
# Development only (needs the server source): builds the sh1x match server that Kernel33 hosts from the server/sh1x
# submodule and refreshes the prebuilt copy in third_party/sh1x that every build links. Never copies server sources
# into this repository.
#
#   sh scripts/build-server.sh                 build server/sh1x/bin/libsh1x_host.a, copy it to third_party/sh1x
#   K33_SERVER_CLEAN=1 sh scripts/build-server.sh   remove the object folder first (after header/.inc/asm changes)
#   K33_SERVER_DIR=D:/CoD/some_sh1x_worktree sh scripts/build-server.sh   build another checkout of the server
#   K33_P2P_STAGE=<install> sh scripts/build-server.sh   also write that install's main/sh1x_svr_scripts.iwd
#   K33_SERVER_EXE=1 sh scripts/build-server.sh     also cross-build the child-process exe in WSL (mode child)
#
# Built with the client's own compiler (scripts/toolchain.inc.sh), so the server's objects, libstdc++ and libgcc agree
# with the client's. It needs the prebuilt mbedtls/tomcrypt archives the server links (lib/*.a; not in the server's
# git): they are in third_party/server_libs, or K33_SERVER_LIBS names another folder holding them.
set -e
cd "$(dirname "$0")/.."   # the repository root, from any directory
if [ -n "${K33_SERVER_DIR:-}" ]; then SRV=$K33_SERVER_DIR; else git submodule update --init server/sh1x; SRV=server/sh1x; fi
. scripts/toolchain.inc.sh
LIBS=${K33_SERVER_LIBS:-third_party/server_libs}
for a in libmbedcrypto.a libmbedtls.a libmbedx509.a libtomcrypt.a; do
    [ -f "$SRV/lib/$a" ] || cp "$LIBS/$a" "$SRV/lib/$a"
done
OBJ=obj/k33-msys
[ -n "$K33_HOST_TOOLS" ] && OBJ=obj/k33-cross   # a cross build keeps its objects apart from the MSYS2 ones
[ "${K33_SERVER_CLEAN:-0}" = 1 ] && rm -rf "$SRV/$OBJ"
# The client's compiler (scripts/toolchain.inc.sh), make and nasm. -pipe: gcc's temporary files would otherwise go to
# C:\WINDOWS under make.
(
    cd "$SRV" && make -j8 OS=Windows_NT "CC=$CC -pipe" "CPP=$CXX -pipe" "AR=${K33_HOST_TOOLS}ar" "HOST_TOOLS=$K33_HOST_TOOLS" \
        "WINDRES=$WINDRES" "PYTHON=$PYTHON" OBJ_DIR=$OBJ host-lib
)
if [ "${K33_SERVER_EXE:-0}" = 1 ]; then
    WIN=$(cd "$SRV" && pwd -W)
    case "$WIN" in [A-Za-z]:*) WSLDIR="/mnt/$(echo "$WIN" | cut -c1 | tr A-Z a-z)$(echo "$WIN" | cut -c3-)";; *) WSLDIR="$WIN";; esac
    wsl -d Ubuntu-24.04 -- bash -lc "cd '$WSLDIR' && make -j8 OS=Windows_NT CC=i686-w64-mingw32-gcc CPP=i686-w64-mingw32-g++ AR=i686-w64-mingw32-ar OBJ_DIR=obj/k33-p2p bin/sh1x_dedrun.exe"
fi
# Refresh the pinned copies the build uses (scripts/build.sh links the lib, scripts/stage_server_data.py packs the
# data): third_party/sh1x/libsh1x_host.a and the waypoints / bot class table of the server's bin/home/main.
TP=third_party/sh1x
cp "$SRV/bin/libsh1x_host.a" "$TP/libsh1x_host.a"
rm -rf "$TP/data"
(cd "$SRV" && git ls-files bin/home/main/waypoints bin/home/main/mp) | while read -r f; do
    t="$TP/data/${f#bin/home/main/}"; mkdir -p "$(dirname "$t")"; cp "$SRV/$f" "$t"
done
cp "$SRV/License.md" "$TP/LICENSE.md"
echo "refreshed $TP from the server at $(git -C "$SRV" rev-parse --short HEAD)"
# A test install: K33_P2P_STAGE=<folder> also writes main/sh1x_svr_scripts.iwd there (and the loose GSC with
# K33_STAGE_RAWFILES=1); K33_SERVER_EXE=1 also the child-process exe as p2p/k33p2p_dedrun.exe.
if [ -n "${K33_P2P_STAGE:-}" ]; then
    STAGE=$K33_P2P_STAGE
    mkdir -p "$STAGE/main"
    if [ "${K33_SERVER_EXE:-0}" = 1 ]; then mkdir -p "$STAGE/p2p"; cp "$SRV/bin/sh1x_dedrun.exe" "$STAGE/p2p/k33p2p_dedrun.exe"; fi
    if [ "${K33_STAGE_RAWFILES:-0}" = 1 ]; then RAW="--rawfiles $STAGE/main/sh1x_rawfiles"; else RAW=; fi
    $PYTHON scripts/stage_server_data.py --out "$STAGE/main" $RAW
fi
$PYTHON -B tests/test_gsc_drift.py --server "$SRV" || true   # informational: scripts/gsc vs the server's rawfiles pin
