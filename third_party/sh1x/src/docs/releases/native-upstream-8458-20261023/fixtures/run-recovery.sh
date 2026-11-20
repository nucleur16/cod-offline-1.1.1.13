#!/usr/bin/env bash
set -euo pipefail
inc=(-I/src/src)
for area in /src/src/codol/*; do [[ ! -d "$area" ]] || inc+=("-I$area"); done
flags=(-m32 -msse2 -O1 -g -DCODOL_SERVER -D_GNU_SOURCE -ffunction-sections -fdata-sections)
for variant in normal asan; do
  san=()
  if [[ "$variant" == asan ]]; then san=(-fsanitize=address,undefined -fno-sanitize=alignment --param asan-globals=0 -fno-omit-frame-pointer -no-pie); fi
  gcc "${flags[@]}" "${san[@]}" "${inc[@]}" /recipe/test_server_recovery.c -Wl,--gc-sections -o /out/recovery-$variant > /out/recovery-$variant-build.log 2>&1
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 /out/recovery-$variant > /out/recovery-$variant-run.log 2>&1
  cat /out/recovery-$variant-run.log
done
