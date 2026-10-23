#pragma once
#include <stdint.h>

/* Exact d821 C1 native room contract. This only normalizes the native
 * search-closure publication; it never creates members, Ready or StartGame. */
static int CodolManagedQuickOpen(uint32_t caller, int main_room, int host,
    int native_controller, unsigned native_kind, int playlist_mode,
    const unsigned char *info)
{
    return info && (caller == 0x0071B121u || caller == 0x0071B312u) &&
        main_room && host && native_controller && native_kind == 1 && playlist_mode &&
        info[0x0c] == 11 && info[0x0d] == 2 && info[0x0e] == 12 && info[0x28] == 1;
}

static void CodolManagedQuickNormalize(unsigned char *info)
{
    info[0x29] = 0;
    info[0x2a] = 0;
}
