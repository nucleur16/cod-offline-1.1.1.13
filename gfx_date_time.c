/* Included by kernel33.c before TakeOver. Exact C1 AS3 Date.setTime repair.
 * The original TimeClip/epoch/NaN implementation remains executable and intact.
 * Only the member initializer is redirected; no timezone/system clock is set.
 * Date receiver/output pointers exist only for this synchronous native call. */
#include <stdint.h>
#include "gfx_date_time_policy.h"
#include "gfx_date_time_guards.h"

#define GDT_SET_TIME_VA 0x0088A640u
#define GDT_INITIALIZER_VA 0x00B36E50u
#define GDT_MEMBER_VA 0x00CE7D38u
typedef DWORD (WINAPI *GDTGetDynamicFn)(PDYNAMIC_TIME_ZONE_INFORMATION);
typedef BOOL (WINAPI *GDTToLocalFn)(const DYNAMIC_TIME_ZONE_INFORMATION *, const SYSTEMTIME *, LPSYSTEMTIME);
typedef double *(__attribute__((thiscall)) *GDTSetTimeFn)(void *, double *, double);
static GDTGetDynamicFn g_gdtGetDynamic;
static GDTToLocalFn g_gdtToLocal;

static int GfxDateTargetOffset(double clipped, int32_t *offset)
{
    uint64_t ticks, local_ticks;
    FILETIME utc_file, local_file;
    SYSTEMTIME utc, local;
    DYNAMIC_TIME_ZONE_INFORMATION zone;
    if (!g_gdtGetDynamic || !g_gdtToLocal || !gdt_filetime(clipped, &ticks))
        return 0;
    utc_file.dwLowDateTime = (DWORD)ticks;
    utc_file.dwHighDateTime = (DWORD)(ticks >> 32);
    if (!FileTimeToSystemTime(&utc_file, &utc) ||
        g_gdtGetDynamic(&zone) == TIME_ZONE_ID_INVALID) return 0;
    /* The Ex API applies a named zone's registered rules even when the
     * supplied snapshot disables DST. Honor that explicit OS preference. */
    if (zone.DynamicDaylightTimeDisabled) {
        int64_t standard = -((int64_t)zone.Bias + zone.StandardBias) * 60000;
        if (standard < -86400000 || standard > 86400000) return 0;
        *offset = (int32_t)standard;
        return 1;
    }
    if (!g_gdtToLocal(&zone, &utc, &local) ||
        !SystemTimeToFileTime(&local, &local_file)) return 0;
    local_ticks = ((uint64_t)local_file.dwHighDateTime << 32) | local_file.dwLowDateTime;
    return gdt_offset(ticks, local_ticks, offset);
}

/* ECX=self; the unused fastcall EDX slot leaves the measured thiscall stack
 * unchanged: output pointer followed by Number(double), callee pops12 bytes.
 * The original returns that same output pointer in EAX on every branch. */
__attribute__((used,noinline)) double *__attribute__((fastcall))
GfxDateSetTime(void *self, void *unused_edx, double *output, double milliseconds)
{
    double *result;
    double clipped;
    int32_t offset;
    (void)unused_edx;
    result = ((GDTSetTimeFn)(DWORD_PTR)GDT_SET_TIME_VA)(self, output, milliseconds);
    memcpy(&clipped, (const BYTE *)self + 0x28, sizeof(clipped));
    if (GfxDateTargetOffset(clipped, &offset))
        memcpy((BYTE *)self + 0x20, &offset, sizeof(offset));
    return result;
}

static uint32_t GfxDateGuardHash(const BYTE *bytes, uint32_t length)
{
    uint32_t hash = 2166136261u;
    while (length--) { hash ^= *bytes++; hash *= 16777619u; }
    return hash;
}

static void PatchGfxDateTime(void)
{
    HMODULE kernel;
    DWORD replacement;
    if (!Readable((const void *)(DWORD_PTR)GDT_MEMBER_VA, 8) ||
        *(const DWORD *)(DWORD_PTR)GDT_MEMBER_VA ||
        *(const DWORD *)(DWORD_PTR)(GDT_MEMBER_VA + 4)) {
        Log("Date.setTime: initializer already ran; retained native behavior");
        return;
    }
    for (unsigned i = 0; i < sizeof(g_gdtGuards) / sizeof(g_gdtGuards[0]); ++i) {
        const GfxDateGuard *guard = &g_gdtGuards[i];
        const BYTE *code = (const BYTE *)(DWORD_PTR)guard->va;
        uint32_t hash;
        if (!Readable(code, guard->length)) {
            Log("Date.setTime: unreadable exact %s; retained native behavior", guard->name);
            return;
        }
        hash = GfxDateGuardHash(code, guard->length);
        if (hash != guard->shipping_hash && hash != guard->retained_hash) {
            Log("Date.setTime: exact %s differs; retained native behavior", guard->name);
            return;
        }
    }
    kernel = GetModuleHandleA("kernel32.dll");
    g_gdtGetDynamic = kernel ? (GDTGetDynamicFn)(void *)GetProcAddress(kernel, "GetDynamicTimeZoneInformation") : NULL;
    g_gdtToLocal = kernel ? (GDTToLocalFn)(void *)GetProcAddress(kernel, "SystemTimeToTzSpecificLocalTimeEx") : NULL;
    if (!g_gdtGetDynamic || !g_gdtToLocal) {
        Log("Date.setTime: historical timezone APIs unavailable; retained native behavior");
        return;
    }
    /* Preserve all19 initializer bytes except the four-byte target immediate.
     * The existing CRT initializer will install this member before AS3 use. */
    replacement = (DWORD)(DWORD_PTR)GfxDateSetTime;
    WriteCode((void *)(DWORD_PTR)(GDT_INITIALIZER_VA + 1), &replacement, 4);
    Log("Date.setTime: exact member initialized with target-date timezone cache repair");
}
