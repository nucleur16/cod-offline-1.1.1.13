#ifndef CODOL_GFX_DATE_TIME_POLICY_H
#define CODOL_GFX_DATE_TIME_POLICY_H
#include <stdint.h>

/* The original Date.setTime performs TimeClip first. Never alter its epoch,
 * rounding or NaN result. The OS conversion is attempted only for integral
 * clipped milliseconds representable by Windows' signed FILETIME calendar. */
static int gdt_filetime(double milliseconds, uint64_t *ticks)
{
    const int64_t epoch_ms = INT64_C(11644473600000);
    const int64_t max_file_ms = INT64_MAX / INT64_C(10000);
    int64_t value;
    if (milliseconds != milliseconds ||
        milliseconds < -(double)epoch_ms ||
        milliseconds > (double)(max_file_ms - epoch_ms))
        return 0;
    value = (int64_t)milliseconds;
    if ((double)value != milliseconds) return 0;
    *ticks = (uint64_t)(value + epoch_ms) * UINT64_C(10000);
    return 1;
}

static int gdt_offset(uint64_t utc, uint64_t local, int32_t *milliseconds)
{
    int64_t delta;
    if (utc > (uint64_t)INT64_MAX || local > (uint64_t)INT64_MAX)
        return 0;
    delta = (int64_t)local - (int64_t)utc;
    if (delta % INT64_C(10000) || delta < -INT64_C(864000000000) ||
        delta > INT64_C(864000000000)) return 0;
    *milliseconds = (int32_t)(delta / INT64_C(10000));
    return 1;
}
#endif
