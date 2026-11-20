#!/bin/sh
# sh1x server container entrypoint.
#
#   /game   read-only: c1_keys.bin, client_shipRetail/chinese/*.ff, main/*.iwd
#   /data   the independently run lobby's state directory: securitykey.bin,
#           game_admissions.txt, startgame.txt, native_server_rcon.txt,
#           resources/ (mpdata_s.<uid>, match reports)
#   /home/codol   fs_homepath: main/qconsole.log, sessions, per-player progress,
#                 and main/sh1x_svr_scripts.iwd (refreshed from the image each start)
set -eu

GAME=/game
DATA=/data
HOME_DIR=/home/codol
APP=/opt/codol

for need in "$GAME/c1_keys.bin" "$GAME/client_shipRetail/chinese/code_post_gfx_mp.ff" "$GAME/main/iw_config_00.iwd"; do
    if [ ! -f "$need" ]; then
        echo "entrypoint: missing $need -- mount the packaged game/ directory at /game" >&2
        exit 1
    fi
done

# The script pack is refreshed on every start so an image upgrade takes effect;
# runtime state beside it (logs, sessions, progress) is never touched.
mkdir -p "$HOME_DIR/main"
cp "$APP/sh1x_svr_scripts.iwd" "$HOME_DIR/main/sh1x_svr_scripts.iwd"
[ -d "$DATA/resources" ] || echo "entrypoint: $DATA/resources is missing -- is the lobby's state directory mounted at /data?" >&2

# The lobby starts maps over RCON with this password (it reads the same file).
RCON_FILE="$DATA/native_server_rcon.txt"
/usr/local/bin/rcon_password.sh "$RCON_FILE"
RCON_PASSWORD=$(tr -d '\r\n' < "$RCON_FILE")

MAP=$CODOL_MAP
GAMETYPE=$CODOL_GAMETYPE
ZONE_TYPE=$CODOL_ZONE_TYPE
SESSION_MAP=${CODOL_SESSION_MAP:-}

# The lobby stamps the map it sent C1 into startgame.txt. Take it when it is
# fresh and well formed (tools/ol_map_handoff.ps1 rules), unless the map is
# pinned with CODOL_MAP_PINNED=1.
HANDOFF="$DATA/startgame.txt"
if [ "${CODOL_MAP_PINNED:-0}" != "1" ] && [ -f "$HANDOFF" ]; then
    parsed=$(awk -v now="$(date +%s)" -f /usr/local/bin/map_handoff.awk "$HANDOFF" 2>/dev/null) || parsed=""
    if [ -n "$parsed" ]; then
        # shellcheck disable=SC2086
        set -- $parsed
        MAP=$1; GAMETYPE=$2; ZONE_TYPE=$3; SESSION_MAP=$4
        echo "entrypoint: map handoff $MAP $GAMETYPE ($ZONE_TYPE)"
    fi
fi
[ -n "$SESSION_MAP" ] || SESSION_MAP=$MAP

# Bots fill around the real players: none on an empty server, otherwise
# CODOL_BOTFILL minus the real players, at most CODOL_BOTMAX. CODOL_BOTFILL=0
# keeps a fixed CODOL_BOTS count instead.
EXTRA="+set scr_botfill ${CODOL_BOTFILL:-12} +set scr_botmax ${CODOL_BOTMAX:-11} +set scr_bots ${CODOL_BOTS:-0}"

echo "sh1x server: $MAP $GAMETYPE ($ZONE_TYPE) on udp/$CODOL_GAME_PORT"

cd "$APP"
# shellcheck disable=SC2086
exec "$APP/sh1x_dedrun" \
    +set dedicated 2 \
    +set fs_basepath "$GAME" \
    +set fs_homepath "$HOME_DIR" \
    +set net_port "$CODOL_GAME_PORT" \
    +set sv_maxclients "${OL_MAXCLIENTS:-18}" \
    +set rcon_password "$RCON_PASSWORD" \
    +set logfile 2 \
    +set logrcon 1 \
    +set developer "$CODOL_DEVELOPER" \
    +set sv_fps 60 \
    +set scriptPath "fs:scripts" \
    +set scriptOverlay "fs:scripts" \
    +set scriptNativeOverlay "fs:scripts" \
    +set securityKeyPath "$DATA/securitykey.bin" \
    +set admissionPath "$DATA/game_admissions.txt" \
    +set admissionRequired "${CODOL_ADMISSION_REQUIRED:-1}" \
    +set profilePath "$DATA/resources" \
    +set sv_zoneType "$ZONE_TYPE" \
    +set sv_sessionMap "$SESSION_MAP" \
    +set g_gametype "$GAMETYPE" \
    +map "$MAP" $EXTRA $CODOL_EXTRA_ARGS
