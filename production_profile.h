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
#if (CODOL_LOCAL_TESTING != 0 && CODOL_LOCAL_TESTING != 1) || (CODOL_DIAGNOSTICS != 0 && CODOL_DIAGNOSTICS != 1)
#error Client profile flags must be zero or one
#endif
#if !CODOL_LOCAL_TESTING && CODOL_DIAGNOSTICS
#error Diagnostics require the explicit local development profile
#endif
#endif
