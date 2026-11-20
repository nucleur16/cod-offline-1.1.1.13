---
slug: subsystem-weapon
title: Weapons
space: subsystems
summary: src/codol/weapon — how a CoDOL weapon is composed from attachments, the registry that publishes it, the two-handed state machine, locks and missiles.
position: 40
tags: [weapon]
---

A CoD4 weapon is a finished `WeaponDef` shipped in a fastfile. A **CoDOL weapon
is composed at runtime**: a base definition plus a list of attachment items,
assembled into a canonical definition that is then registered and published.
That single difference shapes this entire area.

## Composed weapon names

A composed weapon is identified by a name with a specific grammar, parsed into:

```c
typedef struct {
    char     tag[CODOL_COMPOSED_TAG_SIZE];
    char     canonical[CODOL_COMPOSED_NAME_SIZE];
    unsigned base, flag, count;
    unsigned items[CODOL_COMPOSED_ATTACHMENTS];
    unsigned variants[CODOL_COMPOSED_ATTACHMENTS];
} codol_composed_weapon_t;
```

`CODOL_ParseComposedWeaponName` is strict on purpose: a missing `*`, an empty or
oversized tag, a zero base, a byte-range violation, trailing garbage, or **a
pair without its variant** are all rejected rather than defaulted, and pairs
beyond the array bound are rejected instead of silently dropped.

`CODOL_CanonicalComposedWeaponName` renders the canonical spelling — `*`, tag,
then one space before every number, in the order the name gave them. C1 accepts
other spacings of the same fields, so this module keys its definitions by the
canonical rendering to make sure the variants resolve to one weapon and one
published slot. **Item order is never rewritten**, because C1 stores order.

## Composition

Attachments are not applied to a native `WeaponDef`. They are applied to an
owned copy of the weapon's **serialized CoDOL source image**:

> C1's entry offsets address the CoDOL source layout, whose fields do not
> correspond to the native `WeaponDef` layout.

```c
int  CODOL_BeginWeaponSourceImage(const char *, codol_weapon_source_image_t *);
int  CODOL_ReadWeaponAttachment(const char *, codol_weapon_attachment_t *);
int  CODOL_ApplyWeaponAttachments(codol_weapon_source_image_t *,
                                  const codol_weapon_attachment_t *const *, unsigned);
int  CODOL_WeaponSourceView(const codol_weapon_source_image_t *, codol_mounted_weapon_t *);
void CODOL_FreeWeaponSourceImage(codol_weapon_source_image_t *);
```

The image deep-copies the mounted bytes and resolves every pointer slot and all
32 hide tags. It owns its bytes and every override it later takes, but the
borrowed names it starts with live as long as the defining archive — **an image
must not outlive the mount it was taken from**.

Application follows C1's order exactly (`BG_BuildCustomWeaponDef`): every entry
is offered to the field filter first, which applies scalar, raw and hide-tag
entries immediately and **defers the string operators**; the deferred entries
are then sorted with C1's comparator and applied afterwards. Order matters
because string operators can append to what earlier ones produced.

`CODOL_WeaponSourceView` then presents the patched image to the existing native
converters as if it were a mounted weapon, so an overridden internal name is
what the converters see.

## From image to native definition

`codol_weapon_native.h` holds the readers that fill a `WeaponDef` from a mounted
(or composed) source, one concern at a time:

| Function | What it maps |
|---|---|
| `CODOL_ApplyNativeWeaponTimings` | Fire, detonation, ADS transition, reload/change and sprint timings, plus variants with no IW3 field (`codol_weapon_timing_extensions_t`). |
| `CODOL_ApplyNativeWeaponAmmo` | Starting ammo, reserve, clip capacity, pellet count, reload transfer caps, clip-only / no-partial / segmented flags. |
| `CODOL_ApplyNativeWeaponDamage` | Base bullet damage and distance falloff. |
| `CODOL_ApplyNativeWeaponLocationDamage` | Hit-location table — IW3 order for the first 19 slots, with CoDOL's twentieth shield multiplier kept separately. |
| `CODOL_ApplyNativeWeaponMelee` | Base melee damage and normal/charge timing. |
| `CODOL_ApplyNativeWeaponLockRequirement` | The C1 `+0xbbe` byte that gates firing on a finalized lock. |

Every one of them is atomic on failure and **fabricates no defaults**. None of
them constructs a complete `WeaponDef` on its own; the core that results is
`codol_native_weapon_core_t`, which carries the definition plus the CoDOL-only
fields — missile stickiness, shield multiplier, `inherits_perks`, `dual_wield`,
`ads_reload`, `dual_mag`, `hold_pin`, `silenced`, the partner name.

Ammo, shared-cap and clip indices are **runtime registrations**, never copied
from the serialized image.

## The registry

```c
int CODOL_RegisterWeaponKeys(codol_weapon_registry_t *, unsigned wire, const char *name,
                             const char *ammo, const char *clip,
                             unsigned deduplicate, unsigned no_clip, int base);
int CODOL_PublishComposedWeapon(const char *, const char *, unsigned *slot);
```

`wire` must be the **next sequential slot**; `base` is `-1` for a stock weapon
or an earlier slot for a `*` composed definition. Failure leaves the entire
registry unchanged.

Publication is the point where a composed weapon becomes real: it registers the
canonical definition in the retained native weapon table, publishes the adjusted
name and partners, and rebuilds the engine and wire mappings. **Config string
and registry changes commit together**, and native registration is monotonic in
the same way `BG_GetWeaponIndexForName` is.

`CODOL_BroadcastWeaponConfigstrings` then sends newly committed names to primed
recipients in registry order, and `CODOL_RefreshServerWeaponRegistry` rebuilds
from the exact override/inherited weapon names *before* gamestate — a failed
refresh invalidates the previously exposed registry rather than leaving a stale
one exposed.

## The weapon state machine

C1 runs its whole weapon machine **once per hand**, so the state is per-hand:

```c
typedef struct {
    unsigned weapon;
    int state, animation, time, delay, restrict_time;
    unsigned rechamber, shot_count, trigger_held, shot_pending,
             delay_expired, fresh_pull, reload_request, brass_pending;
} codol_weapon_hand_t;
```

`codol_weapon_state_t` holds both hands plus the primary weapon, dual-wield
flag, offhand primary/secondary classes, the engine's own state and animation,
owned-weapon bitmaps, spin-up charge and second-barrel state.

The engine-state side records which hand produced each fire event
(`codol_engine_shot_record_t`: hand, last_shot, known), because the player-state
event ring has to name the right `EV_SV_FIRE_WEAPON` variant — see
[events](/wiki/subsystem-snapshot).

## Weapon input has an acceptance point

Weapon input is staged and then promoted, so that a command the guard turns away
never reaches the weapon machine:

```c
void CODOL_StageNetworkCommandInput(unsigned client, const codol_usercmd_t *command);
void CODOL_BindExecutedWeaponInput(unsigned client, const usercmd_t *native);
void CODOL_DiscardStagedWeaponInput(unsigned client);
```

`Bind` promotes whatever was staged alongside the native command that just
became `cl->lastUsercmd`. A client with nothing staged is a bot or an internal
producer, **never a stale network command**, so the staging is consumed rather
than left to age. `Discard` is the other half: input staged for a rejected
command is dropped rather than promoted by the next command that does get
through.

`CODOL_BeginSyntheticWeaponInput` / `EndSyntheticWeaponInput` preserve accepted
and pending input around a direct synthetic think.

## Locks and missiles

```c
int CODOL_SetWeaponLock(codol_weapon_lock_t *, unsigned entity, const float *position,
                        int finalize, const float *missile_offset, int direct_fire);
int CODOL_OverlayWeaponLock(codol_player_fields_t *, const codol_weapon_lock_t *);
```

A non-NULL `position` selects a position target, otherwise an entity (0..2046).
`finalize` adds the locked and fire-mode flags. Published through C1 fields
`0x48c`/`0x490`/`0x494`/`0x498`/`0x49c` — **the missile offset is deliberately
server-only**.

The read accessor never returns NULL: an untracked state reads as unlocked.
`CODOL_ResetClientWeaponLock` clears only the live half and only the lock — the
recoil magnitude sharing that block belongs to another subsystem, and captured
snapshots keep the lock they were taken with.

Missile stickiness uses **C1's six-value enum**, and the header says plainly:
never interpret these with IW3's four-value enum.

```c
typedef enum {
    CODOL_STICK_NONE, CODOL_STICK_ALL, CODOL_STICK_ALL_NO_SENTIENTS,
    CODOL_STICK_GROUND, CODOL_STICK_GROUND_WITH_YAW, CODOL_STICK_FLESH,
} codol_missile_stickiness_mode_t;
```

`CODOL_PredictMissileStop` is named for what it is: C1's **AI trajectory
prediction helper**, not authoritative collision. It does not mutate
trajectories.

Remote-controlled missiles keep the controls mode distinct from owner-handle
lifetime, and `CODOL_RemoteMissileClientThink` must be passed the time delta
`ClientThink_real` computed *before* pmove advanced `ps->commandTime`, with the
same command's accepted-input binding still active.

Browse every declaration in this area: [weapon](/reference/areas/weapon).
