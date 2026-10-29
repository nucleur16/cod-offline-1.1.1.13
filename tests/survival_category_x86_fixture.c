/* Offline x86 fixture for the survival category: survival_category.c (install, byte checks, hook, callback) as it
 * ships, with the Windows boundaries as deterministic doubles. tests/test_survival_category_x86.py maps it next to
 * C1.exe in Unicorn and runs C1's own 0x4CCDF0 / 0x4B6380 / 0x612850 with the hook installed and without it. */
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
__declspec(dllexport) DWORD stest_writes, stest_logs;
__declspec(dllexport) char stest_cmdline[256] = "codoMP_client_shipRetail.exe +set dw_username test";
void *memcpy(void *d, const void *s, size_t n) { BYTE *a = d; const BYTE *b = s; for (size_t i = 0; i < n; i++) a[i] = b[i]; return d; }
void *memset(void *d, int v, size_t n) { BYTE *a = d; for (size_t i = 0; i < n; i++) a[i] = (BYTE)v; return d; }
int memcmp(const void *a, const void *b, size_t n) { const BYTE *x = a, *y = b; for (size_t i = 0; i < n; i++) if (x[i] != y[i]) return x[i] - y[i]; return 0; }
static char *TStrstr(const char *h, const char *n)
{
    for (; *h; h++) {
        const char *a = h, *b = n;
        while (*a && *b && *a == *b) a++, b++;
        if (!*b) return (char *)h;
    }
    return NULL;
}
/* C1's image: the only memory the install may read */
static BOOL Readable(const void *p, SIZE_T n)
{
    uintptr_t v = (uintptr_t)p;
    return (v >= 0x400000 && v + n <= 0x0A000000) ||
           (v >= 0x30000000 && v + n <= 0x31000000);
}
static void Log(const char *format, ...) { (void)format; stest_logs++; }
static void WriteCode(void *dst, const void *src, SIZE_T n) { memcpy(dst, src, n); stest_writes++; }
static LPSTR TCmdLine(void) { return stest_cmdline; }
#define GetCommandLineA TCmdLine
#define strstr TStrstr
#include "../survival_category.c"

__declspec(dllexport) void __cdecl STestInstall(void) { PatchSurvivalCategory(); }
__declspec(dllexport) long __cdecl STestZoneLists(void) { return g_k33SurvZoneLists; }
