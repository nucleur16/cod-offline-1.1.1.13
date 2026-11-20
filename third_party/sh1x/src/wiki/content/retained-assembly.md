---
slug: retained-assembly
title: Retained x86 assembly
space: architecture
summary: Why thousands of functions in this server have no C source, how CoDOL hooks into them, and how to read one.
position: 40
tags: [asmsource, architecture]
---

Most of the engine functions that predate the CoD4x fork were never rewritten in
C. They are the original game binary's x86, disassembled and reassembled under
their own names in `src/asmsource/`. The server links that object code directly.

By the numbers, from the last index run:

| | |
|---|---|
| Assembly files | 111 |
| Lines of assembly | ~270,000 |
| Functions whose only implementation is assembly | 3,512 |
| GSC builtins backed by assembly | 398 |

That last row is the one that surprises people: when a map script calls
`getent`, `spawnplane` or `player adsButtonPressed()`, the code that runs is
reassembled disassembly, not C.

## Why it is like this

CoD4 was never open source. CoD4x is a server built *around* the shipped binary:
the parts that had to change were rewritten, and the parts that only had to keep
working were lifted out wholesale. Assembly that came from the shipped binary
has one enormous advantage over a reimplementation — its behaviour is exactly
the behaviour that shipped, including the details nobody has documented.

The disassembly pool it was drawn from is still in the tree at
`tools/cod4_dasm/` (roughly 1.8 million lines). The wiki does not index it,
because it is not server source; `src/asmsource/` is the subset that is actually
built.

## How a listing is put together

Each file is NASM, and starts by declaring what it calls:

```nasm
%include "src/codol/player/codol_player_eflags.inc"

;Imports of bg_pmove:
	extern AngleDelta
	extern BG_CheckProne
	extern stopspeed
%ifdef CODOL_SERVER
	extern CODOL_SprintRechargePauseMs
	extern CODOL_GetSprintLeft
	extern CODOL_AnimationRecords
	extern CODOL_PlayerLinkClampView
%endif
```

Functions are plain labels. Labels of the form `Name_12` are basic-block targets
from the original disassembly, not separate functions — the wiki's index skips
them, so `PM_Friction_50` never shows up as something you can call:

```nasm
PM_Friction:
	...
PM_Friction_50:        ; a jump target inside PM_Friction
	...
PM_MeleeChargeUpdate:
	...
```

The build rule is unremarkable:

```make
$(OBJ_DIR)/%.o: $(SRC_DIR)/asmsource/%.asm
	$(NASM) $(NASMFLAGS) $< -o $@
```

with `NASMFLAGS=-f win -dWin32 --prefix _` on Windows, `-f elf` on Linux, and
`-dCODOL_SERVER` added for the CoDOL target.

## How CoDOL changes behaviour inside them

Two mechanisms, and both are visible in the listing.

**Inserted calls.** Where CoDOL needs different behaviour part-way through a
retained function, a `call CODOL_*` is inserted into the listing and the
implementation is an ordinary C function in one of the `src/codol/` areas. The
sprint recharge pause above is exactly this: `_bg_pmove.asm` calls
`CODOL_SprintRechargePauseMs`, which lives in
`src/codol/player/codol_sprint_state.c`.

**Included constant tables.** Where the difference is a constant rather than
logic, the value moves into a NASM include shared by both halves:

```nasm
; playerState eFlags are native C1 state in a CoDOL build.  C1 reserves
; 0x100/0x200 for Pmove's input/firing markers and uses 0x400/0x800 for the
; two turret modes.  Retained IW3 code must consume the C1 turret pair too.
%ifdef CODOL_SERVER
%define PLAYER_EFLAG_TURRET_MASK 0x0c00
%else
%define PLAYER_EFLAG_TURRET_MASK 0x0300
%endif
```

That file is `src/codol/player/codol_player_eflags.inc`. The animation tables
work the same way: `codol_animation_moves.inc` and its siblings are generated
from measured C1 tables and included by both `_bg_animation_mp.asm` and the C
side.

The makefile records the dependency so a regenerated table rebuilds the listing:

```make
$(OBJ_DIR)/_bg_animation_mp.o: src/codol/player/codol_animation_moves.inc \
                               src/codol/player/codol_animation_conditions.inc \
                               src/codol/player/codol_animation_events.inc
```

## Reading the wiki with this in mind

Every callable in the reference carries a badge saying how it is implemented:

- **C** / **C++** — there is source to read, and the symbol page links to it.
- **x86 asm** — the only definition found is an assembly label. The symbol page
  says so explicitly and shows the first few instructions.
- **no definition found** — declared, but the lexical index found no definition
  anywhere. That usually means a macro generates it, it lives in a vendored
  library, or it is genuinely absent. It is a prompt to look, not a verdict.

You can list them: [every function implemented in
assembly](/reference/symbols?implementation=x86-asm), or [every GSC builtin
backed by assembly](/reference/gsc?implementation=x86-asm).

## Practical consequences

- **You cannot fix a bug in a retained function by editing C.** Either change
  the listing, or insert a `CODOL_*` hook at the right point and implement the
  new behaviour in C.
- **Do not equate field offsets across games.** IW3, IW4, T5 and C1 all lay
  their structures out differently. A retained listing hard-codes IW3 offsets;
  CoDOL state that it has to read must be handed to it explicitly, not implied.
- **Calling convention is cdecl with the `_` prefix on Windows.** A C function
  the assembly calls must be a plain `extern "C"` symbol with no name mangling.
- **Tests can reach it.** Several `make test-codol` targets assemble one listing
  and link it against a C harness, which is the only practical way to get the
  assembly half under test.
