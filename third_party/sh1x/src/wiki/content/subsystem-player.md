---
slug: subsystem-player
title: Player input and movement
space: subsystems
summary: src/codol/player — usercmd decode, the C1 pmove core, sprint and recoil overrides, perks and abilities, animation tables and character selection.
position: 30
tags: [player]
---

This area covers the path from a decoded user command to the movement,
animation and ability state that the [snapshot](/wiki/subsystem-snapshot)
publishes. It is where CoDOL and retained IW3 code are most tightly interleaved,
so most files here exist to feed a retained assembly listing the right value.

## The user command

```c
typedef struct {
    uint32_t serverTime, buttons;
    uint16_t angles[3], weapon, offHandIndex, altModeWeapon, meleeChargeYaw;
    uint8_t  direction, meleeChargeDist;
    uint8_t  selectedLocation[2], selectedLocationAngle, remoteAngles[2];
} codol_usercmd_t;
```

```c
int      CODOL_ReadUsercmd(codol_msg_t *m, uint32_t key,
                           const codol_usercmd_t *from, codol_usercmd_t *to);
uint32_t CODOL_UsercmdKey(uint32_t ack, uint32_t checksum,
                          const uint8_t *command, size_t command_length);
void     CODOL_FilterUsercmd(const codol_usercmd_t *wire, codol_usercmd_t *game);
```

Decoding is atomic: a failure publishes no partially decoded command. The key
derives from the acknowledged sequence, a checksum and the last reliable
command, which is why a desynchronised reliable stream shows up as garbage
input rather than as a protocol error.

`CODOL_FilterUsercmd` produces the *gameplay* command and leaves the wire
command intact, because prediction comparisons need the original.

## The C1 movement core

Movement is being cut over from retained IW3 pmove to a native C1 core. The
seam is one function:

```c
int CODOL_PmoveC1Try(struct pmove_t *pm);
```

Called from `ClientThink`. It consumes the structurally bound full C1 command,
and **returns zero when the native core is intentionally inactive**, in which
case the caller must preserve the current live path. That is the whole gate:

> Component entry points exist, but do not splice them into the retained Pmove
> path. `CODOL_PmoveC1Try` gates the complete core atomically.

Coverage is observable at runtime through `CODOL_PmoveC1Implemented()` and
`CODOL_PmoveC1Required()`.

Inside the core, movement mode selection has a strict order:

```c
typedef enum {
    CODOL_PM_C1_MOVE_NORMAL,
    CODOL_PM_C1_MOVE_MANTLE,
    CODOL_PM_C1_MOVE_LADDER,
} codol_pmove_c1_mode_t;

codol_pmove_c1_mode_t CODOL_PM_C1SelectMove(codol_pmove_c1_t *move);
int CODOL_PM_C1MantleOwnsFrame(struct pmove_t *pm, struct pml_t *pml);
```

C1 gives an active mantle **exclusive ownership of the movement frame**, and
mantle ownership is decided before ladder acquisition runs. Getting that order
wrong produces the classic symptom of a player who can grab a ladder out of a
mantle.

The individual steps live in `codol_pmove_c1_ground.h` and
`codol_pmove_c1_mantle.h`: `GroundTraceC1`, `CheckJumpC1`, `NormalMoveC1`,
`AirMoveC1`, `LadderMoveC1`, `MeleeChargeMoveC1`, `FreeMoveC1`, `FootstepsC1`,
`CheckMantleC1`, `MantleMoveC1`.

`CODOL_PM_UpdateViewheightC1` is exported separately because linked, vehicle and
later native movement paths all need that same state producer.

## Feeding the retained code

Several files exist purely to replace a constant or a computation inside a
retained assembly listing. They are the clearest example of the hook pattern
described in [Retained x86 assembly](/wiki/retained-assembly).

**Sprint.** `CODOL_SprintBaseDurationMs` replaces the `player_sprintTime * 1000`
base inside `BG_GetMaxSprintTime` in `_bg_misc.asm`; the weapon
`sprintDurationScale`, the perk multiplier and the 16383 ms clamp still happen
in the retained code afterwards. `CODOL_SprintRechargePauseMs` does the same for
the recharge pause. `CODOL_OverlayEngineSprint` then publishes the retained
sprint state through C1 player fields `0x238`/`0x23c`/`0x240`/`0x244`/`0x248`,
so the client's own sprint meter and prediction observe the server's duration.

**Recoil.** `CODOL_PlayerRecoilScale` is read by `BG_WeaponFireRecoil` in
retained code and returns `percent / 100` only while the playerState carries the
armed flag *and* a magnitude was recorded for it; an armed state with no
recorded magnitude is left unscaled rather than silently guessed.
`CODOL_OverlayEngineRecoilScale` publishes the armed bit and percentage through
C1 fields `0x344` bit `0x400` and `0x3330`.

Both reset with the spawn memset the engines already perform, so scripts
reapply them per spawn.

## Perks and abilities are two different things

This trips people up, because IW3 had one concept and C1 has two.

**Perks** are IDs in slots (`codol_perk_state_t`), read from `coPerkStatsTable`,
and published through eight measured 8-bit playerState netfields at
`0x4b0..0x4cc`. `CODOL_OverlayPlayerPerks` **rejects** values outside the wire
domain rather than truncating an ID into some other perk.

**Abilities** are C1 ability-modifier state (`codol_client_abilities_t`): a
bitmask plus a table of float values, read from `coAbilityModifiersTable`.

```c
int CODOL_SetAbility(codol_client_abilities_t *, const codol_ability_definition_t *);
int CODOL_SetAbilityValue(codol_client_abilities_t *, const codol_ability_definition_t *, float);
int CODOL_UnsetAbility(codol_client_abilities_t *, const codol_ability_definition_t *);
int CODOL_GetActiveAbilityValue(const codol_client_abilities_t *, const codol_engine_table_t *,
                                unsigned id, float *value);
```

Set initialises an available default and then sets the MSB-first mask bit;
`SetValue` writes the value directly, independently of that bit. **Unset and
Clear change only masks — stored values survive.** These are deliberately VM
level operations, not the higher-level perk accumulation policy.

Table lookups follow C1's own rule: *last* matching row wins, ASCII
case-insensitive, and malformed numeric fields are rejected rather than accepted
as an out-of-range index.

## Animation tables

`codol_animation_moves.inc`, `codol_animation_conditions.inc` and
`codol_animation_events.inc` are generated from measured C1 tables (the header
carries the source SHA-256) and are included by **both** the C side and
`_bg_animation_mp.asm`. The C API around them manages script slots per
`(state, movement, canned)` triple and per event, and pushes condition values
into the retained animation system:

```c
void CODOL_SetAnimConditionValue(int client, int condition, uint64_t value);
void CODOL_UpdateAnimConditionsFromPlayerState(int client, const struct playerState_s *ps);
```

## Character selection

A CoDOL player's appearance is composed, not chosen:

```c
int CODOL_SelectClientCharacter(unsigned client, const char *faction, unsigned gender,
                                const char *camo, unsigned rig_selector);
int CODOL_SelectClientCharacterForWeapon(unsigned client, const char *faction, unsigned gender,
                                         const char *camo, const char *weapon);
```

Selection reads the client's own uploaded public profile through the mounted
playerdata schema, composes it for the visual context, **drops parts whose
XModel is not mounted**, registers the survivors as native model config strings,
and stores the resulting indices. Ordering matters: element 0 is the trousers
when the player has any, as the engine's own column list requires, then the
named groups in measured order, then eyewear, then anything else that resolved.

The weapon variant picks the chest-rig selector from the weapon's class the way
C1 does — load the weapon, read its class at `+0x344`, map it through
`CODOL_ChestRigSelector`; an unknown weapon or class selects rig 0.

`CODOL_PublishStoredClientCharacter` is a **live** producer. Captured snapshots
never consult the live character store, which is what keeps an archived or
spectator-followed frame self-consistent.

## Sentients

`codol_sentient.cpp` is the C1 sentient pool — the registry of things that can
be targeted, including actors and turret owners. Slot exhaustion is explicit
rather than silent: an actor that finds every slot taken keeps `actorSlot == -1`
and publishes nothing, and releasing a slot immediately hands it to the waiting
actor with the lowest entity number, so the published set is deterministic.

Browse every declaration in this area: [player](/reference/areas/player).
