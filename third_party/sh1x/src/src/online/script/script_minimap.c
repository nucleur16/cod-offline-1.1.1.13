/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "script_minimap.h"
#include "scr_vm_functions.h"
#include "sv_state.h"
#include "server.h"
#include "g_shared.h"
#include "scr_vm.h"
#include "cscr_stringlist.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

void Scr_SetNorthYaw(void)
{
    float yaw=Scr_GetFloat(0);
    char value[32];
    float radians;
    /* IW4 5f36a0 publishes %g then updates compassNorth via sin/cos.
     * Retained getnorthyaw/worldspawn use 822; the CoDOL publisher maps
     * that native slot to 1302 for gamestate and reliable updates. */
    snprintf(value,sizeof(value),"%g",(double)yaw);
    SV_SetConfigstring(822,value);
    radians=(float)((double)yaw*0.01745329238474369);
    level.compassNorth[0]=(float)cos((double)radians);
    level.compassNorth[1]=(float)sin((double)radians);
}

static const char *minimap_icon(unsigned slot)
{
    unsigned index=CS_MINIMAP_ICONS+slot;
    const char *name=GetConfigstring(ServerConfigstrings(),index);
    if(!name) name=GetConfigstring(ServerInheritedConfigstrings(),index);
    return name ? name : "";
}
static unsigned plane_icon(unsigned argument)
{
    const char *name;unsigned slot;
    if(Scr_GetNumParam()<=argument || Scr_GetType(argument)==VAR_UNDEFINED)return 0;
    name=Scr_GetString(argument);
    if(!*name)return 0;
    /* C1 54ffb0: distinct 15-entry minimap bank, not material indices. */
    for(slot=0;slot<MINIMAP_ICON_SLOTS;++slot)
        if(!Q_stricmp(minimap_icon(slot),name))return slot+1;
    Scr_Error(va("Minimap icon '%s' was not precached",name));
    return 0;
}
void Scr_SpawnPlane(void)
{
    gentity_t *owner=Scr_GetEntity(0),*plane;
    unsigned classname,friendly,enemy;int flags=0,ownerIndex;vec3_t origin;
    if(!owner->client){Scr_ParamError(0,"Owner entity is not a player");return;}
    classname=Scr_GetConstString(1);
    Scr_GetVector(2,origin);
    friendly=plane_icon(3);enemy=plane_icon(4);
    if(Scr_GetNumParam()>5)flags=Scr_GetInt(5);
    ownerIndex=(int)(owner->client-level.clients);
    if(ownerIndex<0 || ownerIndex>=level.maxclients){Scr_ParamError(0,"Invalid plane owner client");return;}
    plane=G_Spawn();
    Scr_SetString(&plane->classname,classname);
    VectorCopy(origin,plane->r.currentOrigin);
    plane->spawnflags=flags;
    if(!G_CallSpawnEntity(plane)){
        G_FreeEntity(plane);
        Scr_Error(va("unable to spawn \"%s\" entity",SL_ConvertToString(classname)));
        return;
    }
    /* C1 54c8ee/54c8f1/54c8f4: owner, friendly, enemy. No team packing. */
    plane->s.eType=ET_PLANE;
    plane->s.lerp.u.plane.ownerClient=ownerIndex;
    plane->s.lerp.u.plane.enemyIcon=(int)enemy;
    plane->s.lerp.u.plane.friendlyIcon=(int)friendly;
    Scr_AddEntity(plane);
}
unsigned RegisterMinimapIcon(const char *name)
{
    const char *p;
    unsigned slot, index;
    int client;
    if(!name || !*name)return 0;
    /* C1 54ff80 ->54f9d0: whole-bank case-insensitive duplicate pass before
     * first-empty search. Empty input matches an empty slot without writing. */
    for(slot=0;slot<MINIMAP_ICON_SLOTS;++slot)
        if(!Q_stricmp(minimap_icon(slot),name)) return slot+1;
    for(slot=0;slot<MINIMAP_ICON_SLOTS;++slot)
        if(!*minimap_icon(slot)) break;
    if(slot==MINIMAP_ICON_SLOTS) {
        Scr_Error("Too many minimap icon precached. Max allowed is 15");
        return 0;
    }
    /* Use the existing asset-name command representation. Reject names that
     * cannot be represented as one reliable-command token before mutation. */
    if(strlen(name)>MAX_STRING_CHARS-32) {
        Scr_Error("Minimap icon name is too long"); return 0;
    }
    for(p=name;*p;++p)
        if((unsigned char)*p<=32 || *p==127 || *p=='"' || *p==';' || *p=='\\') {
            Scr_Error("Invalid minimap icon name"); return 0;
        }
    index=CS_MINIMAP_ICONS+slot;
    if(!OL_SetConfigstring(ServerConfigstrings(),index,name)) {
        Scr_Error("Could not register minimap icon"); return 0;
    }
    if(!svs.clients || !sv_maxclients) return slot+1;
    for(client=0;client<sv_maxclients->integer;++client) {
        client_t *target=&svs.clients[client];
        if(target->state>=CS_PRIMED && target->gamestateSent)
            SV_SendServerCommandNoLoss(target,"d %u %s",index,name);
    }
    return slot+1;
}
void Scr_PrecacheMinimapIcon(void)
{
    RegisterMinimapIcon(Scr_GetString(0));
}
#endif
