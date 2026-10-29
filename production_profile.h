#ifndef CODOL_PRODUCTION_PROFILE_H
#define CODOL_PRODUCTION_PROFILE_H
#ifndef CODOL_LOCAL_TESTING
#define CODOL_LOCAL_TESTING 0
#endif
#ifndef CODOL_DIAGNOSTICS
#define CODOL_DIAGNOSTICS CODOL_LOCAL_TESTING
#endif
#if (CODOL_LOCAL_TESTING != 0 && CODOL_LOCAL_TESTING != 1) || (CODOL_DIAGNOSTICS != 0 && CODOL_DIAGNOSTICS != 1)
#error Client profile flags must be zero or one
#endif
#if !CODOL_LOCAL_TESTING && CODOL_DIAGNOSTICS
#error Diagnostics require the explicit local development profile
#endif
#endif
