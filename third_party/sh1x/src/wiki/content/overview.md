---
slug: overview
title: What this server is
space: start
summary: A CoD4x dedicated server rebuilt to speak Call of Duty Online, and how to read a codebase that is half C and half original game binary.
position: 10
tags: [overview]
---

The **sh1x server** is a native dedicated server for the Call of Duty Online closed
alpha. It began as [CoD4x](https://github.com/callofduty4x/CoD4x_Server), the
long-running Call of Duty 4 dedicated-server fork, and is being rewritten where
it has to be so that an unmodified **C1 1.1.1.13** client can connect to it,
load a map and play.

That lineage is the single most useful fact about the codebase, because it
explains the shape of everything else:

- CoD4 and Call of Duty Online (CoDOL) share an engine ancestor, so a large part
  of the original server — the frame loop, the filesystem, the GSC virtual
  machine, entity bookkeeping — still does the right thing.
- Everything the client actually sees on the wire is different. The handshake,
  the encryption, the snapshot layout, the player-state field table, the weapon
  model and the asset formats are C1's, not IW3's, and they had to be written
  from scratch.

So the tree has two halves. The retained CoD4x engine sits flat in `src/`; the
CoDOL work sits under `src/codol/<area>/`. See [How the source is laid
out](/wiki/source-layout) for the map.

## What is not CoD4x any more

Under this project's standing instructions, CoD4x is a starting point and not a
compatibility constraint: native subsystems are rewritten freely where CoDOL
needs different behaviour. Concretely, the following are CoDOL implementations
that share little more than a name with their CoD4 ancestors.

| Subsystem | Why it was rewritten |
|---|---|
| [Transport](/wiki/subsystem-net) | C1 does not speak the IW3 netchan. It performs a DTLS-shaped handshake with ECDH key agreement, then carries encrypted records with their own sequencing. |
| [Snapshots](/wiki/subsystem-snapshot) | The player-state and entity-state field tables are C1's measured wire layouts. Field counts, bit widths and ordering all differ. |
| [Weapons](/wiki/subsystem-weapon) | A CoDOL weapon is *composed* from a base definition plus attachments at runtime, rather than shipped as one finished definition. |
| [Player appearance](/wiki/subsystem-assets) | A character is composed from playerdata parts — hands, body, vest, headgear, boots — not chosen from a fixed model list. |
| [Profiles and progression](/wiki/subsystem-stats) | Experience, challenges, unlocks, skill rating and the structured match-data upload are CoDOL concepts with no IW3 equivalent. |

## Half of this code is not C

CoD4x was never a full source port. Most engine functions from before the fork
were never rewritten: they are the shipped game binary's x86, disassembled and
reassembled under their own names in `src/asmsource/`. There are **111 assembly
files, about 270,000 lines**, and the index resolves **3,512 functions** whose
only implementation is that assembly — including **398 of the GSC builtins** a
map script can call.

This matters every time you go looking for how something works:

- If `PM_GroundTrace` has no C source, that is not a missing file. It is
  `src/asmsource/_bg_pmove.asm`, and its behaviour is by construction exactly
  the behaviour that shipped.
- CoDOL changes reach into those functions as `call CODOL_*` inserted into the
  listing, guarded by `%ifdef CODOL_SERVER`. The C side of such a hook is an
  ordinary function in one of the `src/codol/` areas.
- The wiki labels every callable with how it is implemented — `C`, `C++` or
  `x86 asm` — so you never have to guess which case you are in. See [Retained
  x86 assembly](/wiki/retained-assembly).

## What state it is in

Work in progress, and the wiki tries to be honest about that. The reference
index reports what the source *says*: that a function is declared, where it is
defined, what its struct members are. It cannot tell you whether the behaviour
is correct against a real C1 client, and neither can a page that was written
before the last change. Where a subsystem page makes a behavioural claim, it
says what that claim rests on.

The project's own running notes live in `docs/` — `CODOL_IMPLEMENTATION_PASS.md`
is the log, `CODOL_PARITY_PLAN.md` the gap list, `CODOL_PLAYER_ACTION_AUDIT.md`
the per-action status. Those are the authority on what has been verified
against a live client; this wiki is the authority on how the code is arranged.

## Where to go next

- [How the source is laid out](/wiki/source-layout) — the directory map and the
  rules that keep it buildable.
- [Building and running](/wiki/build-and-run) — the cross-build command, the
  launcher, and where the log goes.
- [A server frame](/wiki/server-frame) — what happens twenty times a second.
- [The reference index](/wiki/reading-the-reference) — how the generated half of
  this wiki is built and what it can and cannot tell you.
