# Survival kiosk mouse input and credit display

The Survival script opened interactive kiosks as popups. C1 dispatches `u` at
0x46984b to 0x467ee0, then 0x5f5740, which activates a menu without acquiring UI
input. `q` at 0x469816 calls 0x467d80 then 0x5f4fb0; 0x5f50a5 sets keycatcher
0x10. Survival now uses its existing native openMenu/closeMenu contract for
kiosks. The global native popup behavior remains appropriate for noninteractive
popups. Separate death/downed watchers close the kiosk and release controls.
The old wait-any helper used endon, which terminated its caller on downed
instead of continuing to close the menu.

The authoritative wallet is survival_credit in _survival.gsc. Kills, wave
rewards and sharing update it through add_credits; purchases validate funds and
item eligibility, grant the item, then spend_credits updates the same wallet,
score, client dvar and HUD. This logic is retained. Spawn now publishes the
initial balance; the credit HUD uses the original SO_SURVIVAL_CREDITS label
and remains visible in menus. Open kiosks refresh their affordability state
when credit_updated arrives.

The original converted kiosk menus displayed card tiers, not the credit prices
actually charged by the server. The narrow patch zone carries only the two
interactive menufiles and 11 original English localization dependencies.
Their header retains the kiosk name and adds a live balance line. Enabled and
disabled row-price labels read surHUD_cost_<row>, published by menu_state from
the server's item_cost implementation. Existing purchase responses, hover and
focus handlers, ownership and perk restrictions remain unchanged.

Build: python tools/build_survival_kiosks.py then
python build_zones.py patch_common_gametype_survival --candidate --out output_survival_kiosks

Deployment candidate is output_survival_kiosks/client_shipRetail/chinese/
patch_common_gametype_survival.ff. This category patch loads ahead of the base
Survival zone. Its highlight uses the real gradient_fadein material verified
in common_mp, already loaded at that point; it has no forward material
reference to the later base Survival zone. The patch allocates block0 4096
bytes (emulated usage508) and block3 342394 (usage274138). It contains no extra
world models, textures or sounds. Deploy with the two changed rawfiles from
the active server rawfiles submodule. This work does not deploy or restart.

Checks: actual GSC credit/purchase bodies execute under Node with engine grant
boundaries doubled; insufficient funds, successful debit, grant/ownership
refusal, score/dvar/HUD synchronization and row prices pass. The authored-menu
test checks every original action/focus/ownership/perk gate is preserved,
only real credit prices replace card expressions, balance bindings are present
and serialization matches the built blobs. Native production script-menu tests
pass. Static resolution covers all194 reachable Survival scripts with no
missing builtin/script functions. AssetExplorer loads the patch's13 assets.
Live mouse interaction and visual layout still require the parent's smoke test.
