#!/bin/sh
# Live admission check against the Linux server (WSL/Linux, after tools/build_linux.sh).
#
#   sh tests/test_admission_live.sh <server home with game/ and home/> [port]
#
# <root>/game must hold c1_keys.bin, client_shipRetail/chinese/*.ff, main/*.iwd
# and <root>/home the script tree (see tools/package.py). Starts the server
# with admissionRequired 1 and a hand-written roster, then plays a C1 client
# (tests/admission_client.c) through four cases.
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
ROOT=$1
PORT=${2:-17739}
DATA=$ROOT/data-admission
mkdir -p "$DATA"
rm -f "$DATA/securitykey.bin"

INC="-I$REPO/src $(for d in "$REPO"/src/online/*/; do printf -- '-I%s ' "${d%/}"; done)"
gcc -m32 -O1 $INC "$REPO/tests/admission_client.c" "$REPO/src/online/net/association.c" \
    "$REPO/src/online/net/ol_ecdh.c" "$REPO/src/online/net/ol_crypto.c" "$REPO/src/online/net/wire.c" \
    -L"$REPO/lib-native" -ltomcrypt -lmbedcrypto -o /tmp/admission_client

KEY_A=000102030405060708090a0b0c0d0e0f
KEY_C=c0c1c2c3c4c5c6c7c8c9cacbcccdcecf
KEY_Z=fefefefefefefefefefefefefefefefe
EXP=$(( $(date +%s) + 3600 ))
cat > "$DATA/game_admissions.txt" <<EOF
# test roster
127.0.0.1 $KEY_A 9d8f2a1b3c4d5e6f $EXP Alice
10.9.9.9 $KEY_C 0 $EXP Carol
EOF

cd "$ROOT"
"$REPO/obj/sh1x-linux-bin/sh1x_dedrun" +set dedicated 2 +set fs_basepath "$ROOT/game" +set fs_homepath "$ROOT/home" \
    +set net_port "$PORT" +set developer 1 +set logfile 2 +set sv_fps 60 \
    +set scriptPath "$ROOT/home/codol_rawfiles" +set scriptOverlay "$ROOT/home/codol_rawfiles" \
    +set scriptNativeOverlay "$ROOT/home/codol_rawfiles" \
    +set securityKeyPath "$DATA/securitykey.bin" +set admissionRequired 1 \
    +set profilePath "$DATA" +set sv_zoneType mp +set sv_sessionMap mp_rust +set g_gametype dm \
    +scriptPath +scriptOverlay +scriptNativeOverlay +map mp_rust < /dev/null > "$ROOT/admission-server.out" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null || true' EXIT
LOG=$ROOT/home/main/qconsole.log
for i in $(seq 1 90); do
    grep -q "Calculate referenced files checksums" "$LOG" 2>/dev/null && break
    grep -q "Server crashed\|Sys_Error" "$LOG" 2>/dev/null && { echo "server failed"; exit 1; }
    sleep 2
done
sleep 3

run() { echo "== $1"; shift; /tmp/admission_client 127.0.0.1 "$PORT" "$@" || true; sleep 1; }
run "1: admitted key, its own xuid"         $KEY_A 9d8f2a1b3c4d5e6f Mallory
run "2: admitted key, someone else's xuid"  $KEY_A 1111111111111111 Mallory
run "3: key the lobby never admitted"       $KEY_Z 9d8f2a1b3c4d5e6f Mallory
run "4: admitted key from another IP, uid not latched yet" $KEY_C 2222222222222222 Mallory
echo "== server decisions"
grep "CoDOL admission" "$LOG" || true
