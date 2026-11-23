/* The survival category's C1 hook (code and callback); survival_category.c installs it, and the x86 test fixture
 * (tests/survival_category_x86_fixture.c) runs the same code against C1's own functions in Unicorn.
 * The includer defines K33_SURV_LOG(...). See survival_category.c for the measurements.
 */
#ifndef K33_SURVIVAL_CATEGORY_HOOKS_H
#define K33_SURVIVAL_CATEGORY_HOOKS_H
#include "survival_category_policy.h"

/* 0x4CCDF0 (zone list for a map: ecx = category, edx = map). The category branch starts at 0x4CCEB5. */
#define K33_SURV_ZONE_HOOK_VA     0x004CCEB5u /* mov edi, [ebp-0x1EC] ; push edi ; strnicmp(edi, "zm", 2) */
#define K33_SURV_ZONE_RESUME_VA   0x004CCEBBu /* push edi */
#define K33_SURV_ZONE_MP_VA       0x004CD03Eu /* the mp branch: DB_IsZoneLoaded("common_gametype_mp"), its 4 zones */
#define K33_SURV_DB_ZONE_LOADED_VA 0x004CD7D0u /* char __cdecl DB_IsZoneLoaded(const char *name) */

typedef struct { const char *name; int allocFlags; int freeFlags; } K33SurvZoneInfo;
typedef unsigned char (__cdecl *K33SurvZoneLoadedFn)(const char *name);

__attribute__((used)) unsigned g_k33SurvZoneResume = K33_SURV_ZONE_RESUME_VA;
__attribute__((used)) unsigned g_k33SurvZoneMp = K33_SURV_ZONE_MP_VA;
__attribute__((used)) volatile long g_k33SurvZoneLists; /* counter (the tests) */

/* 0x4CCDF0's list at ebp-0x1E0 (count at ebp-0x1E4, entries {name, allocFlags, freeFlags}): returns 0 when the
 * category is not survival (C1's own branch runs), else 1 with the four survival zones appended unless
 * common_gametype_survival is already loaded (C1 does the same per category); C1's mp branch then runs. */
__attribute__((used)) int __cdecl K33SurvivalZones(const char *category, K33SurvZoneInfo *list, int *count)
{
    int i, n;
    if (!K33IsSurvivalCategory(category)) return 0;
    g_k33SurvZoneLists++;
    if (((K33SurvZoneLoadedFn)K33_SURV_DB_ZONE_LOADED_VA)(K33_SURVIVAL_GAMETYPE_ZONE)) {
        K33_SURV_LOG("survival category: %s already loaded", K33_SURVIVAL_GAMETYPE_ZONE);
        return 1;
    }
    n = *count;
    for (i = 0; i < 4; i++) {
        list[n + i].name = kK33SurvivalZones[i];
        list[n + i].allocFlags = K33_SURVIVAL_ZONE_ALLOC_FLAGS;
        list[n + i].freeFlags = 0;
    }
    *count = n + 4;
    K33_SURV_LOG("survival category: %s (+patch_, localized_) queued ahead of the mp gametype zones",
                 K33_SURVIVAL_GAMETYPE_ZONE);
    return 1;
}

void K33SurvivalZoneHook(void);
__asm__(
    ".text\n"
    /* at 0x4CCEB5 (6 bytes): ebp = 0x4CCDF0's frame, ebx = 0, edi = the map */
    ".globl _K33SurvivalZoneHook\n"
    "_K33SurvivalZoneHook:\n"
    "    movl -0x1ec(%ebp), %edi\n"        /* the replaced instruction: edi = the category */
    "    leal -0x1e4(%ebp), %eax\n"
    "    pushl %eax\n"
    "    leal -0x1e0(%ebp), %eax\n"
    "    pushl %eax\n"
    "    pushl %edi\n"
    "    call _K33SurvivalZones\n"
    "    addl $12, %esp\n"
    "    testl %eax, %eax\n"
    "    jnz 1f\n"
    "    jmp *_g_k33SurvZoneResume\n"
    "1:  movl $4, %esi\n"                   /* the gametype zones' alloc flags, as 0x4CCECE sets them */
    "    jmp *_g_k33SurvZoneMp\n");         /* then C1's mp branch, as for any category that is not zm/coop */


/* C1's crosshair scan (entry 0x439940, mutated into .tvm0) looks at actors (eType 18) only when its mode
 * predicate at 0x8458F2A is true, and stock that is (category == coop): in every other category an actor under
 * the crosshair is skipped before the team test, so the crosshair never turns red (ps weapFlags 0x10) or
 * friendly (0x08) over one. Survival and zombies run their AI as the same ET_ACTOR, with the actor's sentient
 * team in actorInfo +14 (cg+0x168528+slot*0x29C; the server's actor_state.cpp), so C1's own actor arm, with its
 * team/range/dead/FX filters, runs there too. Zombies and hellhounds are axis and zombies players allies, so they
 * classify as enemies, never friendlies (0x08 is also C1's fire lock, 0x41DAAE). The category is not changed. */
#define K33_SURV_CROSSHAIR_HOOK_VA 0x08458F2Au
#define K33_SURV_CROSSHAIR_RESUME_VA 0x08458F31u
__attribute__((used)) unsigned g_k33SurvCrosshairResume=K33_SURV_CROSSHAIR_RESUME_VA;
__attribute__((used)) int __cdecl K33SurvivalCrosshairMode(void)
{
    unsigned index=*(const unsigned *)0x06C3D2B0u;
    const char *cgs;
    if(!Readable((const void *)(0x016D3B58u+index*4),4))return 0;
    cgs=(const char *)(*(const unsigned *)(0x016D3B58u+index*4)^0x3958BEADu);
    return Readable(cgs+0x20,9) && K33IsActorCrosshairCategory(cgs+0x20);
}
void K33SurvivalCrosshairHook(void);
__asm__(
    ".text\n"
    ".globl _K33SurvivalCrosshairHook\n"
    "_K33SurvivalCrosshairHook:\n"
    " pushal\n"
    " call _K33SurvivalCrosshairMode\n"
    " testl %eax,%eax\n"
    " jnz 1f\n"
    " popal\n"
    " testb %al,4(%esp)\n" /* replaced stock predicate, unchanged for every other mode */
    " jmp *_g_k33SurvCrosshairResume\n"
    "1: popal\n"
    " pushl %eax\n"
    " movl $1,%eax\n"
    " testb %al,%al\n" /* true mode predicate; original actor filters follow */
    " popl %eax\n"
    " jmp *_g_k33SurvCrosshairResume\n");
#endif
