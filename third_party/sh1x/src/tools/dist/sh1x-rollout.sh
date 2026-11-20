#!/usr/bin/env bash
# sh1x-rollout.sh -- put a new match-server build live without ending a match.
#
#     ./sh1x-rollout.sh [new-binary]        (default: sh1x_dedrun.new beside this script)
#
# A new script pack goes beside the live one as main/sh1x_svr_scripts.iwd.new
# (NEW_PACK). Never swap the live pack by hand while a server runs: both
# servers read the same main/, and the old binary would compile the new
# scripts at its next map load and die on any builtin it lacks ("unknown
# function", 2026-09-28). The temporary server sees the staged pack through a
# file mount; the live pack is replaced only when the old server restarts.
#
#   1. installs the new binary as sh1x_dedrun (the running server keeps the old
#      one: it holds the file it started from);
#   2. starts a second match server on the new build, on the first free port of
#      ROLLOUT_PORTS, as its own container with the main one's image, mounts and
#      environment (logs in instances/<port>/main);
#   3. tells the lobby, through match_rollout.txt in its state directory, to place
#      new players on the new server and nobody new on the old one ("drain");
#   4. waits until the old server has no real players (its match is over and they
#      are back in the lobby), restarts it on the new build and undrains it;
#   5. drains the temporary server, and removes it once its players have left.
#
# The lobby re-reads match_rollout.txt every two seconds; it is never restarted.
# Run it detached so it survives the ssh session:
#     nohup ./sh1x-rollout.sh > rollout.log 2>&1 &
#
# Settings (environment):
#   MAIN_CONTAINER   the always-on match server container (codol-match-17733)
#   MAIN_PORT        its port (17733)
#   ROLLOUT_PORTS    ports the temporary server may take ("17734 17735 ... 17741")
#   STATE_DIR        the lobby state directory on this host (~/codol/data)
#   POLL             seconds between checks (10)
#   EMPTY_GRACE      seconds a server must stay without real players before it
#                    counts as empty (60, the lobby's placement reservation)
set -euo pipefail

SH1X_DIR=$(cd "$(dirname "$0")" && pwd)
NEW_BIN=${1:-$SH1X_DIR/sh1x_dedrun.new}
PACK=$SH1X_DIR/main/sh1x_svr_scripts.iwd
NEW_PACK=${NEW_PACK:-$PACK.new}
MAIN_CONTAINER=${MAIN_CONTAINER:-codol-match-17733}
MAIN_PORT=${MAIN_PORT:-17733}
ROLLOUT_PORTS=${ROLLOUT_PORTS:-"17734 17735 17736 17737 17738 17739 17740 17741"}
STATE_DIR=${STATE_DIR:-$HOME/codol/data}
POLL=${POLL:-10}
ROLLOUT=$STATE_DIR/match_rollout.txt

log() { echo "[$(date -u +%H:%M:%S)] $*"; }

# humans on a server, or "-" when it does not answer sh1x_status
humans() {
    python3 - "$SH1X_DIR/main/server.cfg" "$1" <<'EOF'
import re, socket, sys
cfg, port = sys.argv[1], int(sys.argv[2])
m = re.search(r'rcon_password\s+"?([^"\s]+)', open(cfg).read())
if not m:
    print("-"); sys.exit()
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.settimeout(1.5)
s.sendto(b"\xff\xff\xff\xffrcon " + m.group(1).encode() + b" sh1x_status", ("127.0.0.1", port))
try:
    data = s.recv(65535).decode("latin1")
    h = re.search(r"humans=(\d+)", data)
    print(h.group(1) if h else "-")
except OSError:
    print("-")
EOF
}

wait_up() {        # port, seconds
    local i
    for ((i = 0; i < $2; i += 2)); do
        [ "$(humans "$1")" != "-" ] && return 0
        sleep 2
    done
    return 1
}

# Empty means empty for EMPTY_GRACE seconds on end. humans counts connected
# clients only; a player the lobby placed here just before the drain is still
# preloading the map (5-15 s) and a player reconnecting after a map change is
# briefly out -- both read as 0. Restarting or removing the server then leaves
# them on "awaiting connection" until EXE_ERR_SERVER_TIMEOUT. The default is
# the lobby's placement reservation (INSTANCE_RESERVE, 60 s).
EMPTY_GRACE=${EMPTY_GRACE:-60}
wait_empty() {     # port
    local h since=
    while :; do
        h=$(humans "$1")
        if [ "$h" = "0" ]; then
            [ -n "$since" ] || since=$(date +%s)
            [ $(( $(date +%s) - since )) -ge "$EMPTY_GRACE" ] && return 0
        else
            since=
            log ":$1 has ${h} real player(s); waiting"
        fi
        sleep "$POLL"
    done
}

port_free() {
    ! ss -uln 2>/dev/null | awk '{print $4}' | grep -q ":$1\$"
}

# The lobby container owns its state directory (root inside the container), so
# write the file through it: /state there is STATE_DIR here.
LOBBY_CONTAINER=${LOBBY_CONTAINER:-codol-lobby}
rollout() {
    printf '%s\n' "$@" | docker exec -i "$LOBBY_CONTAINER" sh -c \
        'cat > /state/match_rollout.txt.tmp && mv /state/match_rollout.txt.tmp /state/match_rollout.txt'
}
rollout_clear() { docker exec "$LOBBY_CONTAINER" rm -f /state/match_rollout.txt; }

# 1. install the new build
if [ -f "$NEW_BIN" ] && [ "$NEW_BIN" != "$SH1X_DIR/sh1x_dedrun" ]; then
    cp -p "$SH1X_DIR/sh1x_dedrun" "$SH1X_DIR/sh1x_dedrun.prev"
    chmod +x "$NEW_BIN"
    mv "$NEW_BIN" "$SH1X_DIR/sh1x_dedrun"
    log "installed $(md5sum "$SH1X_DIR/sh1x_dedrun" | cut -c1-12) (previous kept as sh1x_dedrun.prev)"
else
    log "no new binary at $NEW_BIN; rolling the installed one"
fi

# 2. a temporary server on the new build
TEMP_PORT=
for p in $ROLLOUT_PORTS; do
    if port_free "$p" && ! docker ps -a --format '{{.Names}}' | grep -qx "codol-match-$p"; then
        TEMP_PORT=$p; break
    fi
done
[ -n "$TEMP_PORT" ] || { log "no free port in ROLLOUT_PORTS"; exit 1; }
TEMP=codol-match-$TEMP_PORT
IMAGE=$(docker inspect -f '{{.Config.Image}}' "$MAIN_CONTAINER")
ENVFILE=$(mktemp)
docker inspect -f '{{range .Config.Env}}{{println .}}{{end}}' "$MAIN_CONTAINER" \
    | grep -v -E '^(SH1X_PORT|SH1X_INSTANCE|PATH|HOSTNAME)=' | grep -v '^$' > "$ENVFILE"
MOUNTS=()
while read -r src dst; do MOUNTS+=(-v "$src:$dst"); done < <(
    docker inspect -f '{{range .Mounts}}{{.Source}} {{.Destination}}{{println}}{{end}}' "$MAIN_CONTAINER" | grep -v '^$')
# the distribution's path inside the container (the compose file mounts ../sh1x at /srv/sh1x)
IN_DIR=$(docker inspect -f '{{range .Mounts}}{{.Source}} {{.Destination}}{{println}}{{end}}' "$MAIN_CONTAINER" \
    | awk -v d="$SH1X_DIR" '$1 == d {print $2}')
IN_DIR=${IN_DIR:-/srv/sh1x}
# the staged script pack, for the temporary server only (a rename keeps the
# inode, so this mount survives installing it as the live pack in step 4)
if [ -f "$NEW_PACK" ]; then
    MOUNTS+=(-v "$NEW_PACK:$IN_DIR/main/$(basename "$PACK"):ro")
    log "staged script pack $(md5sum "$NEW_PACK" | cut -c1-12) for the new build"
fi
docker run -d --name "$TEMP" --network host --init --restart no \
    --env-file "$ENVFILE" -e SH1X_PORT="$TEMP_PORT" -e SH1X_INSTANCE="$TEMP_PORT" \
    "${MOUNTS[@]}" --entrypoint "$IN_DIR/start-sh1x-server.sh" "$IMAGE" > /dev/null
rm -f "$ENVFILE"
log "started $TEMP on the new build"
if ! wait_up "$TEMP_PORT" 180; then
    log "$TEMP did not answer within 180 s; removing it, nothing drained"
    docker rm -f "$TEMP" > /dev/null
    exit 1
fi

# 3. new players to the new server, nobody new to the old one
rollout "extra $TEMP_PORT" "drain $MAIN_PORT"
log "lobby: new players -> :$TEMP_PORT, :$MAIN_PORT draining"

# 4. restart the old server once its match is over
wait_empty "$MAIN_PORT"
log ":$MAIN_PORT is empty; restarting $MAIN_CONTAINER on the new build"
if [ -f "$NEW_PACK" ]; then
    cp -p "$PACK" "$PACK.prev"
    mv "$NEW_PACK" "$PACK"
    log "installed the staged script pack (previous kept as $(basename "$PACK").prev)"
fi
docker restart "$MAIN_CONTAINER" > /dev/null
if ! wait_up "$MAIN_PORT" 180; then
    log "$MAIN_CONTAINER did not come back within 180 s; leaving :$TEMP_PORT serving and :$MAIN_PORT drained"
    exit 1
fi
rollout "extra $TEMP_PORT" "drain $TEMP_PORT"
log ":$MAIN_PORT is back; :$TEMP_PORT draining"

# 5. retire the temporary server once its players have left
wait_empty "$TEMP_PORT"
docker rm -f "$TEMP" > /dev/null
rollout_clear
log "removed $TEMP; rollout complete"
