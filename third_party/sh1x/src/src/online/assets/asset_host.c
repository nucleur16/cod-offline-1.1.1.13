/* Asset access for an in-process host (sh1x_host.dll, Kernel33 feat/p2p-inprocess).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Stage 2 of docs/IW4_ASSET_LAYOUT.md converts the server's assets, one type at a time, from the retained
 * IW3 structures to the layout C1 itself holds in memory. For a converted type the host can compare the server's
 * record with C1's own (the check that the layout is right) and, in stage 3, hand the server C1's record instead.
 * Types are C1 asset type numbers (docs/ASSET_DISPATCH.md). Converted so far: 11 sound, 28 weapon, 36 rawfile, 37 string table. */
#include <stddef.h>
#include "sound_native.h"
#include "asset_host.h"
#include "c1_records.h"
#include "c1_weapon.h"

const void *HostFindAsset(unsigned type,const char *name)
{
    if(!name || !*name) return NULL;
    switch(type) {
    case 11: return FindEngineSoundAlias(name);
    case 36: { const C1RawFile *r=NULL; return LoadC1RawFile(name,&r)==1 ? r : NULL; }
    case 37: { const C1StringTable *t=NULL; return LoadC1StringTable(name,&t)==1 ? t : NULL; }
    case 28: { const void *w=NULL; return LoadC1WeaponDef(name,&w)==1 ? w : NULL; }
    default: return NULL;
    }
}

int HostEnumerateAssets(unsigned type,int (*visit)(void *,const char *),void *context)
{
    if(!visit) return -1;
    switch(type) {
    case 11: return EnumerateMountedSoundAliases(visit,context);
    case 36: case 37: return EnumerateMountedNamed(type,visit,context);
    case 28: return EnumerateMountedWeapons(visit,context);
    /* other types: the catalogued definitions, for the generic comparison of C1-layout header copies */
    default: return EnumerateMountedType(type,visit,context);
    }
}
