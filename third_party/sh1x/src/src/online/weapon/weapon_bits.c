#include "weapon_bits.h"

#include <string.h>

#include "q_shared.h"
#include "qcommon_io.h"
#include "playerstate_ext.h"

uint32_t weapbits_result;

static unsigned bits_misses;
static unsigned bits_range;

/* A miss is a retained site that named the wrong register for its playerState -
 * the [A+B+disp] base/index ambiguity that crashed the server on 2026-09-15.
 * It cannot be silent: the site tag names the file and line so it is a one-line
 * fix rather than a hunt. WB_FILE is the small per-file integer that
 * weapon_bits.inc multiplies by a million. */
static void miss(const playerState_t *ps, uint32_t site)
{
    if (bits_misses++ < 8)
        Com_PrintWarning(CON_CHANNEL_SERVER,
            "CoDOL weapon inventory: site %u/%u passed playerState %p, which is "
            "not one of this server's playerState storages; that site names the "
            "wrong register. Treating the inventory as empty.\n",
            (unsigned)(site / 1000000u), (unsigned)(site % 1000000u),
            (const void *)ps);
}

static void out_of_range(uint32_t site, int index, const char *what)
{
    if (bits_range++ < 8)
        Com_PrintWarning(CON_CHANNEL_SERVER,
            "CoDOL weapon inventory: site %u/%u used %s %d, outside 0..%u\n",
            (unsigned)(site / 1000000u), (unsigned)(site % 1000000u),
            what, index, (unsigned)WEAPON_SLOTS - 1u);
}

static uint32_t *array_of(ps_ext_t *ext, int array)
{
    switch (array) {
    case WB_ARRAY_OWNED: return ext->weapons;
    case WB_ARRAY_OLD: return ext->weaponold;
    case WB_ARRAY_RECHAMBER: return ext->weaponrechamber;
    default: return NULL;
    }
}

uint32_t WeaponBitsRead(const playerState_t *ps, int array, int word, uint32_t site)
{
    ps_ext_t *ext = PlayerStateExt(ps);
    const uint32_t *words;
    if (!ext) { miss(ps, site); return 0; }
    if (word < 0 || (unsigned)word >= WEAPON_BIT_WORDS) {
        out_of_range(site, word * 32, "weapon");
        return 0;
    }
    words = array_of(ext, array);
    return words ? words[word] : 0;
}

void WeaponBitsOr(const playerState_t *ps, int array, int word, uint32_t bits, uint32_t site)
{
    ps_ext_t *ext = PlayerStateExt(ps);
    uint32_t *words;
    if (!ext) { miss(ps, site); return; }
    if (word < 0 || (unsigned)word >= WEAPON_BIT_WORDS) {
        out_of_range(site, word * 32, "weapon");
        return;
    }
    words = array_of(ext, array);
    if (!words) return;
    words[word] |= bits;
    ext->weapons_known = 1;
}

void WeaponBitsAnd(const playerState_t *ps, int array, int word, uint32_t mask, uint32_t site)
{
    ps_ext_t *ext = PlayerStateExt(ps);
    uint32_t *words;
    if (!ext) { miss(ps, site); return; }
    if (word < 0 || (unsigned)word >= WEAPON_BIT_WORDS) {
        out_of_range(site, word * 32, "weapon");
        return;
    }
    words = array_of(ext, array);
    if (!words) return;
    words[word] &= mask;
    ext->weapons_known = 1;
}

uint32_t WeaponModelRead(const playerState_t *ps, int weapon, uint32_t site)
{
    const ps_ext_t *ext = PlayerStateExtConst(ps);
    if (!ext) { miss(ps, site); return 0; }
    if (weapon < 0 || (unsigned)weapon >= WEAPON_SLOTS) {
        out_of_range(site, weapon, "weapon");
        return 0;
    }
    return ext->weaponmodels[weapon];
}

void WeaponModelWrite(const playerState_t *ps, int weapon, uint32_t model, uint32_t site)
{
    ps_ext_t *ext = PlayerStateExt(ps);
    if (!ext) { miss(ps, site); return; }
    if (weapon < 0 || (unsigned)weapon >= WEAPON_SLOTS) {
        out_of_range(site, weapon, "weapon");
        return;
    }
    ext->weaponmodels[weapon] = (unsigned char)model;
    ext->weapons_known = 1;
}

/* ----------------------------------------------------------------- C side */

static int test_bit(const playerState_t *ps, int array, unsigned weapon)
{
    const ps_ext_t *ext;
    const uint32_t *words;
    if (weapon >= WEAPON_SLOTS) return 0;
    ext = PlayerStateExtConst(ps);
    if (!ext) return 0;
    switch (array) {
    case WB_ARRAY_OWNED: words = ext->weapons; break;
    case WB_ARRAY_OLD: words = ext->weaponold; break;
    case WB_ARRAY_RECHAMBER: words = ext->weaponrechamber; break;
    default: return 0;
    }
    return (words[weapon >> 5] & (UINT32_C(1) << (weapon & 31))) != 0;
}

static void set_bit(const playerState_t *ps, int array, unsigned weapon, int on)
{
    ps_ext_t *ext;
    uint32_t *words;
    if (weapon >= WEAPON_SLOTS) return;
    ext = PlayerStateExt(ps);
    if (!ext) return;
    words = array_of(ext, array);
    if (!words) return;
    if (on) words[weapon >> 5] |= UINT32_C(1) << (weapon & 31);
    else words[weapon >> 5] &= ~(UINT32_C(1) << (weapon & 31));
    ext->weapons_known = 1;
}

int WeaponBitsOwned(const playerState_t *ps, unsigned weapon)
{ return test_bit(ps, WB_ARRAY_OWNED, weapon); }
int WeaponBitsOldTest(const playerState_t *ps, unsigned weapon)
{ return test_bit(ps, WB_ARRAY_OLD, weapon); }
int WeaponBitsRechamberTest(const playerState_t *ps, unsigned weapon)
{ return test_bit(ps, WB_ARRAY_RECHAMBER, weapon); }
void WeaponBitsOwnedSet(const playerState_t *ps, unsigned weapon, int owned)
{ set_bit(ps, WB_ARRAY_OWNED, weapon, owned); }
void WeaponBitsOldSet(const playerState_t *ps, unsigned weapon, int old)
{ set_bit(ps, WB_ARRAY_OLD, weapon, old); }
void WeaponBitsRechamberSet(const playerState_t *ps, unsigned weapon, int rechamber)
{ set_bit(ps, WB_ARRAY_RECHAMBER, weapon, rechamber); }

unsigned WeaponBitsModelGet(const playerState_t *ps, unsigned weapon)
{
    const ps_ext_t *ext;
    if (weapon >= WEAPON_SLOTS) return 0;
    ext = PlayerStateExtConst(ps);
    return ext ? ext->weaponmodels[weapon] : 0u;
}

void WeaponBitsModelSet(const playerState_t *ps, unsigned weapon, unsigned model)
{
    ps_ext_t *ext;
    if (weapon >= WEAPON_SLOTS) return;
    ext = PlayerStateExt(ps);
    if (!ext) return;
    ext->weaponmodels[weapon] = (unsigned char)model;
    ext->weapons_known = 1;
}

uint32_t WeaponBitsOwnedWord(const playerState_t *ps, unsigned word)
{
    const ps_ext_t *ext;
    if (word >= WEAPON_BIT_WORDS) return 0;
    ext = PlayerStateExtConst(ps);
    return ext ? ext->weapons[word] : 0u;
}

void WeaponBitsInventoryClear(const playerState_t *ps)
{
    ps_ext_t *ext = PlayerStateExt(ps);
    if (!ext) return;
    memset(ext->weapons, 0, sizeof(ext->weapons));
    memset(ext->weaponold, 0, sizeof(ext->weaponold));
    memset(ext->weaponrechamber, 0, sizeof(ext->weaponrechamber));
    memset(ext->weaponmodels, 0, sizeof(ext->weaponmodels));
    ext->weapons_known = 1;
}

unsigned WeaponBitsMisses(void) { return bits_misses + bits_range; }
void WeaponBitsResetMisses(void) { bits_misses = 0; bits_range = 0; }
