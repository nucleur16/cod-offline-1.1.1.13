---
slug: subsystem-snapshot
title: Snapshot and frame publication
space: subsystems
summary: src/codol/snapshot — the field tables, the delta codec, the seven player-state sections, events, config strings and the snapshot history.
position: 20
tags: [snapshot]
---

Everything the client sees each frame is produced here. This is the largest
CoDOL area — 64 files, ~12,000 lines — and the one where being wrong is hardest
to diagnose, because a malformed snapshot does not error: the client decodes it
into something plausible and then behaves strangely.

See [A server frame](/wiki/server-frame) for how this area is driven.

## Field tables are wire layouts, not struct offsets

The `*_fields.inc` files are the measured C1 tables. Their header comments say
exactly what they are and what they are not:

```c
/* Measured C1.exe 1.1.1.13 playerStateFields at 0xC38CA8.
 * Format facts from alpha0022/playerstate_fields.py; offsets are wire-layout
 * identifiers, never offsets into retained IW3 engine structures. */
    {0x0000, -97, 0},
    {0x00f0, -75, 0},
```

Each row is `{offset, bits, hint}`:

```c
typedef struct {
    uint16_t offset;   /* wire-layout identifier */
    int16_t  bits;     /* negative means a signed integer of that width */
    uint8_t  hint;     /* special codec selector */
} codol_player_field_t;
```

There are **244 player fields** (`CODOL_PLAYER_FIELDS`). The player state on the
wire is therefore `codol_player_fields_t` — a flat array of 32-bit words — and
not a struct with named members:

```c
typedef struct { uint32_t field[CODOL_PLAYER_FIELDS]; } codol_player_fields_t;
```

That is the single most important thing to understand about this area. Nothing
reinterprets an engine structure as a CoDOL structure; values are moved field by
field through explicit producers.

Entities and clients work the same way, with a type-dependent table for
entities:

```c
const codol_entity_field_t *CODOL_EntityField(unsigned type, unsigned index);
const codol_entity_field_t *CODOL_ClientField(unsigned index);
```

## The delta codec

```c
int CODOL_WriteDeltaValue(codol_msg_t *msg, int bits, uint32_t from, uint32_t to,
                          codol_delta_context_t *context);
int CODOL_ReachDeltaValue(int bits, uint32_t from, uint32_t to,
                          const codol_delta_context_t *context, uint32_t *held);
```

Values are **raw native 32-bit words** — float bits for float fields, integer
bits for integers. The `hint` selects a special codec; a negative default width
carries a signed integer.

`ReachDeltaValue` is the other half of the design and worth dwelling on: it
computes *the value the client will hold after quantization*. The server records
that, not what it was asked to send. Without it, the next delta would be
computed against a value the client does not have.

Positions use a wide codec with a per-frame base:

```c
int CODOL_SetDeltaPositionBase(codol_delta_context_t *, const float center[3]);
```

C1's wide position codec is `FISTP(center + 2^-30)` with round-to-nearest-even.
Non-finite or out-of-range centres reject atomically.

## The seven player-state sections

A player state on the wire is not one blob. `CODOL_WritePlayerFields` writes the
scalar fields — including the read-origin-velocity flag, the local-client flag,
field gaps and a mandatory second pass over `hint 2` fields — and snapshot
assembly must then follow it with seven array and HUD sections carried by
`codol_player_array_updates_t` and `codol_hud_state_t`:

| Section | Carrier |
|---|---|
| stats | `stats_mask`, `stats[4]` |
| clips | `clip_mask`, `clip_index[16]`, `clip_presence[16]`, `clip_ammo[16][2]` |
| weapons | `weapons_mask`, `weapon_index[15]`, `weapon_flags[15]` |
| weapon indices | `weapon_index_mask` |
| reserve ammo | `reserve_mask`, `reserve_type[16]`, `reserve_ammo[16]` |
| objectives | `codol_objective_t[CODOL_OBJECTIVE_SLOTS]` |
| HUD elements | `codol_hud_element_t[2][CODOL_HUD_SLOTS]` |

The two kinds behave differently, and mixing them up is a classic bug:

> Arrays are **updates**: omitted slots retain their base values. HUD states are
> **complete packed arrays**: reducing the count deletes the tail.

## Entities and clients

```c
int CODOL_WriteEntities(codol_msg_t *msg,
                        const codol_entity_state_t *from, size_t from_count,
                        const codol_entity_state_t *to, size_t to_count,
                        codol_delta_context_t *context);
```

Both sides are **sorted complete states**. Missing old entities are removed,
unchanged ones inherited, absent fields explicitly zeroed. A baseline frame is
simply a call with no `from` entities.

Client state carries two CoDOL-only overlays:

- `CODOL_OverlayClientAppearance` replaces the *entire* appearance, clearing
  removed parts. Models must be contiguous; an empty base is legal in C1. The
  inputs are already-published config-string IDs, and the operation is atomic on
  failure. CoDOL composition uses its own model IDs and storage — never the
  six-entry attachment arrays in the retained IW3 ABI.
- `CODOL_OverlayClientAbilities` writes C1 ability-modifier state at
  `clientState +0xc0..128`. Mask IDs use `0x80000000 >> (id & 31)`; values use
  the modifier table's slot mapping and **must already include the table
  defaults and modifier operations** — the overlay does not translate perk
  names, infer defaults or populate gameplay state.

## Events

```c
int CODOL_EventToWire(unsigned engine_event);
int CODOL_EventParameterToWire(unsigned engine_event, uint32_t parameter, uint32_t *out);
int CODOL_ResolveEventParameters(const uint32_t events[4], const uint32_t parameters[4],
                                 uint32_t out[4]);
int CODOL_PlayerFireEventToWire(unsigned engine_event, unsigned hand, unsigned last_shot);
```

The rule stated in the header is absolute: **never copy an IW3 numeric event
ID**. `CODOL_EventToWire` returns `-1` for semantics with no verified mapping,
and `CODOL_ResolveEventParameters` produces **no partial output** when any of the
four queue slots lacks a verified parameter mapping.

Fire events are their own family. The player-state event ring, unlike an entity
event, carries C1's `EV_SV_FIRE_WEAPON` family: `bg_eventNames` names 30
`EV_FIRE_WEAPON`, 31 `EV_FIRE_WEAPON_LASTSHOT` and 43 `EV_SV_FIRE_WEAPON`, and
C1's own producer picks base 43, or the last-shot variant when the clip empties.

## Config strings

```c
int CODOL_SetConfigstring(codol_configstrings_t *state, unsigned index, const char *value);
int CODOL_ImportConstantConfigstrings(codol_configstrings_t *, size_t rows, const char *const *cells);
int CODOL_SetPlayerNames(codol_configstrings_t *state, unsigned count, const char *const *names);
```

Three distinctions live in this file:

- **NULL is not empty.** An explicitly published empty value *clears* a client
  constant; an unset value leaves it inherited.
- **The inherited bank is separate.** `CODOL_ImportConstantConfigstrings` builds
  a bank of constants the client already owns. Serialising that bank as
  overrides would resend values the client never needed.
- **Player names are replaced as a block**, atomically, bounded to the client's
  63-byte storage; a NULL entry clears a slot.

## History

```c
int CODOL_PrepareHistory(codol_history_t *, const codol_snapshot_state_t *requested,
                         uint32_t base_sequence, int read_origin_velocity,
                         const codol_delta_context_t *);
int CODOL_FinishHistory(codol_history_t *, uint32_t sequence, int accepted);
const codol_snapshot_state_t *CODOL_FindHistoryBase(const codol_history_t *,
                                                    uint32_t acknowledged,
                                                    uint32_t next_sequence);
```

The history stores **quantized client-held values**, never unquantized engine
input, and commits only after transport acceptance. There is exactly one pending
snapshot per history and it is invisible to ACK lookup until finished;
`accepted = 0` discards it. Sequences must strictly increase.

`CODOL_PrepareBaselineHistory` prepares a `SNAPFLAG_BASELINE` frame whose reached
state is copied into the separately pinned no-delta baseline — again, only if
the transport accepts it.

Browse every declaration in this area: [snapshot](/reference/areas/snapshot).
