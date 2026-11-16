#!/bin/sh
# Every Kernel33 source test, on Windows (MSYS2 mingw32; the Quick fixture is an x86 Windows program):
#   sh tests/run_tests.sh
# The portable checks that this script does not run are listed in README.md ("Tests"). Exact C1 x86
# emulation requires an explicitly supplied private image; see docs.
set -e
cd "$(dirname "$0")/.."
BIN=${K33_MINGW:-/c/msys64/mingw32/bin}
OUT=${K33_TEST_OUT:-${TMP:-/tmp}/k33-tests}
mkdir -p "$OUT"
for PROFILE in 0 1; do
    "$BIN/gcc.exe" -std=c99 -O2 -Wall -Wextra -Werror -Wno-cast-function-type \
        -DK33_LOCAL_TESTING=$PROFILE tests/hostgate_windows_fixture.c \
        -o "$OUT/hostgate-$PROFILE.exe" -lws2_32 -liphlpapi
    python tests/test_hostgate_windows.py "$OUT/hostgate-$PROFILE.exe"
done
# Offline mode (docs/OFFLINE_LOBBY.md): the Kernel33 side in an owned fixture; tests/offline/ holds the
# lobby's own differential tests against the Python lobby (they need a private copy of it).
"$BIN/gcc.exe" -std=gnu99 -O2 -Wall -Wextra -Werror -Wno-unused-function -Wno-cast-function-type \
    tests/offline_lobby_fixture.c -o "$OUT/offline_lobby_fixture.exe" -lws2_32
rm -rf "$OUT/offline-lobby"
python tests/test_offline_lobby_windows.py "$OUT/offline_lobby_fixture.exe" "$OUT/offline-lobby"
# The lobby itself (src/offline/*.cpp) as the standalone dwtest.exe, on the resources embedded in it: the
# crypto, the listener on free ports (two at once, loopback only), the players-folder store (the career
# read back from mpdata_s), the rank write-back and the Local Play room policy. Each needs its own
# empty scratch folders.
rm -f "$OUT/dwtest.exe" # a failed build must not leave an older one to be tested
DWTEST_OBJ="$OUT/dwtest-obj" DWTEST_OUT="$OUT/dwtest.exe" sh tests/offline/build_dwtest.sh | grep -v '^g++' || true
"$OUT/dwtest.exe" --selftest
for T in net store career rooms; do
    rm -rf "$OUT/dwtest-$T"
    mkdir -p "$OUT/dwtest-$T/players" "$OUT/dwtest-$T/match" "$OUT/dwtest-$T/state"
    "$OUT/dwtest.exe" --profile "$OUT/dwtest-$T/players" --match-profile "$OUT/dwtest-$T/match" --state "$OUT/dwtest-$T/state" \
        --selftest-$T > "$OUT/dwtest-$T.log" 2>&1 || { tail -30 "$OUT/dwtest-$T.log"; exit 1; }
    tail -1 "$OUT/dwtest-$T.log"
done
# The lobby's weapon attachment table against the mp_weapons zone the client reads (the M14 EBR).
python -B tests/offline/test_m14_attach_table.py "$OUT/dwtest.exe"
python tests/test_ui_baseline.py
python tests/test_quick_ready_gate.py
node tests/campaign_local_host_test.js
node tests/local_match_controls_test.js
node tests/local_play_deferred_test.js
node tests/local_match_layout_test.js
node tests/local_create_room_option_test.js
node tests/ui_style_rows_test.js
python -B tests/test_survival_kiosk_menus.py
python -B tests/test_survival_wave_overrides.py
python -B tests/test_survival_hud.py
python -B tests/test_hud_styles.py
# The console in every profile and its command sync with the pinned match server (docs/OFFLINE_CONSOLE_20261112.md).
python -B tests/test_offline_console.py
"$BIN/gcc.exe" -std=c99 -O2 -Wall -Wextra -Werror -Wno-unused-function tests/hud_style_policy_test.c -o "$OUT/hud_style_policy_test.exe"
"$OUT/hud_style_policy_test.exe"
# The console's `name`: accepted names and the per-profile names file (src/player_name_policy.h).
"$BIN/gcc.exe" -std=c99 -O2 -Wall -Wextra -Werror -Wno-unused-function tests/player_name_policy_test.c -o "$OUT/player_name_policy_test.exe"
"$OUT/player_name_policy_test.exe"
python -B tests/test_survival_status_component.py
python -B tests/test_tip_stage_lookup.py
python -B tests/test_invitations_disabled.py
python -B tests/test_zone_packs.py
python -B tests/test_party_locales.py
# Backfill of marksman kills a career lost while IW4's challenge unlock gate was closed.
python -B tests/test_repair_locked_marksman.py
"$BIN/gcc.exe" -O2 -Wall -Wextra -Werror tests/quick_start_policy_test.c -o "$OUT/quick_start_policy.exe"
"$OUT/quick_start_policy.exe"
"$BIN/gcc.exe" -O2 -Wall -Wextra -Werror tests/controller_glyph_policy_test.c -o "$OUT/controller_glyph_policy.exe"
"$OUT/controller_glyph_policy.exe"
python -B docs/baselines/20261024-gfx-date-time/run_regressions.py \
    --cc "$BIN/gcc.exe" --output-dir "$OUT/gfx-date-time"
"$BIN/gcc.exe" -Wall -Wextra -Werror tests/patch_locale_test.c -o "$OUT/patch_locale_test.exe"
"$OUT/patch_locale_test.exe"
"$BIN/gcc.exe" -std=c99 -O2 -Wall -Wextra -Werror tests/locale_switch_policy_test.c -o "$OUT/locale_switch_policy_test.exe"
"$OUT/locale_switch_policy_test.exe"
"$BIN/gcc.exe" -O2 -Wall -Wextra -Werror tests/survival_category_test.c -o "$OUT/survival_category_test.exe"
"$OUT/survival_category_test.exe"
"$BIN/gcc.exe" -std=c99 -O2 -Wall -Wextra -Werror tests/test_p2p_match_options.c -o "$OUT/p2p_match_options_test.exe"
"$OUT/p2p_match_options_test.exe"
"$BIN/gcc.exe" -std=c99 -O2 -Wall -Wextra -Werror tests/test_p2p_status.c -o "$OUT/p2p_status_test.exe"
"$OUT/p2p_status_test.exe"
# managed_quick_windows.c needs its .qa section at 0x700000, where C1's room code is: as the first section of an
# image based at 0x6F0000 with 64 KB section alignment (.init is placed first in .text).
"$BIN/gcc.exe" -O0 -Wall -Wextra -c tests/managed_quick_windows.c -o "$OUT/managed_quick.o"
"$BIN/objcopy.exe" --rename-section .qa=.init "$OUT/managed_quick.o" "$OUT/managed_quick_init.o"
"$BIN/gcc.exe" -s -o "$OUT/managed_quick_windows.exe" "$OUT/managed_quick_init.o" \
    -Wl,--image-base=0x6f0000 -Wl,--section-alignment=0x10000 -Wl,--disable-dynamicbase
"$OUT/managed_quick_windows.exe"
if [ -n "${K33_C1_IMAGE:-}" ]; then
    "$BIN/gcc.exe" -O2 -fno-builtin -fno-stack-protector -shared -nostdlib \
        -Wl,--entry,0 -Wl,--image-base,0x20000000 tests/quick_start_x86_fixture.c \
        -o "$OUT/quick-start-x86.dll"
    python tests/test_quick_start_x86.py --fixture "$OUT/quick-start-x86.dll" \
        --image "$K33_C1_IMAGE" --output "$OUT/quick-start-x86.json"
else
    echo "exact private C1 x86 proof not run (set K33_C1_IMAGE); source checks only"
fi
# The survival category against C1.exe (1.1.1.13) itself: K33_C1_EXE, the dump C1.exe next to the game.
if [ -n "${K33_C1_EXE:-}" ]; then
    "$BIN/gcc.exe" -O2 -Wall -fno-builtin -fno-stack-protector -shared -nostdlib \
        -Wl,--entry,0 -Wl,--image-base,0x20000000 tests/survival_category_x86_fixture.c \
        -o "$OUT/survival-category-x86.dll"
    python -B tests/test_survival_category_x86.py --fixture "$OUT/survival-category-x86.dll" --image "$K33_C1_EXE"
    python -B tests/test_survival_crosshair_x86.py --fixture "$OUT/survival-category-x86.dll" --image "$K33_C1_EXE"
else
    echo "survival category C1 x86 proof not run (set K33_C1_EXE to 1.1.1.13's C1.exe)"
fi
echo "all Kernel33 source tests passed"
