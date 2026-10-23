#!/bin/sh
# Sourced after entering the source directory; no installation side effects.
case "${K33_LOCAL_TESTING:-0}" in 0|1) ;; *) echo "K33_LOCAL_TESTING must be 0 or 1" >&2; exit 1;; esac
set -- "-DCODOL_LOCAL_TESTING=${K33_LOCAL_TESTING:-0}"
if [ -n "${K33_LAUNCHER_HOST_SHA256:-}" ]; then
    case "$K33_LAUNCHER_HOST_SHA256" in *[!0-9a-f]*) echo "Launcher pin must be lowercase SHA-256" >&2; exit 1;; esac
    [ "${#K33_LAUNCHER_HOST_SHA256}" = 64 ] || { echo "Launcher pin must contain 64 hex characters" >&2; exit 1; }
    set -- "$@" "-DCODOL_LAUNCHER_BRIDGE_HOST_SHA256=\"$K33_LAUNCHER_HOST_SHA256\""
fi
