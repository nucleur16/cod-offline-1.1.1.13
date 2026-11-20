/* Native candidate selection regression: interaction must respect the same
 * per-client hide masks, including clients joining later. Invisible
 * trigger_use brushes remain usable despite SVF_NOCLIENT. */
#include "script_playeruse.h"
#include "g_public.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static gentity_t players[MAX_CLIENTS], target, second;
static gclient_t client;
static gentity_t *argument;
static unsigned param_errors, object_errors;
static const char *zone = "mp";

gentity_t *Scr_GetEntity(unsigned index) { assert(index == 0); return argument; }
void Scr_ParamError(int index, const char *message)
{ assert(index == 0 && message && *message); ++param_errors; }
void Scr_ObjectError(const char *message)
{ assert(message && *message); ++object_errors; }
const char *ServerZoneType(void) { return zone; }

static scr_entref_t reference(gentity_t *entity)
{ scr_entref_t ref = {(uint16_t)entity->s.number, 0}; return ref; }
static void disable(unsigned slot, gentity_t *entity)
{ argument = &players[slot]; Scr_DisablePlayerUse(reference(entity)); }
static void enable(unsigned slot, gentity_t *entity)
{ argument = &players[slot]; Scr_EnablePlayerUse(reference(entity)); }
static void initialize(void)
{
    memset(players, 0, sizeof(players));
    memset(&target, 0, sizeof(target));
    memset(&second, 0, sizeof(second));
    for (unsigned i = 0; i < MAX_CLIENTS; ++i) {
        players[i].s.number = (int)i;
        players[i].client = &client;
    }
    target.s.number = 100;
    second.s.number = 101;
    PlayerUseResetAll();
}

static void disabled_use_lifecycle(void)
{
    const unsigned clients[] = {0, 31, 32, 63};
    const int targets[] = {31, 32, 63, 64, MAX_GENTITIES - 3};
    for (unsigned i = 0; i < sizeof(clients)/sizeof(clients[0]); ++i) {
        unsigned slot = clients[i];
        for (unsigned j = 0; j < sizeof(targets)/sizeof(targets[0]); ++j) {
            target.s.number = targets[j];
            assert(PlayerUseAllowed(&players[slot], &target));
            disable(slot, &target);
            assert(!PlayerUseAllowed(&players[slot], &target));
            assert(PlayerUseAllowed(&players[(slot + 1) % MAX_CLIENTS], &target));
            assert(PlayerUseAllowed(&players[slot], &second));
            enable(slot, &target);
            assert(PlayerUseAllowed(&players[slot], &target));
        }
    }
    target.s.number = 100;
    disable(0, &target); disable(31, &target); disable(0, &second);
    PlayerUseResetClient(0);
    assert(PlayerUseAllowed(&players[0], &target));
    assert(PlayerUseAllowed(&players[0], &second));
    assert(!PlayerUseAllowed(&players[31], &target));
    PlayerUseClearTarget(target.s.number); /* G_FreeEntity and target reuse. */
    assert(PlayerUseAllowed(&players[31], &target));
    disable(31, &target); disable(32, &target); disable(32, &second);
    PlayerUseClearTarget(target.s.number);
    assert(PlayerUseAllowed(&players[31], &target));
    assert(PlayerUseAllowed(&players[32], &target));
    assert(!PlayerUseAllowed(&players[32], &second));
    PlayerUseResetAll();
    assert(PlayerUseAllowed(&players[32], &second));
    PlayerUseClearTarget(-1); PlayerUseClearTarget(MAX_GENTITIES);
    PlayerUseResetClient(-1); PlayerUseResetClient(MAX_CLIENTS);
    puts("PASS: per-player disable/enable, client reset, all reset and reused target isolation");
}

static void visibility(void)
{
    const unsigned owners[] = {0, 31, 32, 63};
    initialize();
    for (unsigned i = 0; i < MAX_CLIENTS; ++i)
        assert(PlayerUseAllowed(&players[i], &target));
    /* Native trigger_use_shared sets NOCLIENT on an ordinary usable brush. */
    target.r.contents = 0x200000;
    target.s.un2 = 1;
    target.r.svFlags |= SVF_NOCLIENT;
    for (unsigned i = 0; i < MAX_CLIENTS; ++i)
        assert(PlayerUseAllowed(&players[i], &target));
    target.r.svFlags &= ~SVF_NOCLIENT;
    for (unsigned owner = 0; owner < sizeof(owners)/sizeof(owners[0]); ++owner) {
        unsigned slot = owners[owner];
        target.r.clientMask[0] = target.r.clientMask[1] = (int)UINT32_MAX;
        target.r.clientMask[slot >> 5] &= (int)~(UINT32_C(1) << (slot & 31));
        for (unsigned i = 0; i < MAX_CLIENTS; ++i)
            assert(PlayerUseAllowed(&players[i], &target) == (i == slot));
        /* A new connection has fresh disabled-use state, but its mask bit
         * must still forbid a pickup belonging to someone else. */
        PlayerUseResetClient(17);
        assert(!PlayerUseAllowed(&players[17], &target));
        disable(slot, &target);
        assert(!PlayerUseAllowed(&players[slot], &target));
        enable(slot, &target);
        assert(PlayerUseAllowed(&players[slot], &target));
        target.r.svFlags |= SVF_NOCLIENT;
        assert(PlayerUseAllowed(&players[slot], &target));
        target.r.svFlags &= ~SVF_NOCLIENT;
    }
    target.r.clientMask[0] = target.r.clientMask[1] = 0;
    assert(PlayerUseAllowed(&players[17], &target));
    puts("PASS: invisible native use brushes, hide masks, owner-only slots 0/31/32/63 and later joins");
}

static void invalid_arguments(void)
{
    initialize();
    assert(!PlayerUseAllowed(NULL, &target));
    assert(!PlayerUseAllowed(&players[0], NULL));
    players[0].client = NULL;
    assert(!PlayerUseAllowed(&players[0], &target));
    argument = &players[0]; Scr_DisablePlayerUse(reference(&target));
    assert(param_errors == 1);
    players[0].client = &client;
    players[0].s.number = MAX_CLIENTS;
    assert(!PlayerUseAllowed(&players[0], &target));
    Scr_DisablePlayerUse(reference(&target)); assert(param_errors == 2);
    players[0].s.number = 0;
    scr_entref_t bad_class = {100, 1};
    Scr_DisablePlayerUse(bad_class); assert(object_errors == 1);
    target.s.number = MAX_GENTITIES;
    assert(!PlayerUseAllowed(&players[0], &target));
    Scr_DisablePlayerUse(reference(&target)); assert(object_errors == 2);
    zone = "mp"; assert(PlayerUseRadius() == 128.0f);
    zone = "zm"; assert(PlayerUseRadius() == 72.0f);
    zone = NULL; assert(PlayerUseRadius() == 128.0f);
    puts("PASS: invalid receivers/players/targets and category use radii");
}

int main(void)
{
    assert(MAX_CLIENTS == 64);
    initialize();
    disabled_use_lifecycle();
    visibility();
    invalid_arguments();
    return 0;
}
