/* Regression for the 2026-10-05 production crash "SV_GetCachedSnapshot: end of
 * message" (build 5056f2fac037, zm_prototype_sh).
 *
 * d4712f16 let a script_model carry an entity-state animtree selector with no
 * entity-state play, (entry 0, tree N), for the snapshot-command-driven
 * mystery-box lid. The archive writer emitted that pair but the archive reader
 * still rejected any pair with exactly one half zero, flagged the message as
 * overflowed, and the first antilag/archive lookup after the lid was bound
 * Com_Error'd the server.
 *
 * This drives the production SV_GetCachedSnapshotInternal over archived frames
 * written with the production archived-entity and extension writers:
 *  1. a frame holding a bound lid decodes, and the lid keeps (0, tree);
 *  2. a malformed frame (a record the reader refuses) is rejected without Com_Error, remembered,
 *     and does not disturb the good frame. */
#include "server.h"
#include "entitystate_ext.h"
#include "playerstate_ext.h"
#include "qcommon_io.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

server_t sv;
serverStatic_t svs;
svsHeader_t svsHeader;
cvar_t *ol_debugTrace, *com_developer;
qboolean svsHeaderValid;
static cvar_t shownet; /* .integer set to -1 in main: no client is shown */
cvar_t *sv_shownet = &shownet;
extern cvar_t *msg_printEntityNums; /* msg.c; registered by MSG_RegisterCvars */
static cvar_t print_entity_nums;
static int warnings;

qboolean Assert_MyHandler(const char *exp, const char *file, int line,
                         const char *function, const char *fmt, ...)
{ (void)fmt; fprintf(stderr, "assert %s at %s:%d %s\n", exp, file, line, function); abort(); }
void Com_PrintWarning(conChannel_t channel, const char *fmt, ...)
{ (void)channel; (void)fmt; ++warnings; }
void Com_Printf(conChannel_t channel, const char *fmt, ...)
{ (void)channel; (void)fmt; }
void Com_PrintError(conChannel_t channel, const char *fmt, ...)
{ (void)channel; (void)fmt; }
void Com_DPrintf(conChannel_t channel, const char *fmt, ...)
{ (void)channel; (void)fmt; }
void Com_Error(int level, const char *fmt, ...)
{ (void)level; fprintf(stderr, "Com_Error: %s\n", fmt); abort(); }
void Com_Memset(void *data, byte value, int size) { memset(data, value, (size_t)size); }

/* Only the archive ring is tracked storage in this fixture. */
const void *EntityStateExtLiveBase(unsigned *count)
{ if (count) *count = 0; return NULL; }
const void *EntityStateExtSnapshotBase(unsigned *count)
{ if (count) *count = 0; return NULL; }
const void *EntityStateExtArchiveBase(unsigned *count)
{
    if (count) *count = sizeof(svs.cachedSnapshotEntities) / sizeof(svs.cachedSnapshotEntities[0]);
    return svs.cachedSnapshotEntities;
}
const void *PlayerStateExtLiveBase(unsigned *count)
{ *count = 0; return NULL; }
const void *PlayerStateExtSnapshotBase(unsigned *count)
{ *count = 0; return NULL; }
const void *PlayerStateExtArchiveBase(unsigned *count)
{ *count = 0; return NULL; }
/* No clients are archived by this fixture. */
void ReadArchivedClientAbilities(msg_t *msg, const clientState_t *cache)
{ (void)msg; (void)cache; abort(); }

enum { LID = 192, SOURCE = 4000, TIME = 81000 };

/* Writes one key frame holding the given archived entity, the way
 * SV_ArchiveSnapshot does, and registers it as archived frame `index`. */
static int write_frame(int index, archivedEntity_t *entity)
{
    msg_t msg;
    snapshotInfo_t info;
    int start = svs.nextArchivedSnapshotBuffer;
    memset(&info, 0, sizeof(info));
    info.archived = 1;
    info.fromBaseline = 1;
    MSG_Init(&msg, svs.archivedSnapshotBuffer + start, ARCHIVEDSSBUF_SIZE - start);
#ifndef NDEBUG
    MSG_WriteLong(&msg, 0xdeadbeef);
#endif
    MSG_WriteBit1(&msg);
#ifndef NDEBUG
    MSG_WriteLong(&msg, 0xdeadbee1);
#endif
    MSG_WriteLong(&msg, TIME + index * 50);
    MSG_ClearLastReferencedEntity(&msg);
    MSG_WriteBit0(&msg); /* no clients */
#ifndef NDEBUG
    MSG_WriteLong(&msg, 0xdeadbee9);
#endif
    MSG_ClearLastReferencedEntity(&msg);
#ifndef NDEBUG
    MSG_WriteLong(&msg, 0xdeadbee7);
#endif
    MSG_WriteDeltaArchivedEntity(&info, &msg, TIME, &sv.svEntities[LID].baseline, entity, DELTA_FLAGS_FORCE);
    EntityStateExtWrite(&msg, &entity->s, NULL);
#ifndef NDEBUG
    MSG_WriteLong(&msg, 0xdeadbee7);
#endif
    MSG_WriteEntityIndex(&info, &msg, ENTITYNUM_NONE, GENTITYNUM_BITS);
    assert(!msg.overflowed);
    svs.archivedSnapshotFrames[index % NUM_ARCHIVED_FRAMES].start = start;
    svs.archivedSnapshotFrames[index % NUM_ARCHIVED_FRAMES].size = msg.cursize;
    svs.nextArchivedSnapshotBuffer = start + msg.cursize;
    svs.nextArchivedSnapshotFrames = index + 1;
    return msg.cursize;
}

int main(void)
{
    archivedEntity_t *lid = &svs.cachedSnapshotEntities[SOURCE];
    cachedSnapshot_t *cached;
    unsigned entry = 99, tree = 99;

    shownet.integer = -1;
    msg_printEntityNums = &print_entity_nums;
    svs.time = 100000;
    svs.numCachedSnapshotEntities = sizeof(svs.cachedSnapshotEntities) / sizeof(svs.cachedSnapshotEntities[0]);
    svs.numCachedSnapshotClients = sizeof(svs.cachedSnapshotClients) / sizeof(svs.cachedSnapshotClients[0]);
    /* Decoded entities land well below the fixture's source slot. */
    svs.nextCachedSnapshotEntities = 0;
    SV_ResetCachedSnapshotFailures();

    memset(lid, 0, sizeof(*lid));
    lid->s.number = LID;
    lid->s.eType = ET_SCRIPTMOVER;
    lid->s.solid = 0;
    lid->s.lerp.pos.trBase[0] = 128.0f;
    lid->r.svFlags = 0;
    sv.svEntities[LID].baseline.s.number = LID;
    /* The bound mystery-box lid: client tree 8, no entity-state play. */
    assert(EntityStateModelAnimCaptured(&lid->s, 0, 8));
    /* An entity-state play needs both halves; a play without a tree is not a
     * representable state. */
    assert(!EntityStateModelAnimCaptured(&lid->s, 5, 0));

    /* 1. The frame decodes, and the lid keeps its tree with no play. */
    write_frame(0, lid);
    cached = SV_GetCachedSnapshotInternal(0, 0, 1);
    assert(cached);
    assert(cached->num_entities == 1);
    {
        archivedEntity_t *decoded = &svs.cachedSnapshotEntities[cached->first_entity % svs.numCachedSnapshotEntities];
        assert(decoded->s.number == LID && decoded->s.eType == ET_SCRIPTMOVER);
        assert(EntityStateModelAnim(&decoded->s, &entry, &tree));
        assert(entry == 0 && tree == 8);
    }
    assert(SV_CachedSnapshotFailureCount() == 0);

    /* A full play (entry, tree) still round-trips. */
    assert(EntityStateModelAnimCaptured(&lid->s, 2, 8));
    write_frame(1, lid);
    cached = SV_GetCachedSnapshotInternal(1, 0, 1);
    assert(cached && cached->num_entities == 1);
    assert(EntityStateModelAnim(&svs.cachedSnapshotEntities[cached->first_entity % svs.numCachedSnapshotEntities].s,
                                      &entry, &tree));
    assert(entry == 2 && tree == 8);

    /* 2. A malformed frame, of the same class as the production one: a record
     * the writer emits but the reader refuses. The selector stays captured
     * while the entity is archived as a non-script-mover, which the extension
     * reader rejects by flagging the message. The server must not Com_Error
     * (the stub aborts); the lookup simply misses. */
    assert(EntityStateModelAnimCaptured(&lid->s, 0, 8));
    lid->s.eType = ET_GENERAL;
    write_frame(2, lid);
    lid->s.eType = ET_SCRIPTMOVER;
    warnings = 0;
    assert(!SV_GetCachedSnapshotInternal(2, 0, 1));
    assert(SV_CachedSnapshotFailureCount() == 1 && warnings == 1);
    /* Remembered: not decoded (or reported) again. */
    assert(!SV_GetCachedSnapshotInternal(2, 0, 1));
    assert(SV_CachedSnapshotFailureCount() == 1 && warnings == 1);
    /* The good frames are untouched and still served from the cache. */
    cached = SV_GetCachedSnapshotInternal(0, 0, 1);
    assert(cached && cached->archivedFrame == 0 && cached->num_entities == 1);
    /* A map's archive reset forgets the failure. */
    SV_ResetCachedSnapshotFailures();
    assert(SV_CachedSnapshotFailureCount() == 0);

    /* Explicit vehicle material follows the captured archived state, including
     * subsequent live changes and clearing back to automatic team selection. */
    assert(EntityStateExtClear(&lid->s));
    lid->s.eType=ET_HELICOPTER;
    EntityStateVehicleCaptured(&lid->s,0,4,1);
    assert(EntityStateVehicleCompassCaptured(&lid->s,7));
    assert(!EntityStateVehicleCompassCaptured(&lid->s,16));
    write_frame(3,lid);
    assert(EntityStateVehicleCompassCaptured(&lid->s,8));
    cached=SV_GetCachedSnapshotInternal(3,0,1);
    assert(cached && cached->num_entities==1);
    assert(EntityStateVehicleCompass(&svs.cachedSnapshotEntities[
        cached->first_entity % svs.numCachedSnapshotEntities].s)==7);
    assert(EntityStateVehicleCompassCaptured(&lid->s,0));
    write_frame(4,lid);
    cached=SV_GetCachedSnapshotInternal(4,0,1);
    assert(cached && cached->num_entities==1);
    assert(EntityStateVehicleCompass(&svs.cachedSnapshotEntities[
        cached->first_entity % svs.numCachedSnapshotEntities].s)==0);

    puts("PASS: archived (0,tree) script-model selector decodes; malformed archived frame is dropped, not fatal");
    return 0;
}
