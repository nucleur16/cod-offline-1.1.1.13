#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#ifdef OL_SERVER
/* CoDOL sentient registry - see sentient.h for the C1 evidence that
 * fixes the pool size, the record fields and the iterator semantics.
 *
 * Producers implemented here:
 *   - connected clients, through the shared client spawn / session-team /
 *     disconnect paths;
 *   - non-client actor entities, through the "actor_" classname C1 routes
 *     into its actor spawner (0x538da7, 0x56b1ce, 0x6f0d34) and through the
 *     script spawner;
 *   - script sentients, through C1's makeEntitySentient (0x57a3d0) and
 *     FreeEntitySentient (0x57a6a0) semantics.
 *
 * What this module deliberately is not: C1's AI. The record's actor slot
 * (C1 +0x04), the perception pair records the sentry vision code walks and
 * every behaviour field of the 0x1b10-byte actor struct are not reproduced.
 * An "actor" here is a real, linked, damageable non-client entity that is a
 * valid target and a valid friendly - nothing more. */

#include "sentient.h"
#include "actor_lifecycle.h"
#include "actor_scripts.h"
#include "threat_bias.h"
#include "g_shared.h"
#include "g_public.h"
#include "entity.h"
#include "qcommon_io.h"
#include "game/surfaceflags.h"

#include <string.h>

extern "C"
{
/* src/g_entity.c; no header exposes it. */
qboolean __cdecl G_LevelSpawnString(const char *key, const char *defaultString, const char **out);
/* src/asmsource/_g_spawn_mp.asm. */
int G_SpawnFloat(const char *key, const char *defaultValue, float *out);
/* src/server.h is a server-side header the game files do not pull in; the
 * other CoDOL game modules declare it locally the same way. */
void __cdecl SV_LinkEntity(gentity_s *ent);
}

static sentient_t s_sentients[MAX_SENTIENTS];

/* C1 finds an entity's record through gentity+0x184. This fork cannot grow
 * gentity_s: retained assembly indexes g_entities with a compiled stride and
 * reads fixed offsets, so the back-pointer lives here instead. -1 is "none". */
static short s_sentientForEntity[MAX_GENTITIES];

static void Sentient_ClearRecord(sentient_t *sentient)
{
    sentient->entity = NULL;
    sentient->team = SENTIENT_TEAM_NONE;
    sentient->ownerNum = -1;
    sentient->dontTarget = 0;
    sentient->inuse = 0;
    sentient->expendable = 0;
    sentient->isActor = 0;
    sentient->actorSlot = -1;
    VectorClear(sentient->origin);
    sentient->threatBias = 0;
    sentient->threatBiasGroup = 0;
}

/* Actor slots are handed out lowest-first so the published records stay in
 * ascending order, which is what the writer's own validation requires. A slot
 * is released with the record, so a freed actor's slot is reusable by the next
 * one without the client seeing two actors on the same slot in one frame. */
static int Sentient_AllocActorSlot(void)
{
    int slot;
    int i;

    for (slot = 0; slot < MAX_ACTOR_SLOTS; ++slot)
    {
        for (i = 0; i < MAX_SENTIENTS; ++i)
        {
            if (s_sentients[i].inuse && s_sentients[i].isActor &&
                s_sentients[i].actorSlot == slot)
            {
                break;
            }
        }
        if (i == MAX_SENTIENTS)
        {
            return slot;
        }
    }
    return -1;
}

/* C1's client reads at most 32 actor records (the count is clamped at
 * 0x4871b3) and this fork's writer refuses a number >= 32, so a 33rd actor
 * cannot be published at all. Releasing a slot therefore hands it straight to
 * the waiting actor with the lowest entity number: the published set stays
 * deterministic and a freed slot is reused without waiting for the next
 * registration. */
static void Sentient_ReleaseActorSlot(sentient_t *sentient)
{
    int slot = sentient->actorSlot;
    sentient_t *waiting = NULL;
    int i;

    sentient->actorSlot = -1;
    if (slot < 0)
    {
        return;
    }
    for (i = 0; i < MAX_SENTIENTS; ++i)
    {
        sentient_t *candidate = &s_sentients[i];

        if (candidate == sentient || !candidate->inuse || !candidate->isActor ||
            candidate->actorSlot >= 0 || !candidate->entity)
        {
            continue;
        }
        if (!waiting || candidate->entity->s.number < waiting->entity->s.number)
        {
            waiting = candidate;
        }
    }
    if (waiting)
    {
        waiting->actorSlot = slot;
    }
}

int Sentient_PublishedActorCount(void)
{
    int count = 0;
    int i;

    for (i = 0; i < MAX_SENTIENTS; ++i)
    {
        if (s_sentients[i].inuse && s_sentients[i].isActor && s_sentients[i].actorSlot >= 0)
        {
            ++count;
        }
    }
    return count;
}

int Sentient_UnpublishedActorCount(void)
{
    int count = 0;
    int i;

    for (i = 0; i < MAX_SENTIENTS; ++i)
    {
        if (s_sentients[i].inuse && s_sentients[i].isActor && s_sentients[i].actorSlot < 0)
        {
            ++count;
        }
    }
    return count;
}

sentient_t *Sentient_ActorBySlot(int slot)
{
    int i;

    if (slot < 0 || slot >= MAX_ACTOR_SLOTS)
    {
        return NULL;
    }
    for (i = 0; i < MAX_SENTIENTS; ++i)
    {
        if (s_sentients[i].inuse && s_sentients[i].isActor &&
            s_sentients[i].actorSlot == slot)
        {
            return &s_sentients[i];
        }
    }
    return NULL;
}

int Sentient_Index(const sentient_t *sentient)
{
    if (!sentient)
    {
        return -1;
    }
    return (int)(sentient - s_sentients);
}

sentient_t *Sentient_ByIndex(int index)
{
    if (index < 0 || index >= MAX_SENTIENTS)
    {
        return NULL;
    }
    return &s_sentients[index];
}

/* Level init. The EntHandle registry is reset by EntHandle::Init around this
 * point, so the handles are cleared raw rather than released one by one -
 * the same rule TurretTarget_ResetAll follows for the turret pool. */
void Sentient_ResetAll(void)
{
    int i;

    /* Same reset epoch as the sentient pool: actor identities pair 1:1 with
     * sentients, so releasing them after the handle registry has been reset
     * would release EntHandles that no longer describe this map. */
    ActorLifetimeReset();
    /* T5 Actor_InitThreatBiasGroups runs from the same level-init edge
     * (sentient.cpp:691): every group but the unnamed one is gone. */
    ThreatBias_Reset();

    for (i = 0; i < MAX_SENTIENTS; ++i)
    {
        Sentient_ClearRecord(&s_sentients[i]);
        s_sentients[i].handle.number = 0;
        s_sentients[i].handle.infoIndex = 0;
    }
    for (i = 0; i < MAX_GENTITIES; ++i)
    {
        s_sentientForEntity[i] = -1;
    }
}

sentient_t *Sentient_ForEntity(gentity_s *entity)
{
    int index;
    sentient_t *sentient;

    if (!entity)
    {
        return NULL;
    }
    index = entity->s.number;
    if (index < 0 || index >= MAX_GENTITIES)
    {
        return NULL;
    }
    if (s_sentientForEntity[index] < 0)
    {
        return NULL;
    }
    sentient = &s_sentients[s_sentientForEntity[index]];
    if (!sentient->inuse || sentient->entity != entity)
    {
        s_sentientForEntity[index] = -1;
        return NULL;
    }
    return sentient;
}

int Sentient_TeamForEntity(gentity_s *entity, int *team)
{
    sentient_t *sentient = Sentient_ForEntity(entity);
    if (!sentient || !team)
    {
        return 0;
    }
    *team = sentient->team;
    return 1;
}

/* C1 0x5786c0 with its tail at 0x578733: drops the entity's back-pointer and
 * returns the record to the pool. The AI teardown C1 does either side of that
 * has no counterpart here. */
void Sentient_Free(sentient_t *sentient)
{
    int index;

    if (!sentient || !sentient->inuse)
    {
        return;
    }
    if (sentient->entity)
    {
        index = sentient->entity->s.number;
        if (index >= 0 && index < MAX_GENTITIES &&
            s_sentientForEntity[index] == Sentient_Index(sentient))
        {
            s_sentientForEntity[index] = -1;
        }
    }
    if (sentient->isActor)
    {
        /* The native actor identity is paired with this record, so it has to
         * be dropped before the slot can be handed to a waiting actor. */
        ActorForget(sentient->entity);
        /* The wire slot goes back to the pool - to a waiting actor when there
         * is one. The composition itself is the entity's own native
         * attachment state, which G_FreeEntity clears with the entity. */
        Sentient_ReleaseActorSlot(sentient);
    }
    sentient->handle.setEnt(NULL);
    Sentient_ClearRecord(sentient);
}

/* C1 record +0x0c is the owning entity number plus one, which turret
 * acquisition compares against the sentry owner (0x57ce17). This fork already
 * carries that relationship in the entity's own owner handle, so the record
 * mirrors it rather than inventing a second owner. */
static void Sentient_RefreshOwner(sentient_t *sentient)
{
    sentient->ownerNum = sentient->entity && sentient->entity->r.ownerNum.isDefined()
                             ? sentient->entity->r.ownerNum.entnum()
                             : -1;
}

/* C1 0x578960. The origin snapshot and the team are the two fields the turret
 * family reads; the AI constants C1 also seeds (+0x28, +0x60) belong to the
 * behaviour it runs on the record. */
void Sentient_Setup(sentient_t *sentient, gentity_s *entity, int team)
{
    int index;

    if (!sentient || !entity)
    {
        return;
    }
    sentient->entity = entity;
    sentient->handle.setEnt(entity);
    sentient->team = team;
    sentient->dontTarget = 0;
    Sentient_RefreshOwner(sentient);
    VectorCopy(entity->r.currentOrigin, sentient->origin);

    index = entity->s.number;
    if (index >= 0 && index < MAX_GENTITIES)
    {
        s_sentientForEntity[index] = (short)Sentient_Index(sentient);
    }
}

/* C1 0x5785b0. Both allocation classes first make room: once 24 records are
 * live that carry no actor (0x5785d8..0x57862f counts exactly that), the first
 * expendable record is recycled (0x578640), and when there is none the
 * allocation fails without touching the pool (0x578657). */
sentient_t *Sentient_Alloc(int allocClass)
{
    int i;
    int nonActor = 0;
    sentient_t *sentient;

    if (allocClass == SENTIENT_PERSISTENT || allocClass == SENTIENT_EXPENDABLE)
    {
        for (i = 0; i < MAX_SENTIENTS; ++i)
        {
            if (s_sentients[i].inuse && !s_sentients[i].isActor)
            {
                ++nonActor;
            }
        }
        if (nonActor >= SENTIENT_MAX_PERSISTENT)
        {
            for (i = 0; i < MAX_SENTIENTS; ++i)
            {
                if (s_sentients[i].inuse && s_sentients[i].expendable)
                {
                    break;
                }
            }
            if (i == MAX_SENTIENTS)
            {
                return NULL;
            }
            Sentient_Free(&s_sentients[i]);
        }
    }

    for (i = 0; i < MAX_SENTIENTS; ++i)
    {
        if (!s_sentients[i].inuse)
        {
            break;
        }
    }
    if (i == MAX_SENTIENTS)
    {
        /* C1 0x57867f prints exactly this before returning NULL. */
        Com_PrintWarning(CON_CHANNEL_SERVER, "Sentient allocation failed\n");
        return NULL;
    }

    sentient = &s_sentients[i];
    Sentient_ClearRecord(sentient);
    sentient->expendable = (allocClass == SENTIENT_EXPENDABLE) ? 1 : 0;
    sentient->inuse = 1;
    return sentient;
}

/* A record whose entity is gone is reclaimed on the spot. EntHandleDissociate
 * clears every handle that pointed at a freed entity, so an entity slot that
 * has been recycled cannot be mistaken for the one that was registered. */
static qboolean Sentient_Live(sentient_t *sentient)
{
    if (!sentient->inuse)
    {
        return qfalse;
    }
    /* G_FreeEntity runs EntHandleDissociate, which zeroes the number of every
     * handle that pointed at the freed slot, so a handle that no longer names
     * the entity it was registered with is a slot that has been recycled. */
    if (!sentient->entity || !sentient->entity->r.inuse ||
        sentient->handle.number != (uint16_t)(sentient->entity->s.number + 1))
    {
        Sentient_Free(sentient);
        return qfalse;
    }
    Sentient_RefreshOwner(sentient);
    VectorCopy(sentient->entity->r.currentOrigin, sentient->origin);
    return qtrue;
}

/* C1 0x578e70: the first record whose class bit is in the mask. */
sentient_t *Sentient_First(unsigned int classMask)
{
    int i;

    for (i = 0; i < MAX_SENTIENTS; ++i)
    {
        if (!Sentient_Live(&s_sentients[i]))
        {
            continue;
        }
        if (classMask & (1u << s_sentients[i].team))
        {
            return &s_sentients[i];
        }
    }
    return NULL;
}

/* C1 0x578eb0: the next record after this one, same test. C1 derives the
 * index by dividing the record offset by 0x7c; the index is explicit here. */
sentient_t *Sentient_Next(sentient_t *sentient, unsigned int classMask)
{
    int i;

    i = Sentient_Index(sentient);
    if (i < 0)
    {
        return NULL;
    }
    for (++i; i < MAX_SENTIENTS; ++i)
    {
        if (!Sentient_Live(&s_sentients[i]))
        {
            continue;
        }
        if (classMask & (1u << s_sentients[i].team))
        {
            return &s_sentients[i];
        }
    }
    return NULL;
}

/* C1 builds this table on the stack at 0x57cc51..0x57cc73 and indexes it with
 * the turret's own team: a turret takes every class except its own, and the
 * team3 turret additionally takes class 4. Teams 4..6 select nothing, which is
 * why an unconfigured turret acquires no one. */
unsigned int Sentient_ClassMask(int turretTeam)
{
    static const unsigned int table[SENTIENT_TEAM_COUNT] =
    {
        0xffffff9fu, 0xcu, 0xau, 0x16u, 0u, 0u, 0u
    };

    if (turretTeam < 0 || turretTeam >= SENTIENT_TEAM_COUNT)
    {
        return 0;
    }
    return table[turretTeam];
}

/* This fork's session teams onto C1's sentient classes. TEAM_RED and
 * TEAM_BLUE are C1's axis (1) and allies (2).
 *
 * A free-for-all player (TEAM_FREE) is class 4. Class 0 is in no turret
 * team's class mask (0x57cc51: axis 0xc, allies 0xa, team3 0x16), so an FFA
 * player filed there could never be acquired by any sentry. C1's FFA turret is
 * the team3 one: its mask is the only one that takes class 4, and it is the
 * only team for which acquisition skips the sentry owner himself (0x57cde0..
 * 0x57cdf1), which only matters when the owner is in the turret's own target
 * set, i.e. in free-for-all. Class 4 is also C1's unaligned default: the actor
 * spawner seeds its sentient with it (0x6c4efe). Axis and allies turrets do
 * not take class 4, and no turret team counts it as friendly (the blocker
 * 0x57cb80 only looks at the turret's own class), so team games are
 * unaffected. */
static int Sentient_TeamForClient(const gclient_s *client)
{
    if (!client)
    {
        return SENTIENT_TEAM_NONE;
    }
    switch (client->sess.cs.team)
    {
    case TEAM_RED:
        return SENTIENT_TEAM_AXIS;
    case TEAM_BLUE:
        return SENTIENT_TEAM_ALLIES;
    case TEAM_FREE:
        return SENTIENT_TEAM_FOUR;
    default:
        return SENTIENT_TEAM_NONE;
    }
}

static sentient_t *Sentient_RegisterEntity(gentity_s *entity, int team, int allocClass)
{
    sentient_t *sentient;

    sentient = Sentient_ForEntity(entity);
    if (sentient)
    {
        sentient->team = team;
        VectorCopy(entity->r.currentOrigin, sentient->origin);
        return sentient;
    }
    sentient = Sentient_Alloc(allocClass);
    if (!sentient)
    {
        return NULL;
    }
    Sentient_Setup(sentient, entity, team);
    return sentient;
}

void Sentient_ClientSpawned(gentity_s *entity)
{
    if (!entity || !entity->client)
    {
        return;
    }
    /* C1 keeps a sentient across a death: acquisition filters on the
     * candidate's health (0x57ce30) instead of deregistering the record, and
     * the friendly blocker does not look at health at all. Re-spawning only
     * refreshes the team and the cached origin. */
    Sentient_RegisterEntity(entity, Sentient_TeamForClient(entity->client),
                            SENTIENT_PERSISTENT);
}

void Sentient_ClientTeamChanged(gclient_s *client)
{
    int index;
    gentity_s *entity;

    if (!client)
    {
        return;
    }
    index = (int)(client - level.clients);
    if (index < 0 || index >= level.maxclients)
    {
        return;
    }
    entity = &g_entities[index];
    if (!entity->r.inuse || entity->client != client)
    {
        return;
    }
    Sentient_RegisterEntity(entity, Sentient_TeamForClient(client),
                            SENTIENT_PERSISTENT);
}

void Sentient_ClientDisconnected(int clientNum)
{
    if (clientNum < 0 || clientNum >= level.maxclients)
    {
        return;
    }
    Sentient_Free(Sentient_ForEntity(&g_entities[clientNum]));
}

void Sentient_EntityFreed(gentity_s *entity)
{
    /* Forget owns neither the entity nor its DObj - G_FreeEntity releases
     * those - but it must run before the sentient slot can be recycled. An
     * actor entity freed without a sentient (a failed spawn) reaches here too. */
    ActorForget(entity);
    Sentient_Free(Sentient_ForEntity(entity));
}

static int Sentient_TeamFromName(const char *name)
{
    if (!name || !*name)
    {
        return SENTIENT_TEAM_NONE;
    }
    if (!Q_stricmp(name, "axis"))
    {
        return SENTIENT_TEAM_AXIS;
    }
    if (!Q_stricmp(name, "allies"))
    {
        return SENTIENT_TEAM_ALLIES;
    }
    if (!Q_stricmp(name, "team3"))
    {
        return SENTIENT_TEAM_THREE;
    }
    if (!Q_stricmp(name, "team4"))
    {
        return SENTIENT_TEAM_FOUR;
    }
    if (!Q_stricmp(name, "neutral") || !Q_stricmp(name, "none"))
    {
        return SENTIENT_TEAM_NONE;
    }
    return -1;
}

static const char *Sentient_TeamName(int team)
{
    switch (team)
    {
    case SENTIENT_TEAM_AXIS:
        return "axis";
    case SENTIENT_TEAM_ALLIES:
        return "allies";
    case SENTIENT_TEAM_THREE:
        return "team3";
    case SENTIENT_TEAM_FOUR:
        return "team4";
    default:
        return "neutral";
    }
}

/* The entity half of a non-client actor: a linked, damageable body with a
 * team. This is the representation the turret family needs; it is not AI and
 * runs no behaviour of its own. */
static qboolean Sentient_ConfigureActor(gentity_s *entity, const char *model, int team, int health)
{
    sentient_t *sentient;

    VectorCopy(playerMins, entity->r.mins);
    VectorCopy(playerMaxs, entity->r.maxs);
    entity->r.contents = CONTENTS_BODY;
    entity->health = health;
    entity->takedamage = 1;
    if (model && *model)
    {
        G_SetModel(entity, model);
    }
    G_SetOrigin(entity, entity->r.currentOrigin);
    SV_LinkEntity(entity);

    sentient = Sentient_RegisterEntity(entity, team, SENTIENT_PERSISTENT);
    if (!sentient)
    {
        return qfalse;
    }
    if (!sentient->isActor)
    {
        int slot = Sentient_AllocActorSlot();

        if (slot < 0)
        {
            /* Every wire slot is taken. The entity stays a sentient - it is a
             * valid target and a valid friendly - but it carries no actor
             * record, so nothing publishes a composition for it. */
            Com_PrintWarning(CON_CHANNEL_SERVER,
                             "actor entity %i has no free actor slot; it stays a sentient and "
                             "publishes no composition until a slot is released\n",
                             entity->s.number);
        }
        sentient->actorSlot = slot;
        sentient->isActor = 1;
    }
    return qtrue;
}

/* C1 dispatches a map entity whose classname starts with "actor_" into its
 * actor spawner instead of the classname table (0x538d94, 0x56b1ab and
 * 0x6f0d34 all compare the same six bytes). The keys read here are the ones
 * this fork can honour without an AI: team, model and health. */
qboolean Sentient_SpawnActorEntity(gentity_s *entity, const char *classname)
{
    const char *value;
    const char *model = NULL;
    int team = SENTIENT_TEAM_NONE;
    float health = 100.0f;
    int parsed;

    if (!entity || !classname || Q_stricmpn(classname, "actor_", 6))
    {
        return qfalse;
    }
    /* C1 SP_actor 6C4B74 routes spawnflag 1 before allocating either
     * actor or sentient. A spawner is not a damageable actor and must not
     * occupy an actor composition slot or become a turret target.
     * C1 SP_actor_spawner 6F1280 / T5 SP_actor_spawner agree on these
     * semantic fields; use native member names, never C1 byte offsets. */
    if (entity->spawnflags & 1)
    {
        entity->model = 0;
        entity->clipmask = 0;
        entity->r.contents = 0;
        entity->r.svFlags = SVF_NOCLIENT;
        entity->s.eType = ET_ACTOR_SPAWNER;
        entity->item[0].clipAmmoCount = -1;
        entity->item[0].ammoCount = 0;
        /* C1 defaults an omitted count to one, but preserves an explicitly
         * authored zero (6F12B3..6F1334 scans the spawn keys for "count"). */
        if (!entity->count &&
            (!level.spawnVars.spawnVarsValid ||
             !G_LevelSpawnString("count", "", &value)))
            entity->count = 1;
        /* T5 SP_actor_spawner runs the aitype's own spawner() on the template
         * so spawnactor has authored fields to duplicate. A classname whose
         * aitype scripts were not loaded leaves an inert template rather than
         * a spawner that would hand script a fake actor; the fatal gate for a
         * map that really authors dogs is OL_ActorScriptsLoadMapTypes. */
        if (!ActorScriptsSpawnerPending(entity) && !ActorScriptsSpawner(entity))
            Com_PrintWarning(CON_CHANNEL_SERVER,
                "CoDOL actor: spawner '%s' has no initialized aitype script; it will not spawn\n",
                classname);
        return qtrue;
    }
    if (level.spawnVars.spawnVarsValid)
    {
        if (G_LevelSpawnString("team", "", &value))
        {
            parsed = Sentient_TeamFromName(value);
            if (parsed >= 0)
            {
                team = parsed;
            }
        }
        G_LevelSpawnString("model", "", &model);
        G_SpawnFloat("health", "100", &health);
    }
    return Sentient_ConfigureActor(entity, model, team, (int)health);
}

/* ------------------------------------------------------------------ script */

static gentity_s *Sentient_Receiver(scr_entref_t ref)
{
    if (ref.classnum || ref.entnum >= MAX_GENTITIES)
    {
        Scr_ObjectError("Not an entity");
        return NULL;
    }
    return &g_entities[ref.entnum];
}

/* C1 0x57a3d0. Rejects an entity that is a player or is AI-driven with
 * "Do not use makeEntitySentient on AI or player" (0xc5e900), takes the
 * expendable flag from its optional argument (0x57a42b builds the allocation
 * class as 2 + flag) and warns when the pool is full (0xc5e930). An entity
 * that already has a record only has its expendable flag updated (0x57a4be). */
void Scr_MakeEntitySentient(scr_entref_t ref)
{
    gentity_s *entity = Sentient_Receiver(ref);
    sentient_t *sentient;
    int expendable = 0;
    int team = SENTIENT_TEAM_NONE;

    if (!entity)
    {
        return;
    }
    sentient = Sentient_ForEntity(entity);
    if (entity->client || (sentient && sentient->isActor))
    {
        Scr_Error("Do not use makeEntitySentient on AI or player");
        return;
    }
    if (Scr_GetNumParam() > 0)
    {
        expendable = Scr_GetInt(0) ? 1 : 0;
    }
    if (Scr_GetNumParam() > 1)
    {
        team = Sentient_TeamFromName(Scr_GetString(1));
        if (team < 0)
        {
            Scr_ParamError(1, "unknown team; must be axis, allies, team3, team4 or neutral");
            return;
        }
    }

    if (sentient)
    {
        /* C1 0x57a4be only refreshes the expendable flag on a record that
         * already exists. */
        sentient->expendable = (unsigned char)expendable;
        sentient->team = team;
        return;
    }
    sentient = Sentient_Alloc(expendable ? SENTIENT_EXPENDABLE
                                               : SENTIENT_PERSISTENT);
    if (!sentient)
    {
        Com_PrintWarning(CON_CHANNEL_SERVER,
                         "makeentitysentient: too many non-expendable sentients, "
                         "unable to make entity %i a sentient\n", entity->s.number);
        return;
    }
    Sentient_Setup(sentient, entity, team);
}

/* C1 0x57a6a0, including its "Do not use FreeEntitySentient on AI or player"
 * rejection (0xc5ea08). */
void Scr_FreeEntitySentient(scr_entref_t ref)
{
    gentity_s *entity = Sentient_Receiver(ref);

    if (!entity)
    {
        return;
    }
    if (entity->client)
    {
        Scr_Error("Do not use FreeEntitySentient on AI or player");
        return;
    }
    Sentient_Free(Sentient_ForEntity(entity));
}

void Scr_IsSentient(scr_entref_t ref)
{
    gentity_s *entity = Sentient_Receiver(ref);

    Scr_AddBool(entity && Sentient_ForEntity(entity) != NULL);
}

void Scr_SetSentientTeam(scr_entref_t ref)
{
    gentity_s *entity = Sentient_Receiver(ref);
    sentient_t *sentient;
    int team;

    if (!entity)
    {
        return;
    }
    sentient = Sentient_ForEntity(entity);
    if (!sentient)
    {
        Scr_Error("setsentientteam: entity is not a sentient");
        return;
    }
    team = Sentient_TeamFromName(Scr_GetString(0));
    if (team < 0)
    {
        Scr_ParamError(0, "unknown team; must be axis, allies, team3, team4 or neutral");
        return;
    }
    sentient->team = team;
}

void Scr_GetSentientTeam(scr_entref_t ref)
{
    gentity_s *entity = Sentient_Receiver(ref);
    sentient_t *sentient;

    if (!entity)
    {
        return;
    }
    sentient = Sentient_ForEntity(entity);
    if (!sentient)
    {
        Scr_AddUndefined();
        return;
    }
    Scr_AddString(Sentient_TeamName(sentient->team));
}

/* The record's "do not target" byte, C1 +0x18, which turret acquisition tests
 * both for the tracked target (0x57cd81) and for a fresh candidate
 * (0x57cdd6). */
void Scr_SetDontTarget(scr_entref_t ref)
{
    gentity_s *entity = Sentient_Receiver(ref);
    sentient_t *sentient;

    if (!entity)
    {
        return;
    }
    sentient = Sentient_ForEntity(entity);
    if (!sentient)
    {
        Scr_Error("setdonttarget: entity is not a sentient");
        return;
    }
    sentient->dontTarget = Scr_GetInt(0) ? 1 : 0;
}

void Scr_GetDontTarget(scr_entref_t ref)
{
    gentity_s *entity = Sentient_Receiver(ref);
    sentient_t *sentient;

    if (!entity)
    {
        return;
    }
    sentient = Sentient_ForEntity(entity);
    Scr_AddBool(sentient && sentient->dontTarget);
}

/* ------------------------------------------------------------ threat bias */

/* The receiver's record. A player is always a sentient in IW4/T5; here a
 * client that has not spawned yet has no record, so one is made on demand
 * with the session team (ClientSpawned refreshes it later). Anything else
 * that is not a sentient is T5 GScr_GetSentient_SP's "not a sentient". */
static sentient_t *Sentient_ThreatReceiver(scr_entref_t ref)
{
    gentity_s *entity = Sentient_Receiver(ref);
    sentient_t *sentient;

    if (!entity)
    {
        return NULL;
    }
    sentient = Sentient_ForEntity(entity);
    if (!sentient && entity->client && entity->r.inuse)
    {
        Sentient_ClientSpawned(entity);
        sentient = Sentient_ForEntity(entity);
    }
    if (!sentient)
    {
        Scr_ObjectError("not a sentient");
    }
    return sentient;
}

/* T5 GScr_RequireThreatBiasGroup_SP (g_scr_main_mp.cpp:18068-18080): an
 * unknown name is a script error, as in IW4. */
static int ThreatBias_RequireGroup(unsigned int parameter)
{
    const char *name = Scr_GetString(parameter);
    int group = ThreatBias_Find(name);

    if (group < 0)
    {
        Scr_Error(va("Invalid threat bias group '%s'.\n", name));
    }
    return group;
}

/* createthreatbiasgroup(<name>); T5 0x00819290 (g_scr_main_mp.cpp:18239). */
static void Scr_CreateThreatBiasGroup(void)
{
    const char *name;

    if (Scr_GetNumParam() != 1)
    {
        Scr_ParamError(0, "createthreatbiasgroup [name]");
        return;
    }
    name = Scr_GetString(0);
    if (ThreatBias_Find(name) >= 0)
    {
        return;
    }
    if (!*name)
    {
        Scr_ParamError(0, "threat bias group name must not be empty");
        return;
    }
    if (ThreatBias_Create(name) < 0)
    {
        Com_PrintWarning(CON_CHANNEL_SCRIPT, "Too many threat groups, can't create '%s'\n", name);
    }
}

/* setthreatbias(<group1>, <group2>, <threat>): group2 is <threat> more
 * likely to attack group1; T5 0x008193D0 (g_scr_main_mp.cpp:18211). */
static void Scr_SetThreatBias(void)
{
    int group1, group2;

    if (Scr_GetNumParam() != 3)
    {
        Scr_ParamError(0, "setthreatbias [threatener] [threatened] [threat]");
        return;
    }
    group1 = ThreatBias_RequireGroup(0);
    group2 = ThreatBias_RequireGroup(1);
    if (group1 < 0 || group2 < 0)
    {
        return;
    }
    ThreatBias_Set(group1, group2, Scr_GetInt(2));
}

/* setignoremegroup(<group1>, <group2>): group2 ignores group1. IW4 writes the
 * sentinel the T5 threat pass tests (Actor_CheckIgnore, actor_threat.cpp:42;
 * Actor_UpdateSingleThreat :423), which is also what getthreatbias then
 * reports -- IW4 animscripts/combat_utility.gsc tests it as < -10000. */
static void Scr_SetIgnoreMeGroup(void)
{
    int group1, group2;

    if (Scr_GetNumParam() != 2)
    {
        Scr_ParamError(0, "setignoremegroup [group to ignore] [group that ignores]");
        return;
    }
    group1 = ThreatBias_RequireGroup(0);
    group2 = ThreatBias_RequireGroup(1);
    if (group1 < 0 || group2 < 0)
    {
        return;
    }
    ThreatBias_Set(group1, group2, THREATBIAS_IGNORE);
}

/* threatbiasgroupexists(<name>); T5 0x008192D0 returns an int. */
static void Scr_ThreatBiasGroupExists(void)
{
    if (Scr_GetNumParam() != 1)
    {
        Scr_ParamError(0, "threatbiasgroupexists [name]");
        return;
    }
    Scr_AddInt(ThreatBias_Find(Scr_GetString(0)) >= 0);
}

/* getthreatbias(<group1>, <group2>); T5 0x00819320. */
static void Scr_GetThreatBias(void)
{
    int group1, group2;

    if (Scr_GetNumParam() != 2)
    {
        Scr_ParamError(0, "getthreatbias [group for] [group against]");
        return;
    }
    group1 = ThreatBias_RequireGroup(0);
    group2 = ThreatBias_RequireGroup(1);
    if (group1 < 0 || group2 < 0)
    {
        return;
    }
    Scr_AddInt(ThreatBias_Get(group1, group2));
}

/* <sentient> setthreatbiasgroup([name]); no argument returns the sentient to
 * the unnamed group 0 (T5 0x008195A0, g_scr_main_mp.cpp:21177-21190; IW4
 * maps/airport_code.gsc calls it bare). */
static void Scr_SetThreatBiasGroup(scr_entref_t ref)
{
    sentient_t *sentient = Sentient_ThreatReceiver(ref);
    int group;

    if (!sentient)
    {
        return;
    }
    if (Scr_GetNumParam() != 1)
    {
        sentient->threatBiasGroup = 0;
        return;
    }
    group = ThreatBias_RequireGroup(0);
    if (group >= 0)
    {
        sentient->threatBiasGroup = group;
    }
}

/* <sentient> getthreatbiasgroup(); "" for group 0 (T5 0x00819630). */
static void Scr_GetThreatBiasGroup(scr_entref_t ref)
{
    sentient_t *sentient = Sentient_ThreatReceiver(ref);

    if (!sentient)
    {
        return;
    }
    Scr_AddString(ThreatBias_Name(sentient->threatBiasGroup));
}

void ThreatBias_RegisterScript(void)
{
    Scr_AddFunction("createthreatbiasgroup", Scr_CreateThreatBiasGroup, qfalse);
    Scr_AddFunction("setthreatbias", Scr_SetThreatBias, qfalse);
    Scr_AddFunction("setignoremegroup", Scr_SetIgnoreMeGroup, qfalse);
    Scr_AddFunction("threatbiasgroupexists", Scr_ThreatBiasGroupExists, qfalse);
    Scr_AddFunction("getthreatbias", Scr_GetThreatBias, qfalse);
    Scr_AddMethod("setthreatbiasgroup", Scr_SetThreatBiasGroup, qfalse);
    Scr_AddMethod("getthreatbiasgroup", Scr_GetThreatBiasGroup, qfalse);
}

/* T5 sentient_fields.cpp:18 "threatbias", F_INT, on players and actors
 * alike (IW4 animscripts/pain.gsc writes it on the actor, IW4 levels on the
 * player). */
int Sentient_SetThreatBiasField(gentity_s *entity)
{
    sentient_t *sentient = Sentient_ForEntity(entity);

    if (!sentient && entity && entity->client && entity->r.inuse)
    {
        Sentient_ClientSpawned(entity);
        sentient = Sentient_ForEntity(entity);
    }
    if (!sentient)
    {
        return 0;
    }
    sentient->threatBias = Scr_GetInt(0);
    return 1;
}

int Sentient_GetThreatBiasField(gentity_s *entity)
{
    sentient_t *sentient = Sentient_ForEntity(entity);

    if (!sentient)
    {
        return 0;
    }
    Scr_AddInt(sentient->threatBias);
    return 1;
}
int Sentient_SetDontTargetForEntity(gentity_s *entity, int on)
{
    sentient_t *sentient = Sentient_ForEntity(entity);
    if (!sentient) return 0;
    sentient->dontTarget = on ? 1 : 0;
    return 1;
}
#endif

#ifdef OL_SERVER
#include "persistent_ai.h"

extern "C" int PersistentSentientsReleased(void) {
    for(unsigned i=0;i<MAX_SENTIENTS;++i) {
        const sentient_t *s=&s_sentients[i];
        if(s->entity||s->inuse||s->isActor||s->handle.number||s->handle.infoIndex)return 0;
    }
    for(unsigned i=0;i<MAX_GENTITIES;++i)if(s_sentientForEntity[i]!=-1)return 0;
    return 1;
}
extern "C" int PersistentSentientStaticsEmpty(void) {
    if(!PersistentSentientsReleased())return 0;
    for(unsigned i=0;i<MAX_SENTIENTS;++i) {
        const sentient_t *s=&s_sentients[i];
        if(s->team!=SENTIENT_TEAM_NONE||s->ownerNum!=-1||s->actorSlot!=-1||
           s->dontTarget||s->expendable||s->origin[0]||s->origin[1]||s->origin[2])return 0;
    }
    return 1;
}
extern "C" int PersistentSentientStaticsReset(void) {
    /* The ordinary helper also raw-resets the actor pool. Its resources must
     * already be released even when this per-owner API is called directly. */
    if(!PersistentSentientsReleased()||!PersistentActorsReleased())return 0;
    Sentient_ResetAll();
    return PersistentSentientStaticsEmpty();
}

#endif
