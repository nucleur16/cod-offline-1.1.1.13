#!/bin/sh
# Differential-test reference: runs a COPY of the Python lobby (CODOL-EMU 1.1.1.13 Demonware emulator)
# on 127.0.0.1:$PYLOBBY_PORT (default 3174, never 3074) with its match-server rcon pointed at an unused
# port, so nothing here can touch a real lobby (3074) or a real match server (17733/17760).
#
#   PYLOBBY_DIR    the copy of the emulator repo (never the original clone, never the live lobby)
#   PYLOBBY_STATE  its state folder
set -e
: "${PYLOBBY_DIR:?set PYLOBBY_DIR to a private copy of the Python lobby}"
: "${PYLOBBY_STATE:?set PYLOBBY_STATE to a private state folder}"
PORT=${PYLOBBY_PORT:-3174}
ALT=${PYLOBBY_ALT_PORT:-3178}
cd "$PYLOBBY_DIR"
rm -f resources/mpdata resources/Recentmetlist
export CODOL_OFFLINE=1 CODOL_LOBBY_HOST=127.0.0.1 CODOL_STATE_DIR="$PYLOBBY_STATE"
export CODOL_MARKETPLACE_BALANCE=0 CODOL_MAP_POLICY=room CODOL_MAP_PREFER=mp_killhouse_sh CODOL_MAP_TYPES=mp,coop,zm,survival
export CODOL_JOIN_FIX=0 CODOL_ADMISSION=auto CODOL_ADMISSION_PORT=0
export CODOL_INSTANCE_PORTS=17933 CODOL_INSTANCE_SPAWN=spawn_disabled.cmd
export CODOL_MATCH_RCON_PORT=17933 CODOL_MATCH_RCON_HOST=127.0.0.1 CODOL_MATCH_RCON_PASSWORD=offltest
export CODOL_MM_HOST_PORT=${PYLOBBY_MM_PORT:-17933}
[ -n "$PYLOBBY_ZONE_DIR" ] && export CODOL_ZONE_DIR="$PYLOBBY_ZONE_DIR"
export PYTHONDONTWRITEBYTECODE=1 PYTHONIOENCODING=utf-8
exec python dw-capture.py --host 127.0.0.1 --port "$PORT" --alt-port "$ALT" --respond --log "$PYLOBBY_STATE/dw-capture.log" "$@"
