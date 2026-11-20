#!/usr/bin/env bash
# Pinned compiler only; all game/key/source inputs mounted read-only, network none.
set -euo pipefail
cd /tmp
sha256sum --quiet --check /recipe/qa.sha256
includes=(-I/src/src)
for area in /src/src/codol/*; do [[ ! -d "$area" ]] || includes+=(-I"$area"); done
inflate=(/src/src/zlib/inflate.c /src/src/zlib/inffast.c /src/src/zlib/inftrees.c /src/src/zlib/adler32.c /src/src/zlib/crc32.c)
for loader in old corrected; do
    fastfile=/src/src/codol/assets/codol_fastfile.c
    [[ "$loader" != old ]] || fastfile=/unsigned-donor/src/codol/assets/codol_fastfile.c
    gcc -m32 -O2 "${includes[@]}" /recipe/archive_probe.c /src/src/codol/assets/codol_assets.c "$fastfile" /src/src/codol/assets/codol_client_crypto.c "${inflate[@]}" -L/src/lib-native -lmbedcrypto -o "/out/archive-$loader" > "/out/archive-$loader.build.log" 2>&1
    set +e
    "/out/archive-$loader" /qa/archive.ff /key/C1.exe > "/out/archive-$loader.json" 2> "/out/archive-$loader.stderr"
    status=$?
    set -e
    printf '%s\n' "$status" > "/out/archive-$loader.exit"
    if [[ "$loader" == old ]]; then [[ "$status" == 1 ]]; else [[ "$status" == 0 ]]; fi
done
nasm -f elf32 -dCODOL_SERVER /recipe/retained.asm -o /out/retained.o
common=(-m32 -msse2 -O1 -g -DCODOL_SERVER -D_GNU_SOURCE -ffunction-sections -fdata-sections -fno-pie -no-pie)
for version in donor corrected; do
    source=/src
    [[ "$version" != donor ]] || source=/animation-donor
    gcc "${common[@]}" "${includes[@]}" /recipe/test_anim_receiver.c "$source/src/codol/script/codol_script_anim.c" /src/src/codol/assets/codol_model_animation.c /out/retained.o -Wl,--gc-sections -lm -o "/out/animation-$version" > "/out/animation-$version.build.log" 2>&1
    if [[ "$version" == donor ]]; then /out/animation-donor --donor; else /out/animation-corrected; fi > "/out/animation-$version.log" 2>&1
done
printf 'REAL_UNSIGNED_ARCHIVE_AND_NATIVE_ANIMATION_REGRESSION_PASS\n'
