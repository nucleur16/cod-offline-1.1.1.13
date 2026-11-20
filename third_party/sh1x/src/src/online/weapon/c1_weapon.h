/* WeaponDef in C1's memory layout. SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef C1_WEAPON_H
#define C1_WEAPON_H
#ifdef __cplusplus
extern "C" {
#endif
enum { C1_WEAPON_SIZE=0xbfc };
/* The server's record of weapon `name` in C1's layout (see c1_weapon.c); borrowed until the mounted set
 * changes. 1 ready, 0 absent, -1 malformed. */
int LoadC1WeaponDef(const char *name,const void **out);
/* Slot kind at a record offset: 0 scalar, 1 string, 2 sound, 3 sound array, 4 asset, 5 points. */
unsigned C1WeaponSlotKind(unsigned offset);
void ResetC1Weapons(void);
int EnumerateMountedWeapons(int (*visit)(void *,const char *),void *context);
/* Registry helpers (asset_registry.c). */
unsigned AssetMountSerial(void);
int EnumerateMountedType(unsigned type,int (*visit)(void *,const char *),void *context);
#ifdef __cplusplus
}
#endif
#endif
