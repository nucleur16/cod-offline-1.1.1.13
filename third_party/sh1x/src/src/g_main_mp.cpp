#include "g_shared.h"
#include "qcommon_io.h"
#include "qcommon.h"
#include "qcommon_logprint.h"

#include "server_public.h"
#include "cscr_stringlist.h"
#ifdef OL_SERVER
#include "script_sound.h"
#include "script_startup_extra.h"
#include "script_vehicle_builtins.h"
#include "script_model_scale.h"
#include "clip_box.h"
extern "C" {
#include "bot_nav.h"
}
#include "script_player_builtins.h"
#include "sentient.h"
#include "actor_damage.h"
#include "actor_lifecycle.h"
#include "actor_audit.h"
#include "actor_ragdoll.h"
#include "vehicle_spawner.h"
#endif

level_locals_t level;

const char* g_dedicatedEnumNames[] = { "listen server", "dedicated LAN server", "dedicated internet server" };


extern "C"
{

#ifdef OL_SERVER
void ResetEntityEventGeneration(unsigned entity_number);
int PhysicsLaunchOwnsEntity(const gentity_t *ent);
void G_Damage(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
    const float *dir, const float *point, int damage, int dflags, int mod, int weapon,
    int hitLoc, unsigned int modelIndex, unsigned int partName, int timeOffset);
#endif

#define MAX_REDIRECTDESTINATIONS 4

static void (*rd_destinations[MAX_REDIRECTDESTINATIONS])( const char *buffer, int len );

void G_PrintRedirect(char* msg, int len)
{

    int i;

    for(i = 0; i < MAX_REDIRECTDESTINATIONS; i++)
    {
        if(rd_destinations[i] == NULL)
            return;

        rd_destinations[i](msg, len);

    }

}
/*
    To Add:
    HL2Rcon_SourceRconSendGameLog(string, stringlen);
*/


void G_PrintAddRedirect(void (*rd_dest)( const char *, int))
{
    int i;

    for(i = 0; i < MAX_REDIRECTDESTINATIONS; i++)
    {
        if(rd_destinations[i] == rd_dest)
        {
            Com_Error(ERR_FATAL, "G_PrintAddRedirect: Attempt to add an already defined redirect function twice.");
            return;
        }

        if(rd_destinations[i] == NULL)
        {
            rd_destinations[i] = rd_dest;
            return;
        }
    }
    Com_Error(ERR_FATAL, "G_PrintAddRedirect: Out of redirect handles. Increase MAX_REDIRECTDESTINATIONS to add more redirect destinations");
}



/*
=================
G_LogPrintf

Print to the logfile with a time stamp if it is open
=================
*/
__cdecl void QDECL G_LogPrintf( const char *fmt, ... ) {

	va_list argptr;

	char string[1024];
	int stringlen;
	int min, tens, sec;
	int timelen;


        if ( g_logTimeStampInSeconds->boolean )
        {
          time_t time1970 = Com_GetRealtime( );
          Com_sprintf(string, sizeof( string ), "%d ", time1970);
        }
        else
	{
	  sec = level.time / 1000;
	  min = sec / 60;
	  sec -= min * 60;
	  tens = sec / 10;
	  sec -= tens * 10;
	  Com_sprintf( string, sizeof( string ), "%3i:%i%i ", min, tens, sec );
	}

	timelen = strlen(string);

	va_start( argptr, fmt );
	Q_vsnprintf( string + timelen, sizeof( string ) - timelen, fmt, argptr );
	va_end( argptr );

	stringlen = strlen( string );

	G_PrintRedirect(string, stringlen);

	if ( !level.logFile ) {
		return;
	}

#ifdef _WIN32
	char outstring[2048];
	stringlen = Q_strLF2CRLF(string, outstring, sizeof(outstring) );
	Com_WriteGameLogfile( outstring, stringlen );
#else
	Com_WriteGameLogfile( string, stringlen );
#endif
}


void G_OpenLogFile()
{
  char serverinfo[1024];
  if ( *g_log->string )
  {
    level.logFile = Com_OpenGameLogfile(g_log->string, 'a', (qboolean)g_logSync->boolean);

    if ( level.logFile )
    {
      SV_GetServerinfo(serverinfo, 1024);
      G_LogPrintf("------------------------------------------------------------\n");
      G_LogPrintf("InitGame: %s\\g_logTimeStampInSeconds\\%d\n", serverinfo, g_logTimeStampInSeconds->boolean != 0);
    }
    else
    {
      Com_PrintWarning(CON_CHANNEL_SERVER, "WARNING: Couldn't open logfile: %s\n", g_log->string);
    }
  }
  else
  {
    Com_Printf(CON_CHANNEL_SERVER, "Not logging to disk.\n");
  }

}

void G_CloseLogFile()
{
	Com_CloseGameLogfile();
	level.logFile = 0;
}


float missile_frametime;
extern int helicopter_thinktime;
extern float vehicle_frametime;

void G_InitSomeVariables(int framerate)
{
    level.framerate = framerate;
    missile_frametime = 1.0f / (float)level.framerate;
    helicopter_thinktime = 1000 / level.framerate;
    vehicle_frametime = 1.0f / (float)level.framerate;
}


void __cdecl G_RunThink(struct gentity_s *ent)
{
  void (__cdecl *think)(struct gentity_s *);
  int thinktime;

  thinktime = ent->nextthink;
  if ( thinktime > 0 && thinktime <= level.time )
  {
    ent->nextthink = 0;
    think = entityHandlers[(uint8_t)ent->handler].think;
    if ( !think )
    {
      Com_Error(ERR_DROP, "NULL ent->think");
    }
    think(ent);
  }
}

#ifdef OL_SERVER
/* Free-moving bodies that have left the world through its floor (off a map
 * edge with nothing beneath them). Nothing stops them otherwise: a falling
 * trajectory has no ground to hit, so the origin runs away until the entity
 * expires, and C1's position codec only reaches 32767 units from the map
 * centre. Below the world model's bounds there is nothing to see or touch.
 *
 * A projectile or dropped item there can never be picked up or land; it is
 * freed, as Q3/IW G_RunItem frees one in a no-drop volume (script waiters
 * get the usual death notify). A corpse, an actor corpse and a physics or
 * physics-launched body may be held by script (the body clone, a care
 * package) and their lifetime is the script's or their think's: they are
 * stopped at the floor instead, still running their normal frame so their
 * think and expiry continue. Returns 1 when the entity was freed. */
static int EntityBelowWorld(gentity_t *ent)
{
  static int reported;
  vec3_t mins, maxs;
  float floorZ;
  int freeIt;
  if ( ent->tagInfo )
    return 0;
  if ( ent->client )
  {
    /* A living player below the world has fallen off the map with nothing
     * to land on. Maps put a trigger_hurt under such edges; where one is
     * missing the player would fall until the round ends. Kill them as that
     * trigger would (retained MOD_TRIGGER_HURT, 13), once: health is then
     * spent. The origin stays as pmove left it. */
    if ( ent->health <= 0 || !ent->takedamage || ent->client->sess.sessionState != SESS_STATE_PLAYING )
      return 0;
    CM_ModelBounds(0, mins, maxs);
    if ( !(mins[2] < maxs[2]) || ent->r.currentOrigin[2] >= mins[2] - 256.0f )
      return 0;
    if ( reported < 32 )
    {
      ++reported;
      Com_Printf(CON_CHANNEL_SERVER, "CoDOL: client %d fell out of the world at z %g (world floor %g); killed\n",
                 ent->s.number, ent->r.currentOrigin[2], mins[2]);
    }
    {
      static const float down[3] = {0.0f, 0.0f, -1.0f};
      G_Damage(ent, &g_entities[ENTITYNUM_WORLD], &g_entities[ENTITYNUM_WORLD], down, ent->r.currentOrigin,
               ent->health + 100000, 0, 13 /* MOD_TRIGGER_HURT */, -1, 0, 0u, 0u, 0);
    }
    return 0;
  }
  switch ( ent->s.eType )
  {
    case ET_MISSILE:
    case ET_ITEM:
      freeIt = 1;
      break;
    case ET_PLAYER_CORPSE:
    case ET_ACTOR_CORPSE:
      freeIt = 0;
      break;
    default:
      if ( !ent->physicsObject && !PhysicsLaunchOwnsEntity(ent) )
        return 0;
      freeIt = 0;
      break;
  }
  CM_ModelBounds(0, mins, maxs);
  if ( !(mins[2] < maxs[2]) )
    return 0;
  floorZ = mins[2] - 256.0f;
  if ( ent->r.currentOrigin[2] >= floorZ && ent->s.lerp.pos.trBase[2] >= floorZ &&
       ent->r.currentOrigin[2] == ent->r.currentOrigin[2] )
    return 0;
  if ( reported < 32 )
  {
    ++reported;
    Com_Printf(CON_CHANNEL_SERVER, "CoDOL: entity %d (type %d, %s) left the world at z %g (world floor %g); %s\n",
               ent->s.number, ent->s.eType, ent->classname ? SL_ConvertToString(ent->classname) : "",
               ent->r.currentOrigin[2], mins[2], freeIt ? "freed" : "stopped at the floor");
  }
  if ( freeIt )
  {
    G_FreeEntity(ent);
    return 1;
  }
  if ( ent->r.currentOrigin[0] != ent->r.currentOrigin[0] || ent->r.currentOrigin[1] != ent->r.currentOrigin[1] )
  {
    VectorCopy(ent->s.lerp.pos.trBase, ent->r.currentOrigin);
  }
  ent->r.currentOrigin[2] = floorZ;
  /* A ragdoll stays one (GScr_IsRagdoll, C1's ragdoll dispatch 10..12):
   * ragdoll interpolate evaluates to its base, like stationary. */
  ent->s.lerp.pos.trType = ent->s.lerp.pos.trType >= TR_RAGDOLL && ent->s.lerp.pos.trType <= TR_RAGDOLL_INTERPOLATE
                           ? TR_RAGDOLL_INTERPOLATE : TR_STATIONARY;
  ent->s.lerp.pos.trTime = level.time;
  ent->s.lerp.pos.trDuration = 0;
  VectorCopy(ent->r.currentOrigin, ent->s.lerp.pos.trBase);
  VectorClear(ent->s.lerp.pos.trDelta);
  SV_LinkEntity(ent);
  return 0;
}
#endif

void G_RunFrameForEntity(struct gentity_s *ent)
{
  assert(ent->r.inuse);

  if ( ent->processedFrame == level.framenum )
  {
    return;
  }
  ent->processedFrame = level.framenum;
#ifdef OL_SERVER
  TraceFireEventsEntity(ent); /* developer: codol_traceFireEvents */
#endif
  if ( ent->tagInfo )
  {
    assert(ent->tagInfo->parent != NULL);
    assert(ent->tagInfo->parent != ent);
    G_RunFrameForEntity(ent->tagInfo->parent);
  }
  assert(ent->r.inuse);

  assertx(ent->r.maxs[0] >= ent->r.mins[0], "entnum: %d, origin: %g %g %g, classname: %s", ent->s.number, ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2], SL_ConvertToString(ent->classname));
  assertx(ent->r.maxs[1] >= ent->r.mins[1], "entnum: %d, origin: %g %g %g, classname: %s", ent->s.number, ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2], SL_ConvertToString(ent->classname));
  assertx(ent->r.maxs[2] >= ent->r.mins[2], "entnum: %d, origin: %g %g %g, classname: %s", ent->s.number, ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2], SL_ConvertToString(ent->classname));

  /* C1 player eFlags 0x10000 means mantle (PM_CheckMantle 0x40a102).
   * It is not the retained non-player temporary-entity lifetime marker.
   * Applying that lifetime rule to a player frees its gentity mid-mantle. */
  if ( (ent->s.lerp.eFlags & 0x10000)
#ifdef OL_SERVER
       && !ent->client
       /* PlayerCmd_ClonePlayer copies the player's eFlags into the corpse, as
        * C1's clone 51c889 does, so a player killed mid-mantle leaves a corpse
        * with 0x10000 and time2 0. This rule freed that body the next frame:
        * the corpse vanished, and the out-of-turn free desynchronised the
        * corpse-info slots from the corpse entity ring. */
       && ent->s.eType != ET_PLAYER_CORPSE
#endif
     )
  {
    if ( level.time > ent->s.time2 )
    {
      G_FreeEntity(ent);
      return;
    }
  }
  if ( level.time - ent->eventTime > 300 )
  {
    if ( ent->freeAfterEvent )
    {
      G_FreeEntity(ent);
      return;
    }
    if ( ent->unlinkAfterEvent )
    {
      ent->unlinkAfterEvent = 0;
      SV_UnlinkEntity(ent);
    }
  }
  if ( ent->freeAfterEvent )
  {
    return;
  }
#ifdef OL_SERVER
  if ( EntityBelowWorld(ent) )
    return;
  /* CoDOL actors (attack dogs) own their whole frame: goal/route selection,
   * the authored animscript transition and the root-motion physics consumer.
   * A generic or uninitialised actor falls through to the retained paths. */
  if ( ent->s.eType == ET_ACTOR_CORPSE || ent->s.eType == ET_ACTOR )
  {
    /* A started ragdoll survives the frame's G_SetOrigin calls, as T5's MP
     * actor re-applies it after think (actor_ragdoll.c). */
    ragdoll_hold_t ragdoll;
    int handled;
    ActorTraceEvents(ent);
    ActorRagdollHold(ent, &ragdoll);
    handled = ent->s.eType == ET_ACTOR_CORPSE ? ActorRunCorpseFrame(ent) : ActorRunFrame(ent);
    ActorRagdollRestore(ent, &ragdoll);
    if ( handled )
      return;
  }
#endif
  switch ( ent->s.eType )
  {
    case ET_MISSILE:
      G_RunMissile(ent);
      return;
    case ET_ITEM:
      if ( ent->tagInfo )
      {
        G_GeneralLink(ent);
        G_RunThink(ent);
        return;
      }
      G_RunItem(ent);
      return;
    case ET_PLAYER_CORPSE:
      G_RunCorpse(ent);
      return;
    default:
      break;
  }
#ifdef OL_SERVER
  /* A PhysicsLaunchServer body is moved by its own simulation
   * (physics_launch.c), as C1's server moves its physics objects. The
   * retained mover must not also run it: MEASURED, G_RunMover left a
   * released crate stationary at the world origin one frame after launch. */
  if ( PhysicsLaunchOwnsEntity(ent) )
  {
    G_RunThink(ent);
    return;
  }
#endif
  if ( ent->physicsObject )
  {
    G_RunItem(ent);
    return;
  }
  if ( ent->s.eType == ET_SCRIPTMOVER || ent->s.eType == ET_PLANE || ent->s.eType == ET_PRIMARY_LIGHT )
  {
    G_RunMover(ent);
    return;
  }
  if ( ent->client )
  {
    G_RunClient(ent);
    return;
  }
  if ( ent->s.eType == ET_GENERAL && ent->tagInfo )
  {
    G_GeneralLink(ent);
  }
  G_RunThink(ent);
}


void __cdecl G_FreeEntity(gentity_s *gEnt)
{
  struct XAnimTree_s *tree;

#ifdef OL_SERVER
  PlayerLinkEntityFreed(gEnt);
  VehiclePath_EntityFreed(gEnt - g_entities);
  TurretMinimap_EntityFreed(gEnt - g_entities);
  EntityModelScaleFreed(gEnt - g_entities);
  ClipBoxFreed(gEnt - g_entities);
  /* T5 G_FreeEntity (OpenBLOPS g_utils_mp.cpp:1805): a freed path blocker
   * gives back every link it cut (Path_ConnectPathsForEntity).  The links are
   * tagged by entity number, so one left cut would also be handed to the
   * next entity spawned in this slot.  MEASURED 2026-10-21, zm_sumpf_sh: the
   * stairs debris's two "jiggle" brushes were deleted on purchase still
   * owning 76 stair links, and upstairs became a 65-node island. */
  BotNavConnectPaths((int)(gEnt - g_entities));
#endif
  G_EntUnlink(gEnt);
  while ( gEnt->tagChildren )
  {
    G_EntUnlink(gEnt->tagChildren);
  }

  SV_UnlinkEntity(gEnt);
  tree = SV_DObjGetTree(gEnt);
  if ( tree )
  {
    XAnimClearTree(tree);
  }
  Com_SafeServerDObjFree(gEnt->s.number);
  G_FreeEntityRefs(gEnt);
  if ( gEnt->pTurretInfo )
  {
    assert(gEnt->pTurretInfo->inuse);
    G_FreeTurret(gEnt);
    assert(gEnt->pTurretInfo == NULL);
  }
  if ( gEnt->scr_vehicle )
  {
    G_VehFreeEntity(gEnt);
    assert(gEnt->scr_vehicle == NULL);
  }
  if ( gEnt->s.eType == 2 )
  {
    PlayerCorpse_Free(gEnt);
  }

  EntHandleDissociate(gEnt);
  gEnt->r.ownerNum.setEnt( NULL);
  gEnt->parent.setEnt(NULL);
  gEnt->missileTargetEnt.setEnt(NULL);

  #ifdef OL_SERVER
  ResetEntitySoundState((unsigned)(gEnt - g_entities));
  ResetVehicleSpawner((unsigned)(gEnt - g_entities));
  /* C1 releases the entity's sentient from its own free path (0x572908 ->
   * 0x5786c0). EntHandleDissociate above has already cleared the record's
   * handle, so this is what keeps the pool from carrying a dead entity. */
  Sentient_EntityFreed(gEnt);
  #endif
  Scr_FreeEntity(gEnt);

  int useCount = gEnt->useCount;
#ifdef OL_SERVER
  /* G_Spawn inlines its initialization rather than calling G_InitGentity.
   * Clear this slot's native extensions at the same lifetime boundary as
   * the entityState counter below, after the entity's teardown callbacks.
   * Captured snapshot/archive extensions retain their original generation. */
  ResetEntityEventGeneration((unsigned)(gEnt - g_entities));
#endif
  memset(gEnt, 0, sizeof(*gEnt));
  gEnt->eventTime = level.time;

  if ( gEnt - level.gentities >= 72 )
  {
    if ( level.lastFreeEnt )
    {
      level.lastFreeEnt->nextFree = gEnt;
    }
    else
    {
      level.firstFreeEnt = gEnt;
    }
    level.lastFreeEnt = gEnt;
    gEnt->nextFree = 0;
  }
  gEnt->useCount = useCount + 1;
}


};


const entityHandler_t entityHandlers[] =
{
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0 },
  { NULL, NULL, NULL, &Touch_Multi, NULL, NULL, NULL, NULL, 0, 0 },
  { NULL, NULL, NULL, NULL, &hurt_use, NULL, NULL, NULL, 0, 0 },
  { NULL, NULL, NULL, &hurt_touch, &hurt_use, NULL, NULL, NULL, 0, 0 },
  { NULL, NULL, NULL, NULL, &Use_trigger_damage, &Pain_trigger_damage, &Die_trigger_damage, NULL, 0, 0},
  { NULL, &Reached_ScriptMover, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0 },
  { NULL, &Reached_ScriptMover, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0 },
  { &G_ExplodeMissile, NULL, NULL, &Touch_Item_Auto, NULL, NULL, NULL, NULL, 3, 4},
  { &G_TimedObjectThink, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0 },
  { &G_ExplodeMissile, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 5, 6 },
  { NULL, NULL, NULL, NULL, NULL, NULL, &player_die, &G_PlayerController, 0, 0 },
  { NULL, NULL, NULL, NULL, NULL, NULL, &player_die, NULL, 0, 0 },
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, &G_PlayerController, 0, 0 },
  { &BodyEnd, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0 },
  { &turret_think_init, NULL, NULL, NULL, &turret_use, NULL, NULL, &turret_controller, 0, 0},
  { &turret_think, NULL, NULL, NULL, &turret_use, NULL, NULL, &turret_controller, 0, 0},
  { &DroppedItemClearOwner, NULL, NULL, &Touch_Item_Auto, NULL, NULL, NULL, NULL, 0, 0},
  { &FinishSpawningItem, NULL, NULL, &Touch_Item_Auto, NULL, NULL, NULL, NULL, 0, 0},
  { NULL, NULL, NULL, &Touch_Item_Auto, NULL, NULL, NULL, NULL, 0, 0 },
  { NULL, NULL, NULL, NULL, &use_trigger_use, NULL, NULL, NULL, 0, 0 },
  { NULL, &Reached_ScriptMover, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0 },
  { &G_FreeEntity, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0 },
  { &G_VehEntHandler_Think, NULL, NULL, &G_VehEntHandler_Touch, &G_VehEntHandler_Use, &G_VehEntHandler_Pain, &G_VehEntHandler_Die, &G_VehEntHandler_Controller, 0, 0},
  { &Helicopter_Think, NULL, NULL, NULL, NULL, &Helicopter_Pain, &Helicopter_Die, &Helicopter_Controller, 0, 0},
  /* ENT_HANDLER_SCAVENGER_BAG: dropped scavenger bag (dropscavengerbag). */
  { &ScavengerBagThink, NULL, NULL, &ScavengerBagTouch, NULL, NULL, NULL, NULL, 0, 0 },
#ifdef OL_SERVER
  /* ENT_HANDLER_ACTOR: the native actor's real G_Damage pain/death
   * callbacks. Everything else about an actor runs in its own frame path. */
  { NULL, NULL, NULL, NULL, NULL, &ActorPain, &ActorDie, NULL, 0, 0 }
#else
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0 }
#endif
};
