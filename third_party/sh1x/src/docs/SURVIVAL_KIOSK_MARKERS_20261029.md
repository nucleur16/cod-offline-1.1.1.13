# Original Survival kiosk HUD and compass markers

The authored CoDOL placements contain two physical kiosk kinds: the locker
provides ammunition/perks and the support case provides killstreaks. Preserve
those positions. Three service HUD waypoints identify the services above those
crates; the locker's single compass objective alternates ammo/perks every two
seconds, while support keeps its own objective.

## Original asset evidence

`mp/kiosks.csv` column 6 supplies the original CoDOL icons, rather than substitute
art: row 0 `kiosk_item_ammo`, row 3 `specialty_plus_one`, row 10
`specialty_predator_missile`. Read-only zonekit parsing of the installed fastfiles
confirms the first two material assets in `common_gametype_survival.ff` and the
last in `common_gametype_mp.ff`, both mounted for Survival.

The existing `p2p/iw_model_18.iwd.json` records the ammo/perk stream dependencies
as `images/kiosk_item_ammo.iwi` and `images/specialty_plus_one.iwi`. Its decoded
surface verification reports zero different pixels and no problem for each.
The predator material/texture already ships in the MP gametype. No new material,
image replacement or stream archive is required.

## Native interfaces

IW5 `_so_survival_armory.gsc` (decompiled `IW5-GSC/scripts/1574.gsc`,
`armory_use_monitor`) creates an archived HUD element at the armory, alpha .75,
`setShader(self.icon,12,12)` and `setWaypoint(1,1,0)`. The kiosk script uses that
contract, with heights 48/72 above the actual crate origins for legible locators.
`script_hud_waypoint.c` implements the real waypoint type/flag updates.

The previous kiosk script called only `objective_add(active,origin)` and supplied
no objective material. Each crate now saves its objective ID and calls
`objective_icon` with the same original service material. The native objective
setter calls `G_MaterialIndex`; the snapshot overlay publishes each objective's
material as a twelve-bit field. The rotation changes only the material of the
existing locker objective. It does not create new physical/map placements.

The rotation ends on `survival_over`. The level cleanup destroys every kiosk HUD
element and deletes each kiosk objective. Map teardown owns the remaining VM/HUD
state, and entity-bound threads end when their kiosk is freed.

## Validation and limits

- `node tests/survival_kiosk_markers_test.js` executes the actual marker GSC
  bodies with only HUD/objective boundaries stubbed: three original materials,
  two unchanged compass anchors, native waypoint arguments, alternation and
  complete shutdown cleanup pass.
- The existing Survival kiosk economy test still passes.
- Survival Favela resolver against source and the deployed native binary passes.
- Rawfile diff whitespace check passes. This is a script-only change; no native
  or client asset rebuild is needed. Actual C1 marker appearance is pending
  parent deployment and gameplay verification.
