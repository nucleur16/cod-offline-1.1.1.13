#!/bin/sh
# One differential run: fresh Python lobby + fresh dwtest, replay a session against both, stop both.
#
#   sh tests/offline/difftest.sh <slot 0-9> <session.json> [dwdiff replay options, e.g. -v --stop]
#
# A slot is a private pair of servers, so several runs can go on at once:
#   Python lobby  127.0.0.1:31<slot>4 (STUN alt 31<slot>8)   in $DWDIFF_ROOT/slot<slot>/pylobby
#   dwtest        127.0.0.1:32<slot>4 (STUN alt 32<slot>8)   state in $DWDIFF_ROOT/slot<slot>/c
# Environment:
#   DWDIFF_ROOT     work folder OUTSIDE the repository (sessions hold real player ids)
#   PYLOBBY_SRC     pristine copy of the Python lobby repo (read only; copied per slot per run)
#   PYLOBBY_ZONE_DIR  optional, the client's zone folder (read only)
#   OFFLINE_SKIP    modules to leave out of dwtest (see build-offline.inc.sh)
#   DWDIFF_KEEP=1   leave both servers running afterwards (stop them with: difftest.sh <slot> stop)
# Never uses 3074, 17733 or 17760; both servers bind 127.0.0.1 only.
set -e
: "${DWDIFF_ROOT:?set DWDIFF_ROOT}" "${PYLOBBY_SRC:?set PYLOBBY_SRC}"
SLOT=$1; SESSION=$2
shift 2 || true
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
PYPORT=31${SLOT}4; PYALT=31${SLOT}8; CPORT=32${SLOT}4; CALT=32${SLOT}8
DIR=$DWDIFF_ROOT/slot$SLOT

kill_port() {
    for pid in $(netstat -ano | grep LISTENING | grep "127.0.0.1:$1 " | awk '{print $NF}' | sort -u); do
        taskkill //F //PID "$pid" >/dev/null 2>&1 || true
    done
}
stop_all() { kill_port "$PYPORT"; kill_port "$CPORT"; }
wait_port() {
    for _i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30; do
        netstat -ano | grep LISTENING | grep -q "127.0.0.1:$1 " && return 0
        sleep 0.5
    done
    echo "port $1 did not come up" >&2; return 1
}
stop_all
[ "$SESSION" = stop ] && exit 0

cd "$REPO"
export DWTEST_OBJ=obj_offline_test$SLOT DWTEST_OUT=$DIR/dwtest.exe
mkdir -p "$DIR"
sh tests/offline/build_dwtest.sh | grep -v '^g++' || true
[ -f "$DWTEST_OUT" ] || { echo "dwtest did not build" >&2; exit 1; }

rm -rf "$DIR/pylobby" "$DIR/pystate" "$DIR/c"
mkdir -p "$DIR/pystate" "$DIR/c/profile" "$DIR/c/state" "$DIR/c/match"
cp -r "$PYLOBBY_SRC" "$DIR/pylobby"
rm -rf "$DIR/pylobby/.git"
# The C++ port reads the SAME pristine resources (its own copy: the Python lobby writes into its folder).
cp -r "$PYLOBBY_SRC/resources" "$DIR/c/resources"

PYLOBBY_DIR=$DIR/pylobby PYLOBBY_STATE=$(cygpath -w "$DIR/pystate") PYLOBBY_PORT=$PYPORT PYLOBBY_ALT_PORT=$PYALT \
    sh tests/offline/run_pylobby.sh > "$DIR/pylobby.out.log" 2>&1 &
"$DWTEST_OUT" --port "$CPORT" --alt-port "$CALT" --differential --mm-port 17933 \
    --resources "$(cygpath -w "$DIR/c/resources")" --profile "$(cygpath -w "$DIR/c/profile")" \
    --state "$(cygpath -w "$DIR/c/state")" --match-profile "$(cygpath -w "$DIR/c/match")" \
    ${PYLOBBY_ZONE_DIR:+--zone-dir "$PYLOBBY_ZONE_DIR"} --log "$(cygpath -w "$DIR/dwtest.log")" > /dev/null 2>&1 &
wait_port "$PYPORT"; wait_port "$CPORT"

rc=0
PYLOBBY_DIR=$DIR/pylobby python tests/offline/dwdiff.py replay "$SESSION" --a 127.0.0.1:$PYPORT --b 127.0.0.1:$CPORT "$@" || rc=$?
[ "${DWDIFF_KEEP:-0}" = 1 ] || stop_all
exit $rc
