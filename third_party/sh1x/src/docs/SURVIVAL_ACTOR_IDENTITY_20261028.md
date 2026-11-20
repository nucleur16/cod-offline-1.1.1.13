# Survival actor targeting and weapon drops, 2026-10-28

Source and portable regressions pass. C1 visual/pickup verification remains
pending. The parent builds and deploys the final integrated changes.

## Actor team is a real C1 target-classification input

These instructions were read from the installed 1.1.1.13 C1.exe using
`tools/c1_disasm.py`; offsets are C1 virtual addresses.

- `42a718` copies actor snapshot record `+08` to actorInfo `+14`.
- `42a670` resolves an actor's composition at `cg+168514+slot*29c`.
- `433770` selects the target identity. Its actor arm at `433793` reads
  `[cg+(index-19)*29c+168528]`, exactly that actorInfo `+14`, into the output
  supplied in EDI. Its player arm at `4337d3` reads
  `[cg+index*b50+15ae70]`, clientInfo `+4c`, into the same output.
- `435613` calls that helper with EDI pointing to `[ebp-14]`. `43561b`
  reads the viewing client's clientInfo `+4c`; `43561e` compares the target
  value to it. A mismatch branches to `435633` and produces enemy=1.
  Equal nonzero teams produce enemy=0. Equal zero teams additionally compare
  owner/client identity. `43563b` saves the result into target record `+40`.

Thus the three-bit actor record field is the actual team consumed by C1's
enemy classifier, rather than a guess from its width. The producer previously
left this canonical field zero while storing actorTeam only in server metadata
for alternate dog models.

`CODOL_Sentient_TeamForClient` maps TEAM_RED to axis=1 and TEAM_BLUE to allies=2.
`CODOL_OverlayEngineClientState` publishes the same 1/2 values in C1 client
state `+4` (C1 team-name getter `50e080`). `ProduceActorStates` now
publishes the actual actor sentient team at `+08`, rejects values outside the
native 0..6 vocabulary, and archives that canonical byte image with its frame.
The free-player sentient class 4 is used for turret masks; the player's C1
client team remains 0. Native crosshair classification preserves that distinction.

No client timing patch or friendly-fire bit substitution is involved. The
older claim that the enemy color branch was unreachable was superseded by the
packed-solid/virtualized prediction audit in IMPLEMENTATION_PASS.md.

## Held weapon drop ordering

Survival's custom MP actor callback omitted the soldier animscript's weapon
drop. Its callback now invokes `codol_actorDropWeapon` before returning to the
MP reward callback, excluding dogs explicitly. The native primitive consumes
the actual entity-state weapon and uses retained `Drop_Weapon` for the item,
tag placement, native ammunition, physics, owner and drop-ring cleanup. It
detaches the matching hand model and clears the held weapon only on success.
Primary/exclusive guns and projectile launchers are eligible; equipment is not.

`ActorDie` synchronously executes the killed callback while the entity is
still ET_ACTOR. `ActorRunFrameBody` changes it to a corpse only after the death
animscript completes. Therefore the drop receiver remains an actor when called.
The retained clip-only drop branch also now guards its playerState read with
`ent->client`, matching T5 ThrowDownWeapon's non-player handling.

The Survival type table includes m79 as a primary and RPG in alt-weapon columns.
The drop primitive supports projectile primaries. The existing actor arming/
shooting helper still accepts bullet definitions only; projectile AI fire is a
separate pre-existing implementation limit.

## Validation

- `test_actor_team`: real producer and archive/capture paths, live team
  change 1->2, old frame retaining 1, invalid team rejection, wrong archive time.
- `test_lookat`: hostile/friendly actor sentients, free-player class
  distinction, dead/hidden/neutral/corpse rejection, player and target-loss paths.
- `test_actor_drop`: native drop call and tag, failed spawn preservation,
  exactly-once consumption, projectile primary, equipment/nonweapon rejection.
- Survival Favela, MP war and campaign Favela script resolver gates passed
  against source and the final integrated server binary.
- The parent built the final Windows server EXE and host DLL, including team
  publication and launcher drops, and deployed matching binaries and scripts.
  Live C1 enemy crosshair and pickup verification remains pending.
