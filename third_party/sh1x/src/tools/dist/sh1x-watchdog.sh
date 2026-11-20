#!/usr/bin/env bash
# sh1x-watchdog.sh -- restart a match server that stopped serving.
#
#     nohup ./sh1x-watchdog.sh > watchdog.log 2>&1 &
#
# Every POLL seconds it asks each watched server for `sh1x_status` over
# loopback rcon. A server that does not answer at all (hung, e.g. a map load
# spinning at 100% CPU) or answers running=0 (no level loaded after a failed
# map load) for DEAD_AFTER seconds on end is restarted with `docker restart`.
# A parked server (parked=1: a coop match ended and players went back to
# the lobby) is healthy.
# A normal map load takes seconds, far below DEAD_AFTER.
#
# Settings (environment):
#   WATCH        "container:port ..." (default "codol-match-17733:17733")
#   POLL         seconds between checks (15)
#   DEAD_AFTER   seconds without service before a restart (120)
set -uo pipefail

SH1X_DIR=$(cd "$(dirname "$0")" && pwd)
WATCH=${WATCH:-"codol-match-17733:17733"}
POLL=${POLL:-15}
DEAD_AFTER=${DEAD_AFTER:-120}

log() { echo "[$(date -u +%Y-%m-%d\ %H:%M:%S)] $*"; }

# "ok", "down" (no level loaded) or "silent" (no answer)
state() {
    python3 - "$SH1X_DIR/main/server.cfg" "$1" <<'EOF'
import re, socket, sys
cfg, port = sys.argv[1], int(sys.argv[2])
m = re.search(r'rcon_password\s+"?([^"\s]+)', open(cfg).read())
if not m:
    print("ok"); sys.exit()          # cannot ask: never restart blindly
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.settimeout(3)
s.sendto(b"\xff\xff\xff\xffrcon " + m.group(1).encode() + b" sh1x_status", ("127.0.0.1", port))
try:
    data = s.recv(65535).decode("latin1")
except OSError:
    print("silent"); sys.exit()
# A coop server parked after its mission (map=none running=0 parked=1)
# is waiting for the lobby's next Ready, not broken.
if "parked=1" in data:
    print("ok"); sys.exit()
print("down" if "running=0" in data or "map=none" in data else "ok")
EOF
}

declare -A bad_since
log "watching $WATCH (restart after ${DEAD_AFTER}s without service)"
while :; do
    for entry in $WATCH; do
        name=${entry%%:*}; port=${entry##*:}
        # only a running container is ours to judge (the rollout may be restarting it)
        docker inspect -f '{{.State.Running}}' "$name" 2>/dev/null | grep -q true || { bad_since[$name]=; continue; }
        s=$(state "$port")
        if [ "$s" = "ok" ]; then
            [ -n "${bad_since[$name]:-}" ] && log "$name serving again"
            bad_since[$name]=
            continue
        fi
        now=$(date +%s)
        [ -n "${bad_since[$name]:-}" ] || { bad_since[$name]=$now; log "$name: $s"; }
        if [ $(( now - bad_since[$name] )) -ge "$DEAD_AFTER" ]; then
            log "$name: $s for $(( now - bad_since[$name] ))s; restarting"
            cp -p "$SH1X_DIR/main/qconsole.log" "$SH1X_DIR/main/qconsole.log.watchdog-$(date -u +%H%M%S)" 2>/dev/null
            docker restart "$name" > /dev/null
            bad_since[$name]=
        fi
    done
    sleep "$POLL"
done
