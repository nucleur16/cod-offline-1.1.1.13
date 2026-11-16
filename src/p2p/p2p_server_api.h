/*
 * p2p_server_api.h - the sh1x match server's host API, linked into Kernel33.dll (included by p2p_inproc.c).
 *
 * The server repo's makefile target host-lib (bin/libsh1x_host.a, server win32/sys_host.c) is the dedicated
 * server's objects as one relocatable object in which only this API is global; scripts/build.sh links it. A
 * Kernel33.dll built without it (K33_SERVER=0, or no server build: -DK33_NO_SERVER=1) gets stubs: hosting then
 * fails with "built without the match server".
 */
#ifndef K33_P2P_SERVER_API_H
#define K33_P2P_SERVER_API_H

#if !K33_NO_SERVER
int __cdecl Sh1xHost_Init(const char *cmdline);
int __cdecl Sh1xHost_Frame(void);
void __cdecl Sh1xHost_Command(const char *text);
int __cdecl Sh1xHost_ExitCode(void);
unsigned __cdecl Sh1xHost_OnDemandBytes(void);
const void *__cdecl Sh1xHost_FindAsset(unsigned type, const char *name);
int __cdecl Sh1xHost_EnumerateAssets(unsigned type, int (__cdecl *visit)(void *, const char *), void *context);
const void *__cdecl Sh1xHost_FindC1Header(unsigned type, const char *name, unsigned *length);
const void *__cdecl Sh1xHost_ResolveC1Pointer(const void *addr, unsigned *kind, unsigned *size);
int __cdecl Sh1xHost_C1PointersIn(const void *addr, unsigned length, unsigned *offsets, unsigned max);
const void *__cdecl Sh1xHost_FindC1Record(unsigned type, const char *name);
void __cdecl Sh1xHost_SetC1AssetProvider(const void *(__cdecl *find)(unsigned, const char *));
void __cdecl Sh1xHost_SetC1ZoneHost(int (__cdecl *await_zones)(const char *, const char *, unsigned),
                                    int (__cdecl *zone_held)(const char *));
void __cdecl Sh1xHost_SetC1ZoneFind(const char *(__cdecl *zone_of)(unsigned, const char *));
void __cdecl Sh1xHost_SetC1ScriptStrings(const char *(__cdecl *script_string)(unsigned));
#define K33_HAS_SERVER 1
#else
static int __cdecl Sh1xHost_Init(const char *cmdline) { (void)cmdline; return -100; }
static int __cdecl Sh1xHost_Frame(void) { return 0; }
static void __cdecl Sh1xHost_Command(const char *text) { (void)text; }
static int __cdecl Sh1xHost_ExitCode(void) { return 0; }
static unsigned __cdecl Sh1xHost_OnDemandBytes(void) { return 0; }
static const void *__cdecl Sh1xHost_FindAsset(unsigned t, const char *n) { (void)t; (void)n; return NULL; }
static int __cdecl Sh1xHost_EnumerateAssets(unsigned t, int (__cdecl *v)(void *, const char *), void *c) { (void)t; (void)v; (void)c; return -1; }
static const void *__cdecl Sh1xHost_FindC1Header(unsigned t, const char *n, unsigned *l) { (void)t; (void)n; (void)l; return NULL; }
static const void *__cdecl Sh1xHost_ResolveC1Pointer(const void *a, unsigned *k, unsigned *s) { (void)a; (void)k; (void)s; return NULL; }
static int __cdecl Sh1xHost_C1PointersIn(const void *a, unsigned l, unsigned *o, unsigned m) { (void)a; (void)l; (void)o; (void)m; return -1; }
static const void *__cdecl Sh1xHost_FindC1Record(unsigned t, const char *n) { (void)t; (void)n; return NULL; }
static void __cdecl Sh1xHost_SetC1AssetProvider(const void *(__cdecl *f)(unsigned, const char *)) { (void)f; }
static void __cdecl Sh1xHost_SetC1ZoneHost(int (__cdecl *a)(const char *, const char *, unsigned), int (__cdecl *h)(const char *)) { (void)a; (void)h; }
static void __cdecl Sh1xHost_SetC1ZoneFind(const char *(__cdecl *z)(unsigned, const char *)) { (void)z; }
static void __cdecl Sh1xHost_SetC1ScriptStrings(const char *(__cdecl *s)(unsigned)) { (void)s; }
#define K33_HAS_SERVER 0
#endif

#endif
