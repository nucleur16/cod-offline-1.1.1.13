# Survival crate class selection through the original pause UI

The kiosk previously opened generic `changeclass`. C1's actual menu-name router
at `0x6B7940` treats that name as movie activation only. Its team variants
`changeclass_marines` and `changeclass_opfor` reach `0x6B7C4F`, which invokes
the original pause movie's `cpp_openChooseClassMenu` at `0x6B7CCB`.

The kiosk now opens the player's precached team variant. Existing pause code
builds the profile class list, updates its current-class indicator immediately,
and submits `ScriptMenuResponse("changeclass", classID)`. The server's existing
transaction wrapper validates a living player, a different real custom slot,
and sufficient wallet balance. It debits once and invokes the actual `_menus`
callback with the immediate Survival loadout permission. It does not wait for
the next wave or respawn. Support restoration follows the normal class rebuild.

Closing the Scaleform pause menu normally has no GSC cancel response. A small
MainPause observer is active only while a Survival kiosk selection is pending.
It must observe the menu open before reporting a later close, so a queued open
cannot be mistaken for cancellation. Escape/back reports a free cancellation;
the server also absorbs a late close following a successful class selection.
Death/down/disconnect cancels the pending purchase and unfreezes controls.

Validation: three original-C1 menu-router executions pass with image hash guard;
actual AS observer tests cover delayed open, close-once, successful choice and
other modes; actual kiosk and `_menus` callback tests show an immediate loadout
and single wallet debit outside grace period. Cancellation, same/invalid class,
insufficient balance and duplicate replies remain free. MainPause compiled and
the shared `common` candidate built successfully. No physical cursor or
desktop key input is needed for these checks. Live in-game visual acceptance
remains separate from the code and compiled-asset checks.
