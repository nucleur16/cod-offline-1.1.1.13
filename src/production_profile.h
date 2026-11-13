#ifndef CODOL_PRODUCTION_PROFILE_H
#define CODOL_PRODUCTION_PROFILE_H
#ifndef CODOL_LOCAL_TESTING
#define CODOL_LOCAL_TESTING 0
#endif
#ifndef CODOL_DIAGNOSTICS
#define CODOL_DIAGNOSTICS CODOL_LOCAL_TESTING
#endif
#ifndef CODOL_KERNEL_ROOM_CONTEXT
/* Keep the migration opt-in until complete proxy/name-display parity passes. */
#define CODOL_KERNEL_ROOM_CONTEXT 0
#endif
#ifndef CODOL_KERNEL_NAME_DISPLAY
#define CODOL_KERNEL_NAME_DISPLAY 0
#endif
#if CODOL_KERNEL_NAME_DISPLAY != 0 && CODOL_KERNEL_NAME_DISPLAY != 1
#error Kernel name-display migration flag must be zero or one
#endif
#if CODOL_KERNEL_NAME_DISPLAY && !CODOL_KERNEL_ROOM_CONTEXT
#error Kernel name display requires the main-thread room-context binder
#endif
#if CODOL_KERNEL_ROOM_CONTEXT != 0 && CODOL_KERNEL_ROOM_CONTEXT != 1
#error Kernel room-context migration flag must be zero or one
#endif
#ifndef CODOL_OFFLINE_LOBBY
/* The in-client lobby (offline_lobby.c, docs/OFFLINE_LOBBY.md). This client is offline only: every
 * build has it and it is always active. The macro is not a build option any more (no script sets
 * it); it only lets an owned test fixture compile one source file without the lobby (define it 0). */
#define CODOL_OFFLINE_LOBBY 1
#endif
#if CODOL_OFFLINE_LOBBY != 0 && CODOL_OFFLINE_LOBBY != 1
#error Offline lobby flag must be zero or one
#endif
#if (CODOL_LOCAL_TESTING != 0 && CODOL_LOCAL_TESTING != 1) || (CODOL_DIAGNOSTICS != 0 && CODOL_DIAGNOSTICS != 1)
#error Client profile flags must be zero or one
#endif
#if !CODOL_LOCAL_TESTING && CODOL_DIAGNOSTICS
#error Diagnostics require the explicit local development profile
#endif
#endif
