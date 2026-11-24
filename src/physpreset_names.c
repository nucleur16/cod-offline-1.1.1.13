/*
 * physpreset_names.c - physics presets a zone loads without a name or a sound prefix. Included by vlobby_env.c next
 * to smodel_sort.c; installed for every map (InstallPhysPresetNames).
 *
 * When a map starts, C1 (1.1.1.13) registers the collision sounds of every physpreset asset (0x0046DE40, from
 * CG_RegisterSounds 0x0046E340): it enumerates them in hash order and reads each one's sound prefix (+0x1C) without
 * testing it, for the sound names (0x0046DC20) and to find presets of the same prefix (0x0046DEF6).
 *
 * mp_offshore (ported from 3.19.26.22) reaches a preset whose prefix is NULL. Its model sofa_pillow_modern_04 has
 * the preset ",default" as a stub (the name, then 64 zero bytes), and the name is written as an offset to another
 * stub's name further on in the zone (the ",default" techset inside gfx_map). Nothing is there yet when the stub is
 * linked, so C1 reads the name "": no ',' makes it an asset of its own, "" hashes to the first slot, and the first
 * preset CG_RegisterSounds looks at has no prefix (AV read 0 at 0x0046DC36, multiplayer and Survival alike).
 *
 * The repair, after C1 has read a preset (0x004BB3F0) and before it links it (0x004CC390): a stub (no mass, no prefix)
 * without a name becomes ",default" (C1's own preset "default", what the type falls back to everywhere else), and a
 * missing prefix becomes "", which C1 takes as "no sounds of its own" (collision_default). A whole preset whose name
 * is not there yet (mp_sd_jardin's on another model) keeps its values under the name "", which C1 copes with.
 *
 * C1 sites (1.1.1.13), byte-checked; on a mismatch one line is logged and nothing is changed:
 *   0x004BB563  call 0x004BB3F0 (Load_PhysPreset, its only caller), replaced by a call to PhysPresetLoad
 *   0x004BB3F0  reads the 0x44 bytes at [0x017C6974] (varPhysPreset), the name at +0, the prefix at +0x1C
 */

#define PPN_CALL_VA   0x004BB563u
#define PPN_LOAD_VA   0x004BB3F0u
#define PPN_VAR_VA    0x017C6974u
#define PPN_MASS      0x08u
#define PPN_PREFIX    0x1Cu
#define PPN_SIZE      0x44u

static void __cdecl PhysPresetLoad(void)
{
    ((void (__cdecl *)(void))(DWORD_PTR)PPN_LOAD_VA)();
    BYTE *preset = *(BYTE *const *)(DWORD_PTR)PPN_VAR_VA;
    if (!Readable(preset, PPN_SIZE)) return;
    const char **name = (const char **)preset, **prefix = (const char **)(preset + PPN_PREFIX);
    if (*prefix) return;
    if ((!*name || !**name) && !*(const DWORD *)(preset + PPN_MASS)) {
        *name = ",default";
        Log("physpreset: a zone refers to one before its name is loaded; C1's \"default\" stands in");
    }
    *prefix = "";
}

static void InstallPhysPresetNames(void)
{
    static const BYTE kLoadHead[16] = { 0x83, 0x3D, 0x44, 0xDC, 0xEB, 0x01, 0x02, 0x56, 0x75, 0x15, 0x8B, 0x35, 0x74, 0x69, 0x7C, 0x01 };
    static const BYTE kLoadName[8] = { 0x8B, 0x06, 0x89, 0x35, 0xEC, 0x6E, 0x7C, 0x01 };    /* 0x004BB435 mov eax, [esi] */
    static const BYTE kLoadPrefix[8] = { 0x8B, 0x4E, 0x1C, 0x8D, 0x46, 0x1C, 0xA3, 0xEC };  /* 0x004BB47C mov ecx, [esi+0x1C] */
    static const BYTE kCall[14] = { 0xE8, 0x88, 0xFE, 0xFF, 0xFF, 0xA1, 0x08, 0x6F, 0x7C, 0x01, 0x8B, 0x08, 0x68, 0x5C };
    BYTE *call = (BYTE *)(DWORD_PTR)PPN_CALL_VA;
    if (!Readable(call, sizeof(kCall)) || memcmp(call, kCall, sizeof(kCall)) ||
        !Readable((void *)(DWORD_PTR)PPN_LOAD_VA, 0x94) ||
        memcmp((void *)(DWORD_PTR)PPN_LOAD_VA, kLoadHead, sizeof(kLoadHead)) ||
        memcmp((void *)(DWORD_PTR)0x004BB435u, kLoadName, sizeof(kLoadName)) ||
        memcmp((void *)(DWORD_PTR)0x004BB47Cu, kLoadPrefix, sizeof(kLoadPrefix))) {
        Log("physpreset: the preset loader 0x%08X or its call 0x%08X differs; not repaired", PPN_LOAD_VA, PPN_CALL_VA);
        return;
    }
    BYTE patch[5] = { 0xE8 };
    LONG to = (LONG)(DWORD_PTR)PhysPresetLoad - (LONG)(PPN_CALL_VA + 5);
    memcpy(patch + 1, &to, 4);
    WriteCode(call, patch, sizeof(patch));
}
