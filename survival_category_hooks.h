/* The survival category's three C1 hooks (code and callbacks); survival_category.c installs them, and the x86 test
 * fixture (tests/survival_category_x86_fixture.c) runs the same code against C1's own functions in Unicorn.
 * The includer defines K33_SURV_LOG(...) and K33_SURV_READABLE(p, n). See survival_category.c for the measurements.
 */
#ifndef K33_SURVIVAL_CATEGORY_HOOKS_H
#define K33_SURVIVAL_CATEGORY_HOOKS_H
#include "survival_category_policy.h"

/* 0x4CCDF0 (zone list for a map: ecx = category, edx = map). The category branch starts at 0x4CCEB5. */
#define K33_SURV_ZONE_HOOK_VA     0x004CCEB5u /* mov edi, [ebp-0x1EC] ; push edi ; strnicmp(edi, "zm", 2) */
#define K33_SURV_ZONE_RESUME_VA   0x004CCEBBu /* push edi */
#define K33_SURV_ZONE_LOAD_VA     0x004CD0E1u /* mov eax, [ebp-0x1E4] ; DB_LoadXAssets(list, eax, 0) */
/* 0x4B6380 (the load-size estimate behind the loading bar: ecx/esi = category). */
#define K33_SURV_SIZE_HOOK_VA     0x004B6461u /* push esi ; mov eax, 2 ; ... strnicmp(esi, "zm", 2) */
#define K33_SURV_SIZE_RESUME_VA   0x004B6467u /* mov edx, "zm" */
#define K33_SURV_SIZE_SKIP_VA     0x004B6596u /* after the gametype zones: the common_mp part */
/* 0x612850 (const configstring table for edi = category). */
#define K33_SURV_CONST_HOOK_VA    0x00612850u /* push esi ; push edi ; mov eax, 4 */
#define K33_SURV_CONST_RESUME_VA  0x00612857u /* mov edx, "coop" */
#define K33_SURV_CONST_TAIL_VA    0x0061288Eu /* push esi (the table name) ; DB_FindXAssetHeader(0x25, name) ; ... */
#define K33_SURV_CONST_TABLE_VA   0x02A99A44u /* the table (StringTable *) */
#define K33_SURV_CONST_CRC_VA     0x02A99A50u /* its checksum, compared with the gamestate's */
#define K33_SURV_DB_ZONE_LOADED_VA 0x004CD7D0u /* char __cdecl DB_IsZoneLoaded(const char *name) */

typedef struct { const char *name; int allocFlags; int freeFlags; } K33SurvZoneInfo;
typedef unsigned char (__cdecl *K33SurvZoneLoadedFn)(const char *name);

__attribute__((used)) unsigned g_k33SurvZoneResume = K33_SURV_ZONE_RESUME_VA;
__attribute__((used)) unsigned g_k33SurvZoneLoad = K33_SURV_ZONE_LOAD_VA;
__attribute__((used)) unsigned g_k33SurvSizeResume = K33_SURV_SIZE_RESUME_VA;
__attribute__((used)) unsigned g_k33SurvSizeSkip = K33_SURV_SIZE_SKIP_VA;
__attribute__((used)) unsigned g_k33SurvConstResume = K33_SURV_CONST_RESUME_VA;
__attribute__((used)) unsigned g_k33SurvConstTail = K33_SURV_CONST_TAIL_VA;
__attribute__((used)) const char g_k33SurvConstName[] = K33_SURVIVAL_CONST_TABLE;
__attribute__((used)) volatile long g_k33SurvZoneLists, g_k33SurvConstTables; /* counters (the log, the tests) */

__attribute__((used)) int __cdecl K33SurvivalCategoryC(const char *category)
{
    return K33IsSurvivalCategory(category);
}

/* 0x4CCDF0's list at ebp-0x1E0 (count at ebp-0x1E4, entries {name, allocFlags, freeFlags}): returns 0 when the
 * category is not survival (C1's own branch runs), else 1 with the four survival zones appended unless
 * common_gametype_survival is already loaded (C1 does the same per category). */
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
    K33_SURV_LOG("survival category: %s (+patch_, localized_) queued", K33_SURVIVAL_GAMETYPE_ZONE);
    return 1;
}

/* After C1 looked the survival table up and checksummed it (0x61288E..0x6128BB): store the shared checksum
 * (survival_category_policy.h). A table that is not ours (DB_FindXAssetHeader's default mp/defaultStringTable.csv when
 * no loaded zone has it) keeps C1's checksum, so the server refuses the gamestate instead of a wrong table loading. */
__attribute__((used)) void __cdecl K33SurvivalConstLoaded(void)
{
    const K33SurvStringTable *t = *(const K33SurvStringTable *const *)K33_SURV_CONST_TABLE_VA;
    volatile unsigned *crc = (volatile unsigned *)K33_SURV_CONST_CRC_VA;
    unsigned long n;
    g_k33SurvConstTables++;
    if (!t || !K33_SURV_READABLE(t, sizeof(*t)) || !t->name || !K33_SURV_READABLE(t->name, 1) ||
        !K33SurvEqualI(t->name, K33_SURVIVAL_CONST_TABLE)) {
        K33_SURV_LOG("survival category: %s is in no loaded zone (install codol_common.ff): the config string "
                     "checksum stays C1's 0x%08X and the server will refuse the match", K33_SURVIVAL_CONST_TABLE, *crc);
        return;
    }
    if (t->rowCount < 0 || t->columnCount < 0 || t->rowCount > 0x10000 || t->columnCount > 64) return;
    n = (unsigned long)t->rowCount * (unsigned long)t->columnCount;
    if (n && (!t->values || !K33_SURV_READABLE(t->values, n * sizeof(K33SurvStringTableCell)))) return;
    {
        unsigned native = *crc, ours = K33SurvTableChecksum(t->values, n);
        *crc = ours;
        K33_SURV_LOG("survival category: %s, %d rows: config string checksum 0x%08X (%d) in place of C1's 0x%08X",
                     K33_SURVIVAL_CONST_TABLE, t->rowCount, ours, (int)ours, native);
    }
}

void K33SurvivalZoneHook(void);
void K33SurvivalSizeHook(void);
void K33SurvivalConstHook(void);
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
    "1:  movl $4, %esi\n"                   /* the map part's alloc flags, as 0x4CCECE sets them */
    "    jmp *_g_k33SurvZoneLoad\n"
    /* at 0x4B6461 (6 bytes): esi = the category */
    ".globl _K33SurvivalSizeHook\n"
    "_K33SurvivalSizeHook:\n"
    "    pushl %esi\n"
    "    call _K33SurvivalCategoryC\n"
    "    addl $4, %esp\n"
    "    testl %eax, %eax\n"
    "    jnz 1f\n"
    "    pushl %esi\n"                      /* the replaced instructions */
    "    movl $2, %eax\n"
    "    jmp *_g_k33SurvSizeResume\n"
    "1:  jmp *_g_k33SurvSizeSkip\n"         /* no stock size is known for the survival zones */
    /* at 0x612850 (7 bytes), the function's entry: edi = the category */
    ".globl _K33SurvivalConstHook\n"
    "_K33SurvivalConstHook:\n"
    "    pushl %edi\n"
    "    call _K33SurvivalCategoryC\n"
    "    addl $4, %esp\n"
    "    testl %eax, %eax\n"
    "    jnz 1f\n"
    "    pushl %esi\n"                      /* the replaced instructions */
    "    pushl %edi\n"
    "    movl $4, %eax\n"
    "    jmp *_g_k33SurvConstResume\n"
    "1:  call 2f\n"                         /* C1's own lookup, checksum and stores, with the survival name */
    "    call _K33SurvivalConstLoaded\n"
    "    xorl %eax, %eax\n"                 /* as C1's tail leaves it */
    "    ret\n"
    "2:  pushl %esi\n"                      /* 0x612850's saved esi, popped by the tail */
    "    movl $_g_k33SurvConstName, %esi\n"
    "    jmp *_g_k33SurvConstTail\n");

#endif
