#!/bin/sh
# Sourced after entering the source directory; no installation side effects.
case "${K33_LOCAL_TESTING:-0}" in 0|1) ;; *) echo "K33_LOCAL_TESTING must be 0 or 1" >&2; exit 1;; esac
K33_DIAGNOSTICS=${K33_DIAGNOSTICS:-${K33_LOCAL_TESTING:-0}}
case "$K33_DIAGNOSTICS" in 0|1) ;; *) echo "K33_DIAGNOSTICS must be 0 or 1" >&2; exit 1;; esac
if [ "${K33_LOCAL_TESTING:-0}" = 0 ] && [ "$K33_DIAGNOSTICS" = 1 ]; then
    echo "Shipping builds cannot enable diagnostics" >&2
    exit 1
fi
case "${K33_KERNEL_ROOM_CONTEXT:-0}" in 0|1) ;; *) echo "K33_KERNEL_ROOM_CONTEXT must be 0 or 1" >&2; exit 1;; esac
case "${K33_KERNEL_NAME_DISPLAY:-0}" in 0|1) ;; *) echo "K33_KERNEL_NAME_DISPLAY must be 0 or 1" >&2; exit 1;; esac
if [ "${K33_KERNEL_NAME_DISPLAY:-0}" = 1 ] && [ "${K33_KERNEL_ROOM_CONTEXT:-0}" != 1 ]; then
    echo "Name-display migration requires K33_KERNEL_ROOM_CONTEXT=1" >&2
    exit 1
fi
if [ "${K33_KERNEL_ROOM_CONTEXT:-0}" = 1 ] && [ "${K33_NO_COPY:-0}" != 1 ]; then
    echo "Room-context migration is not release-qualified; build only with K33_NO_COPY=1" >&2
    exit 1
fi
case "${K33_OFFLINE:-0}" in 0|1) ;; *) echo "K33_OFFLINE must be 0 or 1" >&2; exit 1;; esac
set -- "-DCODOL_OFFLINE_LOBBY=${K33_OFFLINE:-0}" "-DCODOL_LOCAL_TESTING=${K33_LOCAL_TESTING:-0}" "-DCODOL_DIAGNOSTICS=$K33_DIAGNOSTICS" "-DCODOL_KERNEL_ROOM_CONTEXT=${K33_KERNEL_ROOM_CONTEXT:-0}" "-DCODOL_KERNEL_NAME_DISPLAY=${K33_KERNEL_NAME_DISPLAY:-0}"
if [ -n "${K33_LAUNCHER_HOST_SHA256:-}" ]; then
    case "$K33_LAUNCHER_HOST_SHA256" in *[!0-9a-f]*) echo "Launcher pin must be lowercase SHA-256" >&2; exit 1;; esac
    [ "${#K33_LAUNCHER_HOST_SHA256}" = 64 ] || { echo "Launcher pin must contain 64 hex characters" >&2; exit 1; }
    set -- "$@" "-DCODOL_LAUNCHER_BRIDGE_HOST_SHA256=\"$K33_LAUNCHER_HOST_SHA256\""
fi

# Public exact-package pins, generated after the qualified Launcher/Host builds.
# Omission compiles the explicit zero-format refusal policy.
if [ -n "${K33_P2P_PRODUCTION_POLICY_HEADER:-}" ]; then
    [ "$K33_P2P_PRODUCTION_POLICY_HEADER" = production/p2p_production_policy.generated.h ] || { echo "Production policy must use the fixed source path" >&2; exit 1; }
    [ -f "$K33_P2P_PRODUCTION_POLICY_HEADER" ] || { echo "Production policy header is missing" >&2; exit 1; }
    set -- "$@" "-DCODOL_P2P_PRODUCTION_POLICY_HEADER=\"$K33_P2P_PRODUCTION_POLICY_HEADER\""
fi
