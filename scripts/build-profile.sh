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
# The in-client lobby (offline/, docs/OFFLINE_LOBBY.md) is part of every build: this client is offline only.
# Arguments given to scripts/build.sh are kept: they reach gcc ahead of the profile defines.
set -- "$@" "-DCODOL_LOCAL_TESTING=${K33_LOCAL_TESTING:-0}" "-DCODOL_DIAGNOSTICS=$K33_DIAGNOSTICS" "-DCODOL_KERNEL_ROOM_CONTEXT=${K33_KERNEL_ROOM_CONTEXT:-0}" "-DCODOL_KERNEL_NAME_DISPLAY=${K33_KERNEL_NAME_DISPLAY:-0}"
