#!/usr/bin/env bash
set -euo pipefail
profile=${NATIVE_PROFILE:?profile};src=/src;recipe=/recipe;out=/out
cd /src;sha256sum --quiet --check /recipe/source.sha256
{ gcc --version;g++ --version;nasm -v;ldd --version; } > /out/toolchain.txt
inc=(-I/src/src -I/recipe);for area in /src/src/codol/*;do [[ ! -d "$area" ]] || inc+=("-I$area");done
flags=(-m32 -msse2 -O1 -g -DCODOL_SERVER -D_GNU_SOURCE -ffunction-sections -fdata-sections)
san=(-fsanitize=address,undefined -fno-sanitize=alignment --param asan-globals=0 -fno-omit-frame-pointer -no-pie)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
pair() {
 local name=$1;shift
 for variant in normal asan;do
  local extra=();[[ "$variant" != asan ]] || extra=("${san[@]}")
  gcc "${flags[@]}" "${extra[@]}" "${inc[@]}" "$@" -Wl,--gc-sections -lm -o "/out/$name-$variant" > "/out/$name-$variant-build.log" 2>&1
  "/out/$name-$variant" > "/out/$name-$variant-run.log" 2>&1
  printf '%s-%s PASS\n' "$name" "$variant"
 done
}
identity_extra=();[[ "$profile" != zombies ]] || identity_extra+=(/src/src/codol/player/codol_revive_token.c)
pair pending /recipe/test_pending_identity.c /src/src/codol/net/codol_connect.c /src/src/sys_net.c "${identity_extra[@]}"
if [[ "$profile" == normal ]];then
 pair party /recipe/test_party_native.c /src/src/codol/net/codol_quickplay.c /src/src/codol/net/codol_admission.c /src/src/codol/net/codol_match_status.c /src/src/codol/net/codol_connect.c /src/src/codol/script/codol_script_quick_party.c /src/src/sv_game.c
 pair quick /recipe/test_legacy_quick.c /src/src/codol/net/codol_quickplay.c /src/src/codol/net/codol_admission.c /src/src/codol/net/codol_match_status.c
 pair training /recipe/test_training_bot_seat.c /src/src/codol/net/codol_quickplay.c /src/src/codol/net/codol_connect.c
 pair startup /recipe/test_startup_archives.c
fi
pair hide /recipe/test_weapon_masks.c /src/src/codol/script/codol_script_weapon_visual_queries.c /src/src/codol/player/codol_hide_parts.c /src/src/codol/snapshot/codol_entitystate_ext.c
for variant in normal asan;do
 extra=();[[ "$variant" != asan ]] || extra=("${san[@]}")
 [[ "$profile" != zombies ]] || extra+=(-DHAS_ZM_TOPOLOGY)
 for name in codol_bot_nav q_math;do
  file=/src/src/q_math.c;[[ "$name" != codol_bot_nav ]] || file=/src/src/codol/player/codol_bot_nav.c
  gcc "${flags[@]}" "${extra[@]}" "${inc[@]}" -c "$file" -o "/out/$name-$variant.o" > "/out/$name-$variant-build.log" 2>&1
 done
 g++ "${flags[@]}" "${extra[@]}" -std=gnu++11 "${inc[@]}" /recipe/test_route_endpoint.cpp "/out/codol_bot_nav-$variant.o" "/out/q_math-$variant.o" -Wl,--gc-sections -lm -o "/out/route-$variant" > "/out/route-$variant-build.log" 2>&1
 "/out/route-$variant" > "/out/route-$variant-run.log" 2>&1
 printf 'route-%s PASS\n' "$variant"
done
if [[ "$profile" == zombies ]];then
 for mode in candidate candidate-asan baseline-report baseline-drop-order;do
  report=/src/src/codol/stats/codol_match_report.c;extra=()
  [[ "$mode" != baseline-report ]] || report=/recipe/baseline-report-source.c
  [[ "$mode" != candidate-asan ]] || extra=("${san[@]}")
  gcc "${flags[@]}" "${extra[@]}" -std=gnu11 -pthread -DCODOL_ASYNC_CAPTURE_TEST "${inc[@]}" "/recipe/$mode.c" "$report" /src/src/codol/stats/codol_scoreboard_stats.c /src/src/codol/stats/codol_match_gp.c /src/src/codol/snapshot/codol_async_capture.c -lm -Wl,--gc-sections -o "/out/session-$mode" > "/out/session-$mode-build.log" 2>&1
  state="/tmp/state-$mode";mkdir "$state";set +e
  "/out/session-$mode" "$state" > "/out/session-$mode-run.log" 2>&1
  rc=$?;set -e;printf '%s\n' "$rc" > "/out/session-$mode-exit.txt"
  cp -R "$state" "/out/state-$mode"
  if [[ "$mode" == baseline-* ]];then [[ $rc != 0 ]];else [[ $rc == 0 ]];fi
  printf 'session-%s expected-status PASS exit=%s\n' "$mode" "$rc"
 done
fi
sha256sum --quiet --check /recipe/source.sha256
printf 'PINNED_COMPONENTS_COMPLETE profile=%s\n' "$profile"
