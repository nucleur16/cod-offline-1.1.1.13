---
slug: subsystem-script
title: The GSC script surface
space: subsystems
summary: src/codol/script — where scripts are found, the syntax-lowering pass that makes CoDOL GSC parseable, childthread, and the ~880 builtins scripts can call.
position: 50
tags: [script]
---

The server runs Game Script Code written for three different games at once: the
IW4 vendor scripts, the alpha0022 overlay, and native adaptations written for
this project. This area is what makes that work — finding the source, adapting
its syntax, and providing the builtins it calls.

For how the VM underneath executes it, see [The GSC virtual
machine](/wiki/script-vm). For the builtin list itself, see [GSC
builtins](/reference/gsc).

## Where a script comes from

```c
int CODOL_ReadScriptSourceWithNativeOverlay(const char *base, const char *overlay,
                                            const char *native_overlay, const char *name,
                                            char **out, size_t *length);
```

Three roots, searched in precedence order, matching the launcher's three dvars:

| Precedence | Root | Launcher setting |
|---|---|---|
| 1 | Native overlay — this project's adaptations | `codol_scriptNativeOverlay` (`scripts/codol`) |
| 2 | User overlay — alpha0022's `gsc_overlay` | `codol_scriptOverlay` |
| 3 | Reference — the IW4 vendor scripts, `base/mp` then `base` | `codol_scriptPath` |

Two details are deliberate and easy to trip over. The reader applies the exact
alpha0022 US Army LMG/SMG-B export-filename corrections, and it whitespace-repairs
two specific damaged reference head-alias bodies — but **overlay contents never
receive these repairs**, because an overlay file is something a person wrote and
should be taken literally. A malformed or unreadable source returns `-1` rather
than falling through to a lower root.

## Lowering CoDOL syntax

The CoDOL script corpus uses constructs the retained IW3 scanner and parser do
not accept. Rather than fork the parser, a lowering pass rewrites the source
first:

```c
int CODOL_LowerScriptSyntax(const char *, size_t, char **, size_t *);
const char *CODOL_LowerScriptDiagnostic(unsigned *line);
```

It handles file-scope constant declarations (literals, localized strings,
vectors, and constant expressions over earlier constants of the same file),
`foreach` and `foreach key, value`, and explicit call syntax. When it fails,
`CODOL_LowerScriptDiagnostic` names the construct and the 1-based line.

`CODOL_SetScriptBuiltinResolver` installs the lookup used to redirect `::name`
function pointers at registered builtins — `0` unknown, `1` function, `2`
method.

> `CODOL_ScriptLowerSelfTest()` returns **3** when the C++ containers this pass
> relies on have the layout the linked standard library expects. A wrong answer
> means some translation unit built its container templates under a different
> `#pragma pack` and the linker picked those copies. See [the packing
> hazard](/wiki/source-layout).

## childthread

`childthread` is a CoDOL keyword with no IW3 opcode, and the way it is supported
is worth reading as a pattern for any similar problem.

```c
qboolean Scr_ChildThread_SourceHasKeyword(const char *buf, int len);
qboolean Scr_ChildThread_LowerSource(const char *filename, char *buf, int len,
                                     const char **errMsg, int *errPos);
int      Scr_ChildThread_MapOpcode(int opcode, int callPos, int funcPos);
unsigned Scr_ChildThread_Start(unsigned self, unsigned parentLocalId);
```

1. Every `childthread` keyword is rewritten in place to `thread` **plus
   padding**, preserving every byte offset, line and column, so the recorded
   call spans and the compiler's own source positions share one coordinate
   system. This is the trick that makes the whole thing work without touching
   the parser.
2. The recorded spans are consulted at emit time: `Scr_ChildThread_MapOpcode`
   returns the opcode unchanged unless it is `0x54..0x57` *and* a position
   identifies a recorded childthread call.
3. At runtime, `Scr_ChildThread_Start` allocates the root thread and gives it the
   `endon` subscriptions of the calling frame chain — which is the actual
   semantic difference from a plain `thread`.

One sharp edge is documented in the header: file selection does not survive to
compile time, because `ScriptCompile` calls `Scr_LoadScriptInternal` for
`#include`d files, so a nested file prepares — and selects — itself in the middle
of the including file's compilation.

## Precache

```c
void CODOL_Scr_PrecacheItem(void);   /* precacheItem(<weapon>) */
```

This is the retained IW3 precache with one change: **a weapon name that no
mounted CoDOL zone defines is skipped with a developer trace instead of ending
the level.** The IW4 gametype scripts enumerate IW4 weapon and
attachment-combination names from the stats table, while CoDOL composes
attachments through its marketplace tables — so a straight precache of the IW4
list would kill every match on a name that was never meant to exist here.

## Profiles and loadouts

```c
int CODOL_ReadProfileLoadout(unsigned client, unsigned class_index,
                             codol_profile_loadout_t *out);
int CODOL_LoadoutItemName(int item, const char **name, const char **type, const char **group);
```

A stored loadout is a set of marketplace item IDs, and turning that into a
composed weapon is a small build with several ways to go wrong. The recipe
records all of them:

```c
typedef struct {
    char name[CODOL_COMPOSED_NAME_SIZE];
    const char *base_name; unsigned base_index, flag;
    const char *camo_item; codol_camo_source_t camo_source;
    const char *base_world_model, *base_view_model;
    codol_loadout_part_t parts[CODOL_COMPOSED_ATTACHMENTS];
    unsigned count, skipped_illegal, skipped_duplicate, skipped_full,
             collisions, missing_defaults;
} codol_loadout_recipe_t;
```

The counters exist so a caller can **audit** the build rather than only see the
resulting name, and each part records where it came from — equipped, filler or
declared.

The marketplace numbering follows C1's own construction: attachment indices are
one plus the row's position among the `"wat"` rows of the marketplace table, and
base weapon indices are `0` none, `1` `defaultweapon_mp`, then column 0 of
`mp/coWeaponAttachmentTable.csv` in row order.

## Everything else

The rest of the area is the builtin surface itself, one file per topic:
abilities, action buttons, ambient, appearance, arrays, class, collision,
combat, dvars, entities, effects, fog, glass, ground, HUD (notifications,
timers, waypoints, font scale), laststand, light, magic bullet, mantle, map,
match data, math, menus, minimap, missiles, model animation, notify, offhand,
path nodes, perks, physics, player presentation, player use, precache, radar,
recoil, shellshock, skill, slow motion, sort, sound, sprint, tables, thermal
material, turrets, unlocks, use hints, vehicles, vision, and weapon queries.

Two files — `codol_script_startup_missing.c` and `codol_script_missing_next.c` —
exist to register the names a map script reaches for during startup that had no
native implementation yet. They are shims by intent, not by accident.

Browse every declaration in this area: [script](/reference/areas/script), or
search the [GSC builtin list](/reference/gsc?area=script).
