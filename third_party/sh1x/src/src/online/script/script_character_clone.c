/* <script_model> codol_setCharacterClone( [player [, weapons]] ). See the
 * header for the C1 evidence.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "script_character_clone.h"
#include "player_worldweapon.h"
#include "scr_vm_functions.h"
#include "cscr_variable.h"
#include "cscr_stringlist.h"
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

extern void G_DObjUpdate(gentity_t *entity);
extern int BG_FindWeaponIndexForName(const char *name);

/* At most this many named weapons; C1 links two world guns per client. */
#define CLONE_WEAPONS 8

/* The clone's server skeleton. C1 draws the clone from the client's
 * character record, but this server links the world guns to tags of the
 * clone's own server DObj and animates it, so that DObj needs the whole
 * outfit: the player's model plus its tagless character parts (the parts
 * PublishCharacterComposition attaches), not its tagged script
 * attachments. Nothing here is published for a script mover. */
static void copy_character_parts(gentity_t *clone, const gentity_t *player)
{
    unsigned i, n = 0;
    clone->model = player->model;
    for(i = 0; i < 19; ++i) {
        if(!player->attachModelNames[i] || player->attachTagNames[i]) continue;
        clone->attachModelNames[n++] = player->attachModelNames[i];
    }
    for(i = 0; i < 19; ++i) {
        if(i >= n) clone->attachModelNames[i] = 0;
        Scr_SetString(&clone->attachTagNames[i], 0);
    }
    clone->attachIgnoreCollision = ~0u;
    G_DObjUpdate(clone);
}

/* C1 0x55b71f..0x55b8b4: one string, or an array whose elements must all be
 * strings ("All elements need to be strings (weapon names)"). A name the
 * server never registered resolves to nothing and is skipped; this is the
 * find-only lookup, so no weapon slot is spent. */
static int read_weapons(unsigned *weapons, unsigned *count)
{
    *count = 0;
    if(Scr_GetType(1) == VAR_STRING || Scr_GetType(1) == VAR_ISTRING) {
        int weapon = BG_FindWeaponIndexForName(Scr_GetString(1));
        if(weapon > 0) weapons[(*count)++] = (unsigned)weapon;
        return 1;
    }
    if(Scr_GetType(1) == VAR_POINTER && Scr_GetObjectType(Scr_GetObject(1)) == VAR_ARRAY) {
        unsigned child;
        for(child = FindFirstSibling(Scr_GetObject(1)); child; child = FindNextSibling(child)) {
            union VariableUnion *value;
            int weapon;
            if(GetValueType(child) != VAR_STRING) {
                Scr_ParamError(1, "All elements need to be strings (weapon names)");
                return 0;
            }
            value = GetVariableValueAddress(child);
            weapon = BG_FindWeaponIndexForName(SL_ConvertToString(value->stringValue));
            if(weapon > 0 && *count < CLONE_WEAPONS) weapons[(*count)++] = (unsigned)weapon;
        }
        return 1;
    }
    Scr_ParamError(1, "codol_setCharacterClone: weapons must be a weapon name or an array of them");
    return 0;
}

void Scr_SetCharacterClone(scr_entref_t ref)
{
    gentity_t *self, *player;
    gentity_t *linked[CLONE_WEAPONS];
    unsigned weapons[CLONE_WEAPONS], weaponCount = 0, count, i;
    int named = 0;

    if(ref.classnum || ref.entnum >= MAX_GENTITIES) {
        Scr_ObjectError("codol_setCharacterClone: not an entity");
        return;
    }
    self = &g_entities[ref.entnum];
    /* C1 0x43e809 reads +64 only in the eType 6 arm, and a brush mover has no
     * character skeleton to compose on. */
    if(!self->r.inuse || self->s.eType != ET_SCRIPTMOVER || self->classname != scr_const.script_model ||
       self->s.solid == SOLID_BMODEL) {
        Scr_ObjectError(va("codol_setCharacterClone: entity %i is not a model script_model", ref.entnum));
        return;
    }
    if(Scr_GetNumParam() > 2) {
        Scr_Error("codol_setCharacterClone takes at most two arguments");
        return;
    }
    if(Scr_GetNumParam() == 0 || Scr_GetType(0) == VAR_UNDEFINED) {
        self->s.lerp.u.scriptMover.characterClient = 0;
        Scr_MakeArray();
        return;
    }
    player = Scr_GetEntity(0);
    if(!player || !player->client || !player->r.inuse) {
        Scr_ParamError(0, "codol_setCharacterClone: the source must be a connected player");
        return;
    }
    /* C1 0x4437c0 treats an index of 0x13 or more as an actor slot. */
    if(player->s.number < 0 || player->s.number + 1 > CHARACTER_CLONE_MAX) {
        Scr_ParamError(0, va("codol_setCharacterClone: client %i is past C1's %i character clones",
                             player->s.number, CHARACTER_CLONE_MAX));
        return;
    }
    if(Scr_GetNumParam() == 2 && Scr_GetType(1) != VAR_UNDEFINED) {
        if(!read_weapons(weapons, &weaponCount)) return;
        named = 1;
    }

    self->s.lerp.u.scriptMover.characterClient = player->s.number + 1;
    if(player->model) copy_character_parts(self, player);

    /* C1 returns the world guns it linked (0x55b896..0x55b8af). */
    count = LinkCloneWorldWeapons(player, self, named ? weapons : NULL, weaponCount,
                                        linked, CLONE_WEAPONS);
    Scr_MakeArray();
    for(i = 0; i < count; ++i) {
        Scr_AddEntity(linked[i]);
        Scr_AddArray();
    }
}

/* <entity> codol_sendToAll( <milliseconds> ): send this entity to every client
 * for the given time, whatever their PVS (r.broadcastTime, which
 * SV_AddEntitiesVisibleFromPoint honours before the PVS test); 0 stops it.
 *
 * Why a clone needs it: C1's 0x4437c0 does not read a character record by
 * client number. It uses value - 1 as an ENTITY number into the client's
 * entity-DObj map (0x25d9ed8, the same map the animation commands use at
 * 0x46b0f1) and copies that entity's DObj models. When the source player has
 * no DObj on that client - not in its snapshot, or already a spectator - the
 * clone falls back to its own single xmodel (0x443965). 0x42bdf0 rebuilds a
 * clone only while the source DObj exists and has changed, so the source has
 * to be in every client's snapshot when the clone first appears; afterwards
 * the clone keeps the copy. */
void Scr_SendToAll(scr_entref_t ref)
{
    gentity_t *self;
    int ms;
    if(ref.classnum || ref.entnum >= MAX_GENTITIES || !g_entities[ref.entnum].r.inuse) {
        Scr_ObjectError("codol_sendToAll: not an entity");
        return;
    }
    self = &g_entities[ref.entnum];
    ms = Scr_GetInt(0);
    if(ms < 0) {
        Scr_ParamError(0, "codol_sendToAll: time must not be negative");
        return;
    }
    self->r.broadcastTime = ms ? svs.time + ms : 0;
}
#endif
