#!/bin/sh
# sh1x server - Call of Duty Online Closed Alpha Server (Linux, 32-bit runtime).
#
# Needs the 32-bit C and C++ runtimes (Ubuntu: apt install libc6-i386
# lib32stdc++6 lib32gcc-s1). Settings live in main/server.cfg; the lobby runs
# separately on this host.
#
#   SH1X_PORT        UDP port, 17733 unless set
#   SH1X_MAXCLIENTS  player slots, 18 unless set (fixed at start)
#   SH1X_INSTANCE    set by the lobby for extra instances: logs and sessions go to
#                    instances/<name>/main instead of main/
#   SH1X_MAP, SH1X_GAMETYPE, SH1X_ZONE_TYPE, SH1X_SESSION_MAP
#                    boot straight into this match instead of server.cfg's sh1x_boot
#   SH1X_EXTRA_ARGS  extra "+set name value" pairs, applied before server.cfg
set -eu
ROOT=$(cd "$(dirname "$0")" && pwd)
PORT=${SH1X_PORT:-17733}
MAXCLIENTS=${SH1X_MAXCLIENTS:-18}
HOME_DIR=$ROOT
[ -z "${SH1X_INSTANCE:-}" ] || HOME_DIR=$ROOT/instances/$SH1X_INSTANCE

[ -f "$ROOT/client_shipRetail/chinese/code_post_gfx_mp.ff" ] || {
    echo "game files not found under $ROOT/client_shipRetail/chinese" >&2; exit 1; }
[ -f "$ROOT/main/sh1x_svr_scripts.iwd" ] || {
    echo "script pack not found: $ROOT/main/sh1x_svr_scripts.iwd" >&2; exit 1; }
[ -f "$ROOT/C1.exe" ] || [ -f "$ROOT/c1_keys.bin" ] || {
    echo "neither C1.exe nor c1_keys.bin in $ROOT" >&2; exit 1; }

if [ -n "${SH1X_MAP:-}" ]; then
    set -- +set g_gametype "${SH1X_GAMETYPE:-dm}" +set sv_zoneType "${SH1X_ZONE_TYPE:-mp}" \
           +set sv_sessionMap "${SH1X_SESSION_MAP:-$SH1X_MAP}" +map "$SH1X_MAP"
else
    set -- +vstr sh1x_boot
fi

mkdir -p "$HOME_DIR/main"
echo "sh1x server on udp/$PORT - settings main/server.cfg, log $HOME_DIR/main/qconsole.log"
cd "$ROOT"
# shellcheck disable=SC2086
exec "$ROOT/sh1x_dedrun" \
    +set dedicated 2 \
    +set fs_basepath "$ROOT" \
    +set fs_homepath "$HOME_DIR" \
    +set net_port "$PORT" \
    +set sv_maxclients "$MAXCLIENTS" \
    +set logfile 2 \
    +set logrcon 1 \
    +set developer 0 \
    +set sv_fps 60 \
    +set scriptPath "fs:scripts" \
    +set scriptOverlay "fs:scripts" \
    +set scriptNativeOverlay "fs:scripts" \
    ${SH1X_EXTRA_ARGS:-} \
    +exec server.cfg \
    "$@"
