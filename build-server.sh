#!/bin/sh
# Builds the sh1x match server that Kernel33 hosts (p2p.c, docs/P2P_HOSTING.md) from the server/sh1x
# submodule and stages it for a hosting client. Never copies server sources into this repo.
#
#   sh build-server.sh                 cross-build server/sh1x/bin/codol_dedrun.exe in WSL, then stage
#   K33_SERVER_CLEAN=1 sh build-server.sh   remove the object folder first (after header/.inc/asm changes)
#
# Needs WSL (Ubuntu-24.04, i686-w64-mingw32-gcc, nasm) as the server repo's own Windows build does, and
# the prebuilt mbedtls/tomcrypt archives the server links (lib/*.a; not in git): K33_SERVER_LIBS names a
# folder holding them (default D:/CoD/CoD4x_Server/lib, the server repo's own build).
# Staging ($K33_P2P_STAGE, default ../../k33_p2p_staging): codol_dedrun.exe, home/main/sh1x_svr_scripts.iwd
# (the server's tools/codol_stage_server_pack.py) and codol_rawfiles/ (the GSC the exe was built with).
set -e
cd "$(dirname "$0")"
git submodule update --init server/sh1x
SRV=server/sh1x
if [ ! -f "$SRV/bin/home/codol_rawfiles/maps/mp/gametypes/_callbacksetup.gsc" ]; then
    # only the rawfiles: plugins/cod4x_mysql_plugin is a gitlink without a URL, so never --recursive
    git -C "$SRV" -c core.autocrlf=false submodule update --init -- bin/home/codol_rawfiles
    git -C "$SRV/bin/home/codol_rawfiles" config core.autocrlf false
fi
LIBS=${K33_SERVER_LIBS:-D:/CoD/CoD4x_Server/lib}
for a in libmbedcrypto.a libmbedtls.a libmbedx509.a libtomcrypt.a; do
    [ -f "$SRV/lib/$a" ] || cp "$LIBS/$a" "$SRV/lib/$a"
done
OBJ=obj/k33-p2p
[ "${K33_SERVER_CLEAN:-0}" = 1 ] && rm -rf "$SRV/$OBJ"
WIN=$(cd "$SRV" && pwd -W)
case "$WIN" in [A-Za-z]:*) WSLDIR="/mnt/$(echo "$WIN" | cut -c1 | tr A-Z a-z)$(echo "$WIN" | cut -c3-)";; *) WSLDIR="$WIN";; esac
wsl -d Ubuntu-24.04 -- bash -lc "cd '$WSLDIR' && make -j8 OS=Windows_NT CC=i686-w64-mingw32-gcc CPP=i686-w64-mingw32-g++ AR=i686-w64-mingw32-ar OBJ_DIR=$OBJ bin/codol_dedrun.exe"
STAGE=${K33_P2P_STAGE:-../../k33_p2p_staging}
mkdir -p "$STAGE/home/main"
cp "$SRV/bin/codol_dedrun.exe" "$STAGE/k33p2p_dedrun.exe"   # own image name: an image-name kill of codol_dedrun.exe must not reach it
python "$SRV/tools/codol_stage_server_pack.py" --repo "$SRV" --home-main "$STAGE/home/main"
rm -rf "$STAGE/codol_rawfiles"
cp -r "$SRV/bin/home/codol_rawfiles" "$STAGE/codol_rawfiles"
rm -rf "$STAGE/codol_rawfiles/.git"
echo "staged the sh1x server $(git -C "$SRV" rev-parse --short HEAD) in $STAGE (exe, home/main pack, codol_rawfiles)"
