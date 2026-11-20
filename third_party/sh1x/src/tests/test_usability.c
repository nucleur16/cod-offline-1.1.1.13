#include "scr_vm_functions.h"
#include "weapon_flags.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

gentity_t g_entities[MAX_GENTITIES];
static gclient_t player;
static unsigned errors, clears, script_checks;
void Player_UpdateActivate(gentity_t *);
void Player_UpdateCursorHints(gentity_t *);
void Scr_ObjectError(const char *s) { assert(s && *s); ++errors; }
/* allowMelee's boolean argument. */
static int script_int;
int Scr_GetInt(unsigned int index) { assert(index==0); return script_int; }
/* The cursor-hint pass asks this first; no grenade is being thrown back. */
int BG_ThrowingBackGrenade(const playerState_t *ps) { assert(ps==&player.ps); return 0; }
qboolean Scr_IsSystemActive(unsigned char system) { assert(system==1); ++script_checks; return 0; }
/* The activation assembly consults two CoDOL hooks that live in the script
 * area. Linking their real translation units here would pull the whole script
 * VM in for behaviour this harness does not exercise, so they are stubbed the
 * way the VM entry points above are, and the counters below assert the
 * assembly still reaches them. */
static unsigned use_hint_calls, use_allowed_calls;
static qboolean use_hint_result;
qboolean ForceUseHint_Apply(gentity_t *entity)
{ assert(entity); ++use_hint_calls; return use_hint_result; }
int PlayerUseAllowed(const gentity_t *player,const gentity_t *target)
{ assert(player); (void)target; ++use_allowed_calls; return 1; }
float PlayerUseRadius(void) { return 128.0f; }

void TestSetHandle(EntHandle *,gentity_t *) __asm__("_ZN9EntHandle6setEntEP9gentity_s");
void TestSetHandle(EntHandle *h,gentity_t *ent)
{
    assert(h==&player.useHoldEntity && !ent);
    memset(h,0,sizeof(*h)); ++clears;
}
int main(void)
{
    scr_entref_t ref={0}; unsigned i;
    _Static_assert(offsetof(gclient_t,ps.weapFlags)==0x10,"native flag ABI");
    _Static_assert(offsetof(gclient_t,buttons)==0x3088,"native button ABI");
    _Static_assert(offsetof(gclient_t,useHoldEntity)==0x30f8,"native handle ABI");
    _Static_assert(offsetof(gclient_t,useButtonDone)==0x3100,"native use ABI");
    ref.entnum=2; g_entities[2].client=&player;
    for(i=0;i<8;++i) {
        memset(&player,0,sizeof(player));
        player.ps.weapFlags=0x1883;
        player.buttons=(i&1)?0x20:0;
        player.oldbuttons=(i&2)?0x20:0;
        player.latched_buttons=(i&4)?0x28:0;
        memset(&player.useHoldEntity,0x01,sizeof(player.useHoldEntity));
        Scr_DisableUsability(ref);
        Scr_DisableUsability(ref);
        assert(player.ps.weapFlags==0x3883);
        Player_UpdateActivate(&g_entities[2]);
        assert(player.ps.weapFlags==(0x3882|((i&2)&&!(i&1))));
        assert(player.useButtonDone==1 && clears==i+1 && script_checks==0);
        Scr_EnableUsability(ref);
        Scr_EnableUsability(ref);
        assert(!(player.ps.weapFlags&USE_DISABLED));
        assert((player.ps.weapFlags&~1)==0x1882);
    }
    memset(&player,0,sizeof(player));
    player.latched_buttons=0x8;
    Player_UpdateActivate(&g_entities[2]);
    assert(script_checks==1 && clears==8 && !player.useButtonDone);
    for(i=0;i<3;++i) {
        ref.entnum=i==0?3:i==1?MAX_GENTITIES:2; ref.classnum=i==2?2:0;
        Scr_DisableUsability(ref);
        Scr_EnableUsability(ref);
        assert(errors==2*(i+1) && !player.ps.weapFlags);
    }
    /* Player_UpdateCursorHints reaches the forced-hint hook (the first
     * service after the use restrictions; returning true ends the pass there)
     * only while usability is enabled: a player who cannot use is offered no
     * hint. */
    ref.entnum=2; ref.classnum=0;
    memset(&player,0,sizeof(player));
    g_entities[2].health=100; use_hint_result=qtrue;
    assert(use_hint_calls==0);
    Player_UpdateCursorHints(&g_entities[2]);
    assert(use_hint_calls==1);
    Scr_DisableUsability(ref);
    player.ps.cursorHint=7;
    Player_UpdateCursorHints(&g_entities[2]);
    assert(use_hint_calls==1 && player.ps.cursorHint==0);
    Scr_EnableUsability(ref);
    Player_UpdateCursorHints(&g_entities[2]);
    assert(use_hint_calls==2);
    /* allowMelee( false ) sets C1's no-melee pm_flags bit 0x8000000 (C1
     * 0x51b040) and allowMelee( true ) clears it, leaving every other bit. */
    player.ps.pm_flags=0x20005;
    script_int=0; Scr_AllowMelee(ref);
    assert(player.ps.pm_flags==0x8020005);
    Scr_AllowMelee(ref);
    assert(player.ps.pm_flags==0x8020005);
    script_int=1; Scr_AllowMelee(ref);
    assert(player.ps.pm_flags==0x20005);
    ref.entnum=3; Scr_AllowMelee(ref);
    assert(errors==7);
    /* The activation cases above have no candidate use target. */
    (void)use_allowed_calls;
    puts("PASS: native activation assembly enforces usability, cancels holds, preserves release edges and resumes enabled processing; no use hint while disabled; allowMelee owns pm_flags 0x8000000");
}
