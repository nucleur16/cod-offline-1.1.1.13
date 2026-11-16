#ifndef PRODUCTION_PROFILE_H
#define PRODUCTION_PROFILE_H
#ifndef K33_LOCAL_TESTING
#define K33_LOCAL_TESTING 0
#endif
#ifndef K33_DIAGNOSTICS
#define K33_DIAGNOSTICS K33_LOCAL_TESTING
#endif
#ifndef K33_KERNEL_ROOM_CONTEXT
/* Keep the migration opt-in until complete proxy/name-display parity passes. */
#define K33_KERNEL_ROOM_CONTEXT 0
#endif
#ifndef K33_KERNEL_NAME_DISPLAY
#define K33_KERNEL_NAME_DISPLAY 0
#endif
#if K33_KERNEL_NAME_DISPLAY != 0 && K33_KERNEL_NAME_DISPLAY != 1
#error Kernel name-display migration flag must be zero or one
#endif
#if K33_KERNEL_NAME_DISPLAY && !K33_KERNEL_ROOM_CONTEXT
#error Kernel name display requires the main-thread room-context binder
#endif
#if K33_KERNEL_ROOM_CONTEXT != 0 && K33_KERNEL_ROOM_CONTEXT != 1
#error Kernel room-context migration flag must be zero or one
#endif
#ifndef K33_OFFLINE_LOBBY
/* The in-client lobby (offline_lobby.c, docs/OFFLINE_LOBBY.md). This client is offline only: every
 * build has it and it is always active. The macro is not a build option any more (no script sets
 * it); it only lets an owned test fixture compile one source file without the lobby (define it 0). */
#define K33_OFFLINE_LOBBY 1
#endif
#if K33_OFFLINE_LOBBY != 0 && K33_OFFLINE_LOBBY != 1
#error Offline lobby flag must be zero or one
#endif
#if (K33_LOCAL_TESTING != 0 && K33_LOCAL_TESTING != 1) || (K33_DIAGNOSTICS != 0 && K33_DIAGNOSTICS != 1)
#error Client profile flags must be zero or one
#endif
#if !K33_LOCAL_TESTING && K33_DIAGNOSTICS
#error Diagnostics require the explicit local development profile
#endif
#endif
