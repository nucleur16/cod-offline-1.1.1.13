#!/bin/sh
# Builds the sh1x match server that Kernel33 hosts, from its source in third_party/sh1x/src (copied in from the server
# repository by tools/vendor_server.py), and refreshes third_party/sh1x/data (waypoints, bot class table) from it.
# scripts/build.sh builds the same source on its own; this script is for server work and test installs.
#
#   sh scripts/build-server.sh                 build third_party/sh1x/src/bin/libsh1x_host.a
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
SRV=${K33_SERVER_DIR:-third_party/sh1x/src}
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
# Refresh the data scripts/stage_server_data.py packs: the waypoints / bot class table of the server's bin/home/main.
# (Another checkout, K33_SERVER_DIR: scripts/build.sh links its lib with K33_SERVER_LIB=<checkout>/bin/libsh1x_host.a.)
TP=third_party/sh1x
rm -rf "$TP/data"
(cd "$SRV" && find bin/home/main/waypoints bin/home/main/mp -type f) | while read -r f; do
    t="$TP/data/${f#bin/home/main/}"; mkdir -p "$(dirname "$t")"; cp "$SRV/$f" "$t"
done
cp "$SRV/License.md" "$TP/LICENSE.md"
echo "built $SRV/bin/libsh1x_host.a and refreshed $TP/data"
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
