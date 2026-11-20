#include "player.h"
#include "g_shared.h"
#include "bg_public.h"
#include "server_public.h"
#include "qcommon_io.h"
#ifdef OL_SERVER
#include "player_lerp.h"
#endif


extern "C"{

int __cdecl G_ClientCanSpectateTeamOrLocalPlayer(gclient_t *client, clientState_t *cs)
{
  return G_ClientCanSpectateTeam(client, cs->team);
}

qboolean GetFollowPlayerState(int clientNum, playerState_t *ps)
{
  gclient_t *client;
  unsigned int index;

    assert((signed int)clientNum < level.maxclients);

    client = &level.clients[clientNum];
    assert(client);
    assert(client == g_entities[clientNum].client);

  if ( client->ps.otherFlags & POF_PLAYER )
  {
    memcpy(ps, &client->ps, sizeof(playerState_t));
    for ( index = 0; index < 31 && ps->hud.current[index].type; ++index )
    {
      memset(&ps->hud.current[index], 0, sizeof(ps->hud.current[0]));
	    assert(ps->hud.current[index].type == HE_TYPE_FREE);
    }
/*
    while ( index < 31 )
    {
        assert(!memcmp( &ps->hud.current[index], &g_dummyHudCurrent, sizeof( g_dummyHudCurrent ) ));
      ++index;
    }
*/
    return qtrue;
  }
  memset(ps, 0, sizeof(playerState_t));
  return qfalse;
}


void __cdecl ClientThink(int clientNum)
{
  struct gentity_s *ent;

//  assert(Sys_IsServerThread());

  ent = &g_entities[clientNum];

  assert(ent->client);

  assert(bgs == NULL);

  bgs = &level_bgs;
  memcpy(&ent->client->sess.oldcmd, &ent->client->sess.cmd, sizeof(ent->client->sess.oldcmd));
  SV_GetUsercmd(clientNum, &ent->client->sess.cmd);
  ent->client->lastCmdTime = level.time;
  if ( !g_synchronousClients->boolean )
  {
    ClientThink_real(ent, &ent->client->sess.cmd);
  }

  assert(bgs == &level_bgs);

  bgs = NULL;
}


void __cdecl G_PlayerStateToEntityStateExtrapolate(playerState_s *ps, entityState_s *s, int time, int snap)
{
#ifdef OL_SERVER
  /* A player's entity carries its position at ps.commandTime, and a human's
   * commandTime only moves when a usermove packet arrives: C1 batches ~3
   * commands per ~35 ms packet, so at sv_fps 60 the published position
   * advances {0, 0, ~35 ms} from snapshot to snapshot (PacingSnapshot,
   * docs/SWAY_CAUSE_SWEEP_20260916.md R1d). A bot thinks once per
   * frame and advances evenly. C1 interpolates a remote player whose
   * pos.trType is TR_LINEAR_STOP (0x44337e, entity number < 19) by
   * evaluating each snapshot's trajectory at that snapshot's serverTime
   * (0x442ac0 -> BG_EvaluateTrajectory 0x40ca40; mode 3 clamps at
   * trTime + trDuration and never runs backwards), so the trajectory must
   * say when trBase was true and how fast it was moving, or the human is
   * drawn holding still for two snapshots and then jumping.
   *
   * The retained BG_PlayerStateToEntityState (like C1 0x40d190 and IW3)
   * writes TR_INTERPOLATE and zeroes trTime/trDuration/trDelta, so these are
   * stored after it. Storing them first (as C1 0x50bf60 and CoD4x did)
   * published trTime 0, trDuration 0, trDelta 0: no extrapolation at all.
   * The window covers one C1 packet gap (C1's own value is 50 ms) and is
   * never shorter than one server frame. */
  BG_PlayerStateToEntityState(ps, s, snap, 1);
  s->lerp.pos.trType = TR_LINEAR_STOP;
  s->lerp.pos.trTime = time;
  s->lerp.pos.trDuration = level.framerate > 0 && 1000 / level.framerate > 50 ? 1000 / level.framerate : 50;
  VectorCopy(ps->velocity, s->lerp.pos.trDelta);
  if ( snap )
  {
    /* Whole units, as trBase is: a steady run then repeats the same words
     * and the delta coder sends nothing for them. */
    s->lerp.pos.trDelta[0] = (float)(int)s->lerp.pos.trDelta[0];
    s->lerp.pos.trDelta[1] = (float)(int)s->lerp.pos.trDelta[1];
    s->lerp.pos.trDelta[2] = (float)(int)s->lerp.pos.trDelta[2];
  }
#else
  VectorCopy(ps->velocity, s->lerp.pos.trDelta);

  s->lerp.pos.trTime = time;
  s->lerp.pos.trDuration = 1000 / level.framerate;
  BG_PlayerStateToEntityState(ps, s, snap, 1);
  s->lerp.pos.trType = TR_LINEAR_STOP;
#endif
}


void G_UpdateEntityStateFromPlayerState(struct gclient_s *client, struct gentity_s *ent)
{
#ifdef OL_SERVER
      /* Every executed command and every frame's republish passes through
       * here: record the pose the snapshot writer republishes on an even
       * timeline (player_lerp.h). The entity state built below stays
       * the live one, since ClientThink_real links the entity from it. */
      PlayerLerpCapture(ent->s.number, client->ps.commandTime, client->ps.pm_type,
                              client->ps.eFlags, client->ps.origin, client->ps.viewangles,
                              client->ps.velocity);
#endif
      if ( g_smoothClients->boolean )
      {
        G_PlayerStateToEntityStateExtrapolate(&client->ps, &ent->s, client->ps.commandTime, 1);
      }
      else
      {
        BG_PlayerStateToEntityState(&client->ps, &ent->s, 1, 1);
      }
}

};


