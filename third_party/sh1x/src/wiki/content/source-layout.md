---
slug: source-layout
title: How the source is laid out
space: start
summary: Where each kind of code lives, the include and naming rules, and what you have to do to add a file or an area.
position: 20
tags: [overview]
---

Every CoDOL-specific source file lives under `src/codol/<area>/`. The retained
CoD4x engine stays flat in `src/`, and its disassembled functions in
`src/asmsource/`. File names were not changed during the 2026-09-09
restructure, so `grep` for a symbol still lands in the same file it always did.

## The map

| Path | What lives there |
|---|---|
| `src/*.c`, `*.cpp`, `*.h` | Retained CoD4x engine: frame loop, netchan, filesystem, cvars, the GSC virtual machine and compiler, game logic, plugin host. CoDOL changes to it are guarded by `#ifdef CODOL_SERVER`. |
| `src/asmsource/*.asm` | Retained IW3 functions kept as x86 — pmove, mantle, animation, weapons, game entities. CoDOL calls appear as `call CODOL_*`. |
| `src/codol/net/` | Client transport: handshake, ECDH, association, connect, netchan framing, wire codec, pure check. |
| `src/codol/snapshot/` | Everything published to the client each frame: frame builder, snapshot writer and history, delta codec, field tables, engine-state overlays, events, config strings, HUD and radar. |
| `src/codol/player/` | Input and movement: usercmd decode, dive, sprint, recoil, perks and abilities, stance and animation tables, spectate defaults, use hints. |
| `src/codol/weapon/` | Weapon composition from attachments, registry and loader, weapon state machine, locks and missiles, turret heat, weapon config strings. |
| `src/codol/vehicle/` | Vehicle assets, native simulation, spawner. |
| `src/codol/script/` | GSC builtins added for CoDOL scripts, the syntax-lowering pass, precache, profile and loadout access, menus, startup shims. |
| `src/codol/assets/` | Fastfile and IWD reading, asset registry and streaming, xmodel/xsurface/xanim/material/sound/effect natives, character composition, string tables, map custom data. |
| `src/codol/collision/` | BSP collision — brushes, leaves, terrain, static models, materials — plus world planes, physics, dynamic entities and triggers. |
| `src/codol/stats/` | Secure profile, experience and unlock rules, skill rating, match data and its wire format, scoreboard, structured data schema. |
| `scripts/codol/maps/mp/...` | Native GSC adaptations that override the alpha0022 `gsc_overlay` and the IW4 vendor scripts. |
| `bin/START-CODOL-SERVER.cmd` | The launcher. Log lands at `bin/home/main/qconsole.log`. |
| `tests/test_codol_*.c\|.cpp\|.py` | Unit tests, run by `make test-codol`. |
| `tools/` | Offline helpers, plus `tools/cod4_dasm/` — the full CoD4 disassembly the retained assembly was drawn from. |
| `docs/CODOL_*.md` | Per-subsystem design notes and the running implementation log. |

## The include rule

Every area directory is on the include path, so an include never carries a
directory prefix:

```c
#include "codol_msg.h"        /* correct, from anywhere in the tree */
#include "net/codol_msg.h"    /* wrong, and will not compile */
```

The makefile builds that path itself:

```make
CODOL_DIR=$(SRC_DIR)/codol
CODOL_AREAS=net snapshot player weapon vehicle script assets collision stats
CODOL_DIRS=$(addprefix $(CODOL_DIR)/,$(CODOL_AREAS))
CODOL_INC=-I$(SRC_DIR) $(addprefix -I,$(CODOL_DIRS))
```

Two consequences follow from that, and both bite if you forget them:

- **File names must be unique across every area**, because objects land flat in
  `$(OBJ_DIR)`. Two files called `codol_state.c` in two areas would collide.
- **Adding a file needs nothing else.** Drop it in the area it belongs to; the
  glob picks it up. Adding a whole area means creating the directory and
  appending its name to `CODOL_AREAS`.

## Naming inside an area

The suffix tells you which layer a file is:

| Pattern | Layer |
|---|---|
| `codol_<topic>.c` | Engine-facing logic — the part other areas call. |
| `codol_<topic>_native.c` | Parses the raw asset or BSP format. |
| `codol_<topic>_wire.c` | The client encoding. |
| `codol_sv_<topic>.c` | Server-frame glue: what runs each frame. |
| `codol_script_<topic>.c` | The GSC builtin surface for that topic. |
| `codol_<topic>.inc` | A generated table. Some are C fragments, some are NASM — check the comment header. |

## Reading the log

Developer traces are prefixed `CoDOL <topic>:` so a pasted console log can be
filtered by subsystem: `CoDOL pacing`, `CoDOL rov`, `CoDOL mantle`,
`CoDOL spawn`, `CoDOL field audit`, `CoDOL reliable ->`.

## A packing hazard worth knowing

`src/server.h` and its neighbours set `#pragma pack(1)` without popping it,
which is correct for engine structures that must match the original binary's
layout but wrong for anything else. CoDOL headers therefore wrap their own
declarations:

```c
/* Semantic state must not inherit engine-header packing. */
#pragma pack(push, 8)
typedef struct {
    uint32_t sequence;
    const codol_player_fields_t *player;
    /* ... */
} codol_snapshot_state_t;
#pragma pack(pop)
```

The same rule applies to C++ translation units: include the standard library
*before* any project header, or the engine's packing corrupts the layout of
every container in the program.
