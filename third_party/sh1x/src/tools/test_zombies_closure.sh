#!/usr/bin/env bash
# Focused production-body tests; 32-bit Linux GCC. No server or client is launched.
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
inc=(-Isrc)
for area in src/online/*; do [[ ! -d "$area" ]] || inc+=("-I$area"); done
flags=(-m32 -msse2 -O1 -g -DOL_SERVER -D_GNU_SOURCE -ffunction-sections -fdata-sections)
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-sanitize=alignment -fno-omit-frame-pointer -no-pie); fi
run() {
 local name=$1; shift
 gcc "${flags[@]}" "${inc[@]}" "$@" -Wl,--gc-sections -lm -o "$out/$name"
 "$out/$name"
}
run tokens tests/test_zombies_tokens.c src/online/player/revive_token.c
run fx tests/test_zombies_fx_reservation.c src/online/snapshot/configstrings.c
run targeting tests/test_zombies_targeting.c src/online/player/actor_targeting.c
run checkpoint tests/test_zombies_checkpoint.c src/online/stats/match_report.c
run viewmodel-ready tests/test_zombies_viewmodel_ready.c src/online/script/script_entity_fx.c
run localtest-release tests/test_zombies_localtest.c
run localtest-enabled -DLOCAL_TEST_BUILD tests/test_zombies_localtest.c
run status-colour tests/test_client_appearance.c src/msg.c src/online/snapshot/clientstate_ext.c src/online/snapshot/clients.c
