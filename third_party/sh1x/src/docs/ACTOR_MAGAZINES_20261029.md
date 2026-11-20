# Actor magazine ammunition, 2026-10-29

The user requires dropped actor weapons to retain the actual remaining loaded
magazine and give no reserve ammunition. The earlier NPC random-ammunition
drop did not meet that contract.

The prior firing primitive had no native ammunition producer. Soldier scripts
counted `self.a.burstShots` against a family-based `role_clip()` threshold, and
two reload paths reset that count before playing the reload animation. Reading
that script counter at death would falsely refill an interrupted reload and
would not necessarily match the held composed WeaponDef's capacity.

`actor_ammo.c` now owns native primary/alternate magazine counts, keyed by
entity number and incarnation. Equipping initializes actual WeaponDef.iClipSize
once. Repeated calls for the same weapon preserve its count. Actual native
firing consumes one round before Bullet_Fire and its fire event; an empty
magazine cannot emit another shot. A self-referencing alternate is not a second
magazine. Map reset and actor release discard the records.

All four soldier reload-completion paths (exposed, cover, running, and return
to cover) call `codol_actorReloadComplete`
after their animation wait finishes. Their existing killanimscript interruption
ends the function before that call. Dead actors cannot complete a reload.
The script scheduling counter is now reset at completion too.

Native capacity and remaining getters replace family constants for native AI.
Exposed bursts are capped to the remaining magazine; cover and running reload
decisions read the same native count. The historical family fallback is retained
only for the non-AI compatibility path. Thus an actual 32-round MP5K does not use
the old 30-round family threshold or attempt empty shots until a fake threshold.

The drop still uses the real retained item spawn/physics/tag-placement path,
then replaces its randomized ammo in both item slots: clipAmmoCount is the
native remaining magazine and ammoCount is zero. Alternate held modes normalize
to their actual primary pickup, matching T5 ThrowDownWeapon. A failed item spawn
does not clear the held gun or its magazine. Successful drops consume the held
gun and discard the actor's magazine record.

The production regression links actual actor firing, magazine ownership and
drop code. Engine boundaries are Bullet_Fire, item creation, model detach and
asset definitions. It covers full/partial/empty clips, no firing when empty,
zero reserve in both slots, alternate-magazine depletion and held alternate
mode, no alternate, self-reference, failed spawn, reload completion, incomplete/
dead reload preservation, entity reuse, map reset and equipment rejection.
`make run-test_actor_drop` passes. Survival, Coop and MP script
resolvers pass against source and the final integrated binary. The parent built
and deployed the native binary and matching GSC reload/getter calls together.
Actual C1 dropped-item pickup and visual acceptance remains pending.

`node tests/soldier_reload_magazine_test.js` also executes all four actual GSC
reload phases as generators. Killing each thread at its animation wait preserves
both the remaining magazine and scheduler counter; completing it refills once.
The actual role/remaining functions use a 32-round native capacity and ignore an
out-of-sync historical burst counter.
