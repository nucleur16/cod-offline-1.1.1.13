#!/bin/sh
# Builds the sh1x match server that Kernel33 hosts (src/p2p/p2p.c, docs/P2P_HOSTING.md) from the server/sh1x
# submodule and stages its data for a hosting client. Never copies server sources into this repo.
#
#   sh scripts/build-server.sh                 build server/sh1x/bin/libsh1x_host.a (MSYS2 mingw32), then stage
#   K33_SERVER_CLEAN=1 sh scripts/build-server.sh   remove the object folder first (after header/.inc/asm changes)
#   K33_SERVER_DIR=D:/CoD/some_sh1x_worktree sh scripts/build-server.sh   build another checkout of the server
#   K33_SERVER_EXE=1 sh scripts/build-server.sh     also cross-build the child-process exe in WSL (mode child)
#
# The server is linked into Kernel33.dll: the server makefile's host-lib target is built with the same MSYS2
# mingw32 compiler as Kernel33 (gcc, nasm from /c/msys64/usr/bin), so the server's objects, libstdc++ and libgcc
# agree with the client's; scripts/build.sh then links bin/libsh1x_host.a (K33_SERVER_DIR=... when it is not
# server/sh1x). It needs the prebuilt mbedtls/tomcrypt archives the server links (lib/*.a; not in the server's git):
# they are in third_party/server_libs, or K33_SERVER_LIBS names another folder holding them.
# Staging ($K33_P2P_STAGE, default ../../k33_p2p_staging, seen from the repository root): scripts/stage_server_data.py
# writes p2p/home/main/sh1x_svr_scripts.iwd and main/sh1x_rawfiles/. The GSC in both comes from this repository's
# scripts/gsc (README.md, "GSC scripts"), not from the server's bin/home/codol_rawfiles submodule, which is left
# alone (uninitialised is fine) and only matters to the standalone server build.
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
# The staging folder mirrors the client install: copy its contents over the install root (a test install only).
#   p2p/home/main/sh1x_svr_scripts.iwd   the server pack (waypoints, bot tables)
#   main/sh1x_rawfiles/              the GSC tree: scripts/gsc's tracked files (src/p2p/p2p.c's default scripts=)
#   p2p/k33p2p_dedrun.exe            only with K33_SERVER_EXE=1: the same server as a child process (mode child; its
#                                    own image name, so an image-name kill of sh1x_dedrun.exe never reaches it)
# No p2p/sh1x_host.dll: the server is part of Kernel33.dll; an old copy in an install is never loaded.
STAGE=${K33_P2P_STAGE:-../../k33_p2p_staging}
mkdir -p "$STAGE/p2p/home/main" "$STAGE/main"
rm -f "$STAGE/p2p/sh1x_host.dll"
if [ "${K33_SERVER_EXE:-0}" = 1 ]; then cp "$SRV/bin/sh1x_dedrun.exe" "$STAGE/p2p/k33p2p_dedrun.exe"; fi
$PYTHON scripts/stage_server_data.py --server "$SRV" --stage "$STAGE"
echo "staged the sh1x server $(git -C "$SRV" rev-parse --short HEAD) data with the GSC of scripts/gsc in $STAGE (p2p/home/main pack, main/sh1x_rawfiles)"
$PYTHON -B tests/test_gsc_drift.py --server "$SRV" || true   # informational: scripts/gsc vs the server's rawfiles pin
echo "built $SRV/bin/libsh1x_host.a; scripts/build.sh links it into Kernel33.dll (K33_SERVER_DIR=$SRV when it is not server/sh1x)"
