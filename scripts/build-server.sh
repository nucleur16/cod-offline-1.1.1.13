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
# server/sh1x). It needs the prebuilt mbedtls/tomcrypt archives the server links (lib/*.a; not in git):
# K33_SERVER_LIBS names a folder holding them (default D:/CoD/CoD4x_Server/lib, the server repo's own build).
# Staging ($K33_P2P_STAGE, default ../../k33_p2p_staging, seen from the repository root): home/main/sh1x_svr_scripts.iwd
# (the server's tools/stage_server_pack.py) and codol_rawfiles/ (the GSC the server was built with).
set -e
cd "$(dirname "$0")/.."   # the repository root, from any directory
if [ -n "${K33_SERVER_DIR:-}" ]; then SRV=$K33_SERVER_DIR; else git submodule update --init server/sh1x; SRV=server/sh1x; fi
if [ ! -f "$SRV/bin/home/codol_rawfiles/maps/mp/gametypes/_callbacksetup.gsc" ]; then
    # only the rawfiles: plugins/cod4x_mysql_plugin is a gitlink without a URL, so never --recursive
    git -C "$SRV" -c core.autocrlf=false submodule update --init -- bin/home/codol_rawfiles
    git -C "$SRV/bin/home/codol_rawfiles" config core.autocrlf false
fi
LIBS=${K33_SERVER_LIBS:-D:/CoD/CoD4x_Server/lib}
for a in libmbedcrypto.a libmbedtls.a libmbedx509.a libtomcrypt.a; do
    [ -f "$SRV/lib/$a" ] || cp "$LIBS/$a" "$SRV/lib/$a"
done
OBJ=obj/k33-msys
[ "${K33_SERVER_CLEAN:-0}" = 1 ] && rm -rf "$SRV/$OBJ"
# MSYS2's make and nasm, its mingw32 gcc. -pipe: gcc's temporary files would otherwise go to C:\WINDOWS under make.
(
    export PATH="/c/msys64/mingw32/bin:/c/msys64/usr/bin:$PATH"
    cd "$SRV" && make -j8 OS=Windows_NT "CC=gcc -pipe" "CPP=g++ -pipe" AR=ar PYTHON=python OBJ_DIR=$OBJ host-lib
)
if [ "${K33_SERVER_EXE:-0}" = 1 ]; then
    WIN=$(cd "$SRV" && pwd -W)
    case "$WIN" in [A-Za-z]:*) WSLDIR="/mnt/$(echo "$WIN" | cut -c1 | tr A-Z a-z)$(echo "$WIN" | cut -c3-)";; *) WSLDIR="$WIN";; esac
    wsl -d Ubuntu-24.04 -- bash -lc "cd '$WSLDIR' && make -j8 OS=Windows_NT CC=i686-w64-mingw32-gcc CPP=i686-w64-mingw32-g++ AR=i686-w64-mingw32-ar OBJ_DIR=obj/k33-p2p bin/sh1x_dedrun.exe"
fi
# The staging folder mirrors the client install: copy its contents over the install root (a test install only).
#   p2p/home/main/sh1x_svr_scripts.iwd   the server pack (waypoints, bot tables)
#   main/sh1x_rawfiles/              the GSC tree the server was built with (src/p2p/p2p.c's default scripts=)
#   p2p/k33p2p_dedrun.exe            only with K33_SERVER_EXE=1: the same server as a child process (mode child; its
#                                    own image name, so an image-name kill of sh1x_dedrun.exe never reaches it)
# No p2p/sh1x_host.dll: the server is part of Kernel33.dll; an old copy in an install is never loaded.
STAGE=${K33_P2P_STAGE:-../../k33_p2p_staging}
mkdir -p "$STAGE/p2p/home/main" "$STAGE/main"
rm -f "$STAGE/p2p/sh1x_host.dll"
if [ "${K33_SERVER_EXE:-0}" = 1 ]; then cp "$SRV/bin/sh1x_dedrun.exe" "$STAGE/p2p/k33p2p_dedrun.exe"; fi
python "$SRV/tools/stage_server_pack.py" --repo "$SRV" --home-main "$STAGE/p2p/home/main"
rm -rf "$STAGE/main/sh1x_rawfiles"
cp -r "$SRV/bin/home/codol_rawfiles" "$STAGE/main/sh1x_rawfiles"
rm -rf "$STAGE/main/sh1x_rawfiles/.git"
echo "staged the sh1x server $(git -C "$SRV" rev-parse --short HEAD) data in $STAGE (p2p/home/main pack, main/sh1x_rawfiles)"
echo "built $SRV/bin/libsh1x_host.a; scripts/build.sh links it into Kernel33.dll (K33_SERVER_DIR=$SRV when it is not server/sh1x)"
