#!/bin/sh
# Builds the diagnostic profile (K33_LOCAL_TESTING=1 K33_DIAGNOSTICS=1, docs/PRODUCTION_PROFILE.md) with
# scripts/build.sh's source set and flags as build/Kernel33_dev.dll (developer console, k33shot, parity logger),
# and the same profile without the console (-DK33_NO_CONSOLE) as build/Kernel33_noconsole.dll; it fails when that
# one does not build, i.e. when the diagnostic code depends on src/console/console.c. Nothing is copied next to the
# game: install by hand (cp ../Kernel33.dll ../Kernel33.dll.pre-dev && cp build/Kernel33_dev.dll ../Kernel33.dll).
# build/Kernel33.dll is left as the dev build. Extra arguments go to gcc, as for scripts/build.sh.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
K33_NO_COPY=1
K33_LOCAL_TESTING=${K33_LOCAL_TESTING:-1}
K33_DIAGNOSTICS=${K33_DIAGNOSTICS:-$K33_LOCAL_TESTING}
export K33_NO_COPY K33_LOCAL_TESTING K33_DIAGNOSTICS
sh "$ROOT/scripts/build.sh" "$@" -DK33_NO_CONSOLE \
    || { echo "FAILED: the build without console.c (-DK33_NO_CONSOLE) does not link"; exit 1; }
cp "$ROOT/build/Kernel33.dll" "$ROOT/build/Kernel33_noconsole.dll"
sh "$ROOT/scripts/build.sh" "$@"
cp "$ROOT/build/Kernel33.dll" "$ROOT/build/Kernel33_dev.dll"
echo "built build/Kernel33_dev.dll and build/Kernel33_noconsole.dll (not installed)"
