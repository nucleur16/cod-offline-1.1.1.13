#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Inverse of C1.exe MSG_WriteDeltaUsercmdKey 0x0059FA30.
 * Branch/key order and field widths were independently inspected in C1:
 * unchanged 59fbb6, short delta 59fbd8, full delta 59fc52;
 * three 10-bit weapon IDs 59fd74..59fe88; optional payloads 59fe8b..59ff95.
 * Trailing tags are 7/2/5. Python usercmd_codec.py is a secondary reference,
 * not the authority for the client wire format. This is not a runtime claim.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "weapon_slots.h"   /* WEAPON_SLOTS */
#include "usercmd.h"
#include <string.h>

static uint32_t delta_bits(ol_msg_t *m, uint32_t key, uint32_t old, unsigned n)
{
    return ReadBits(m, 1) ?
        (ReadBits(m, n) ^ (key & ((UINT32_C(1) << n) - 1))) : old;
}
static uint16_t delta_short(ol_msg_t *m, uint32_t key, uint16_t old)
{
    return ReadBits(m, 1) ? (uint16_t)(ReadShort(m) ^ key) : old;
}
static uint8_t delta_byte(ol_msg_t *m, uint32_t key, uint8_t old)
{
    return ReadBits(m, 1) ? (uint8_t)(ReadByte(m) ^ key) : old;
}

int ReadUsercmd(ol_msg_t *m, uint32_t key,
                      const ol_usercmd_t *from, ol_usercmd_t *to)
{
    ol_msg_t r = *m;
    ol_usercmd_t c = *from;
    uint32_t k, fire, upper;
    unsigned expected = 7, i;
    c.serverTime = ReadBits(&r, 1) ? from->serverTime + ReadByte(&r) :
                                         ReadLong(&r);
    if (ReadBits(&r, 1) ^ (key & 1)) {
        if (!(ReadBits(&r, 1) ^ (key & 1))) {
            expected = 2;
            k = key ^ c.serverTime;
            c.buttons = (from->buttons & ~UINT32_C(1)) | (ReadBits(&r, 1) ^ (k & 1));
            c.angles[0] = delta_short(&r, k, from->angles[0]);
            c.angles[1] = delta_short(&r, k, from->angles[1]);
            c.direction = delta_bits(&r, k, from->direction, 4);
        } else {
            expected = 5;
            fire = ReadBits(&r, 1) ^ (key & 1);
            c.angles[0] = delta_short(&r, key, from->angles[0]);
            c.angles[1] = delta_short(&r, key, from->angles[1]);
            c.direction = delta_bits(&r, key, from->direction, 4);
            k = key ^ c.serverTime;
            c.angles[2] = delta_short(&r, k, from->angles[2]);
            upper = delta_bits(&r, k, from->buttons >> 1, 29);
            c.buttons = upper * 2 | fire;
            c.weapon = delta_bits(&r, k, from->weapon, 10);
            c.offHandIndex = delta_bits(&r, k, from->offHandIndex, 10);
            c.altModeWeapon = delta_bits(&r, k, from->altModeWeapon, 10);
            if (c.buttons & 0x20000) {
                for (i = 0; i < 2; ++i)
                    c.selectedLocation[i] = delta_byte(&r, k, from->selectedLocation[i]);
                c.selectedLocationAngle = delta_byte(&r, k, from->selectedLocationAngle);
            }
            if (c.buttons & 0x200000)
                for (i = 0; i < 2; ++i)
                    c.remoteAngles[i] = delta_byte(&r, k, from->remoteAngles[i]);
            if (c.buttons & 4) {
                c.meleeChargeYaw = delta_short(&r, k, from->meleeChargeYaw);
                c.meleeChargeDist = delta_bits(&r, k, from->meleeChargeDist, 8);
            }
        }
    }
    if (ReadBits(&r, 3) != expected || r.failed) {
        m->failed = 1; return 0;
    }
    *to = c; *m = r; return 1;
}

uint32_t UsercmdKey(uint32_t ack, uint32_t checksum,
                         const uint8_t *command, size_t length)
{
    uint32_t hash = 0; size_t i;
    if (length > 32) length = 32;
    if (command) for (i = 0; i < length && command[i]; ++i) {
        uint32_t c = command[i];
        /* Hash differs from raw-byte netchan XOR: sanitize '%' and high bytes. */
        if (c & 128 || c == '%') c = '.';
        hash += c * (119 + (uint32_t)i);
    }
    return ack ^ checksum ^ hash ^ (hash >> 10) ^ (hash >> 20);
}

void FilterUsercmd(const ol_usercmd_t *wire, ol_usercmd_t *game)
{
    *game = *wire;
    if (wire->buttons & 0x200000) {
        game->buttons &= 0x80300; game->direction = 0;
    } else if (wire->buttons & 0x10000000) {
        game->buttons &= 0x10001b02; game->direction = 0;
    }
}

/* Store native index + 1 so uninitialized static storage means unmapped. */
static uint16_t weapon_map[WEAPON_IDS];
static uint16_t reverse_weapon_map[WEAPON_SLOTS];
static unsigned weapon_map_generation;

unsigned WeaponMapGeneration(void) { return weapon_map_generation; }
void ResetWeaponMap(void)
{
    ++weapon_map_generation;
    memset(weapon_map, 0, sizeof(weapon_map));
    memset(reverse_weapon_map, 0, sizeof(reverse_weapon_map));
}
int MapWeapon(unsigned wire, unsigned engine)
{
    if (!wire || !engine) return wire == 0 && engine == 0;
    if (wire >= WEAPON_IDS || engine >= WEAPON_SLOTS) return 0;
    if ((weapon_map[wire] && weapon_map[wire] != engine + 1) ||
        (reverse_weapon_map[engine] && reverse_weapon_map[engine] != wire + 1)) return 0;
    weapon_map[wire] = engine + 1; reverse_weapon_map[engine] = wire + 1;
    ++weapon_map_generation;
    return 1;
}
int ReplaceWeaponMap(const int *wire_by_engine,unsigned count)
{
    uint16_t forward[WEAPON_IDS]={0},reverse[WEAPON_SLOTS]={0}; unsigned engine;
    if(!wire_by_engine || !count || count>WEAPON_SLOTS || wire_by_engine[0]!=0) return 0;
    for(engine=1;engine<count;++engine) {
        int wire=wire_by_engine[engine];
        if(wire<=0 || wire>=WEAPON_IDS || forward[wire]) return 0;
        forward[wire]=(uint16_t)(engine+1); reverse[engine]=(uint16_t)(wire+1);
    }
    memcpy(weapon_map,forward,sizeof(forward)); memcpy(reverse_weapon_map,reverse,sizeof(reverse));
    ++weapon_map_generation; return 1;
}
int WeaponToEngine(unsigned wire)
{
    if (!wire) return 0;
    return wire < WEAPON_IDS && weapon_map[wire] ? weapon_map[wire] - 1 : -1;
}
int WeaponToWire(unsigned engine)
{
    if (!engine) return 0;
    return engine < WEAPON_SLOTS && reverse_weapon_map[engine] ?
        reverse_weapon_map[engine] - 1 : -1;
}

#ifdef OL_SERVER
#include "persistent_statics.h"

static int resident_static_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentWeaponMapsEmpty(void) {return resident_static_zero(weapon_map,sizeof(weapon_map)) && resident_static_zero(reverse_weapon_map,sizeof(reverse_weapon_map));}
#endif
