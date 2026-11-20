#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* <script_model> codol_setOverheadName( [name] ). See the header for the C1
 * evidence.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "script_overhead_name.h"
#include "script_character_clone.h"
#include "sv_state.h"
#include "scr_vm_functions.h"
#include "qcommon_io.h"
#include <string.h>
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"

#define OVERHEAD_SLOTS CHARACTER_CLONE_MAX /* clients 0..0x12 */
#define OVERHEAD_NAME_CHARS 64

static char reserved[OVERHEAD_SLOTS][OVERHEAD_NAME_CHARS];
static int reservedLevel = -1;

static int slot_connected(unsigned slot)
{
    return (int)slot < sv_maxclients->integer && svs.clients[slot].state >= CS_CONNECTED;
}

/* A new level starts with no reservations: the movers of the last one are
 * gone, and its slots may now belong to players. */
static void sync_level(void)
{
    if(reservedLevel != level.startTime) {
        memset(reserved, 0, sizeof(reserved));
        reservedLevel = level.startTime;
    }
}

void OverheadNamesReset(void)
{
    memset(reserved, 0, sizeof(reserved));
    reservedLevel = -1;
}

const char *OverheadNameForSlot(unsigned slot)
{
    sync_level();
    if(slot >= OVERHEAD_SLOTS || !reserved[slot][0]) return NULL;
    return reserved[slot];
}

static void clear_movers(unsigned slot)
{
    int i;
    for(i = 0; i < level.num_entities; ++i) {
        gentity_t *ent = &g_entities[i];
        if(!ent->r.inuse || ent->s.eType != ET_SCRIPTMOVER || ent->classname != scr_const.script_model)
            continue;
        if(ent->s.lerp.u.scriptMover.characterClient == (int)slot + 1)
            ent->s.lerp.u.scriptMover.characterClient = 0;
    }
}

void OverheadNameSlotTaken(unsigned slot)
{
    sync_level();
    if(slot >= OVERHEAD_SLOTS || !reserved[slot][0]) return;
    ((void)0);
    reserved[slot][0] = 0;
    clear_movers(slot);
}

/* The highest slot no client occupies that is free or already carries the
 * name; -1 when none is left. */
static int reserve(const char *name)
{
    int slot;
    for(slot = OVERHEAD_SLOTS - 1; slot >= 0; --slot)
        if(reserved[slot][0] && !strcmp(reserved[slot], name)) return slot;
    for(slot = OVERHEAD_SLOTS - 1; slot >= 0; --slot) {
        if(reserved[slot][0] || slot_connected((unsigned)slot)) continue;
        Q_strncpyz(reserved[slot], name, OVERHEAD_NAME_CHARS);
        return slot;
    }
    return -1;
}

void Scr_SetOverheadName(scr_entref_t ref)
{
    gentity_t *self;
    const char *zone = ServerZoneType();
    const char *name;
    int slot;

    if(ref.classnum || ref.entnum >= MAX_GENTITIES) {
        Scr_ObjectError("codol_setOverheadName: not an entity");
        return;
    }
    self = &g_entities[ref.entnum];
    if(!self->r.inuse || self->s.eType != ET_SCRIPTMOVER || self->classname != scr_const.script_model ||
       self->s.solid == SOLID_BMODEL) {
        Scr_ObjectError(va("codol_setOverheadName: entity %i is not a model script_model", ref.entnum));
        return;
    }
    /* The field doubles as C1's character-clone selector; only the coop
     * category, whose friendly AI this exists for, may use it this way. */
    if(!zone || strcmp(zone, "coop")) {
        Scr_Error("codol_setOverheadName is available in coop matches only");
        return;
    }
    sync_level();
    if(Scr_GetNumParam() == 0 || Scr_GetType(0) == VAR_UNDEFINED) {
        self->s.lerp.u.scriptMover.characterClient = 0;
        return;
    }
    name = Scr_GetString(0);
    if(!name || !*name) {
        self->s.lerp.u.scriptMover.characterClient = 0;
        return;
    }
    slot = reserve(name);
    if(slot < 0) {
        Com_PrintWarning(CON_CHANNEL_SERVER,
            "CoDOL overhead name: no free name slot for '%s' (%i clients); not shown\n",
            name, sv_maxclients->integer);
        self->s.lerp.u.scriptMover.characterClient = 0;
        return;
    }
    self->s.lerp.u.scriptMover.characterClient = slot + 1;
    /* Publish now, so a call made while the level initializes is in the
     * first gamestate rather than in a later "d" command. */
    RefreshServerPlayerNames();
}

#endif /* OL_SERVER */

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}

int PersistentTailOverheadEmpty(void){return reservedLevel==-1&&resident_tail_zero(reserved,sizeof(reserved));}
void PersistentTailOverheadReset(void){OverheadNamesReset();}
#endif
