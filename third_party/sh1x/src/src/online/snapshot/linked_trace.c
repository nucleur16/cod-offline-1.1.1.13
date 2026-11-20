/* ol_debugTrace 1: a linked player's published playerState and the
 * trajectory of what it is linked to, at most four lines a second per client.
 *
 * Why: during the coop intro ride (maps/_coopintro.gsc, the rider linked to a
 * script_model mover with playerLinkToDelta) C1 logged a prediction origin
 * error on every client frame equal to that frame's milliseconds times the
 * jeep's 264 units a second (Kernel33 parity log, session 13, 23:34:03-22).
 * C1 PmoveSingle's linked arm (0x4165f0..0x41665a) never moves the origin and
 * clears groundEntityNum (0x416604); CG_PredictPlayerState carries the
 * snapshot origin along ps.groundEntityNum's trajectory before the replay
 * (0x45f544, CG_AdjustPositionForMover 0x440120 for eType 6/13, 0x67d920 for
 * 14) and back for the error check (0x45f5f8) only while that ground is still
 * set. These lines say what the rider's snapshot actually carries.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "q_shared.h"
#include "qcommon.h"
/* Last: server.h leaves pack(1) behind it. */
#include "server.h"
#include "g_shared.h"

void LinkedSnapshotTrace(unsigned slot,const playerState_t *ps)
{
    static int last[MAX_CLIENTS];
    const gentity_t *ent,*parent,*ground;
    if(!OL_TRACE_ON() || !ps || slot>=MAX_CLIENTS || (ps->pm_type!=1 && ps->pm_type!=8)) return;
    if(svs.time-last[slot]<250 && svs.time>=last[slot]) return;
    last[slot]=svs.time;
    ent=&g_entities[slot];
    parent=ent->tagInfo ? ent->tagInfo->parent : NULL;
    ground=ps->groundEntityNum>=0 && ps->groundEntityNum<ENTITYNUM_WORLD ?
        &g_entities[ps->groundEntityNum] : NULL;
    Com_Printf(CON_CHANNEL_SERVER,
        "CoDOL linked ps: client %u server %d cmd %d pm_type %d ground %d (eType %d tr %d) "
        "parent %d (eType %d tr %d time %d dur %d delta %.2f %.2f %.2f) origin (%.3f %.3f %.3f) "
        "vel (%.1f %.1f %.1f) eFlags 0x%x\n",
        slot,svs.time,ps->commandTime,ps->pm_type,ps->groundEntityNum,
        ground ? ground->s.eType : -1,ground ? (int)ground->s.lerp.pos.trType : -1,
        parent ? parent->s.number : -1,parent ? parent->s.eType : -1,
        parent ? (int)parent->s.lerp.pos.trType : -1,parent ? parent->s.lerp.pos.trTime : 0,
        parent ? parent->s.lerp.pos.trDuration : 0,
        parent ? parent->s.lerp.pos.trDelta[0] : 0.0f,parent ? parent->s.lerp.pos.trDelta[1] : 0.0f,
        parent ? parent->s.lerp.pos.trDelta[2] : 0.0f,
        ps->origin[0],ps->origin[1],ps->origin[2],
        ps->velocity[0],ps->velocity[1],ps->velocity[2],(unsigned)ps->eFlags);
}
