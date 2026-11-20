# Survival uses the multiplayer world entity set (2026-10-28)

## Cause and source evidence

`maps/mp/_load::codolRemoveMissionEntities()` applied IW4's non-Spec-Ops
selection only when `sv_zoneType` equalled `mp`. Survival executes the MP
level loader with category `survival`, so it skipped this cleanup. The base
Favela zone itself contains campaign props; a separate campaign addon is not
required to reproduce the obstruction.

The installed 1.1.1.13 mp_fav_tropical.ff was decrypted and its base MapEnts
examined: 2178 entities, including 241 explicitly mission-owned records
(123 script_brushmodel,106 script_model,8 script_origin,3 info_vehicle_node,
1 trigger_use). For example, brush *1 at(-586,394,276) carries
script_specialops=1 and script_specialopsname=coop_take_mp_fav_tropical.
Normal-world selection retains1937 records, including125 ordinary brushes,
378 ordinary script models and22 script_specialops=0 records.

The existing predicate matches the IW4 reference in
`maps/_specialops_code::specialops_remove_flag_check` and
`specialops_remove_name_check`: outside a named mission, remove only
script_specialops=1 and explicitly named mission entities. It does not remove
all props, guess class/model names, alter baked world collision, or invent
positions. The compressed fixture retains the extracted relevant MapEnts keys,
source archive SHA-256, entity-text CRC32 and expected census.

## Changes

- The shared MP loader applies the same authored-ownership predicate to Survival.
  A stale campaign `specialops=1` cannot turn Survival into a campaign. Ordinary
  MP retains its previous explicit-specialops override; coop/zombies retain
  their separate loaders.
- Native clipmap installation composes a session addon only for category coop
  and only when its session map differs from the base map. A legacy
  coop_survival_* ID cannot import campaign entities merely because its
  spelling differs from mp_fav_tropical.
- CRC-validated MP entity overrides are available to Survival's MP base as well.
  Their original CRC, worldspawn/brace and trigger-index checks are unchanged.
- Survival's authored `maps/mp/survival/<level>::data()` remains its source for
  starts, squads, kiosks and support-drop locations. Favela retains15 player
  starts,16 squad leaders,3 kiosks,9 carepackage locations,18 claymore spots,
  1 dog spot,8 chopper start structs and97 helicopter nodes. These are source
  data counts, not a claim that every authored chopper path is selected at once.

## Validation and limits

- Native pure-C category/alias ownership tests pass.
- `node tests/survival_entity_selection_test.js` executes the actual GSC cleanup
  body over all2178 extracted entity records. It checks the exact241 deletions,
  preservation of ordinary geometry, MP/coop/zombies category guards, stale
  specialops state, and unchanged authored placement inventories. A regression
  to the old MP-only guard reproduces the leak.
- Both changed native translation units pass 32-bit MinGW syntax compilation.
- GSC resolver gates passed (exit0) for Survival, ordinary MP war, and
  coop_take_mp_fav_tropical.

No runtime script/zone deployment, process restart, or commit was performed by
this worker. The tests establish ownership selection, not an in-client walking
check; that remains for the parent after integration.
