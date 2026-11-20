# Survival M14: shared weapon patch mount and native composition

## Audit and cause

Native branch `feat/mp-m14` at2b064dac is an ancestor of70ac4780. Its M14 work is
asset integration/documentation; the generic native composer already implements
it. The current M14 composer source preserves that branch. Client main2e08b66's
M14 tables/models match the later dev art sources, and the installed patch files
match the latest staged build receipts:

- `mp_weapons.ff`:108677bytes, MD5 `22e724382b185632b699b54cd583a04d`.
- `mp_weapons_postgfx_mp.ff`:14043bytes, MD5
  `c0111bc74c91dfaa0a79b5970307cf7b` (the corrected streamed-surface art, not the
  superseded inline-geometry build).

The missing behavior was native Survival mounting. C1 loads these permanent
weapon patches globally. The server matched only `<category>_*.ff`; Survival
borrowed the MP gametype archives but skipped the two `mp_weapons` patches.
The client therefore had the released M14 tables and receiver art while the
Survival server lacked them. Ordinary MP already mounted both patches.

## Narrow production change

`category_patch.c/.h` contains the actual sorted patch-folder enumerator,
extracted from `asset_commands.c` to make its filesystem/precedence contract
testable. Existing category-owned prefix behavior, folder override, stock-zone
collision skips, capacity bounds and alphabetical first-definition ranks remain.
Survival additionally borrows exactly `mp_weapons.ff` and
`mp_weapons_postgfx_mp.ff`. It does not import arbitrary MP maps, frontend
zones or similarly named old aliases. Other categories retain their prior policy.
The method does not change weapon art, aliases, class data or gameplay logic.

## Actual before/after native evidence

Both probes used the same installed zones, rawfilesf05ed0c and isolated Favela
Survival UDP17761. Both quit/exited; neither touched user processes.

Before, exact old35E491 EXE (PID77300,29.19seconds):

- M14 attachment query returned50legal rows/147overrides.
- All44 added world/viewmodel queries returnedcomplete0.
- Native composed recipe `*a 21 0` and full camo4 recipe
  `*a 21 4 97 1 11 2 16 0 31 0 32 0 33 0 41 0` both failed:
  `m14ebr_mp camo 0/4 has no complete base-model recipe`.
- The actual stock M14 model slots are `viewmodel_m14_ebr` and
  `weapon_m14ebr_tan`, plus viewhands/knife. This is the stock-model path, not
  the new `wea_m14ebr_m14ebr_*` receiver models. Production `getLoadoutWeapon`
  tests `recipe.base_world_model/base_view_model` and, when absent, returns
  `SafeStockWeapon(base)`. This ties the missing-model composition to the
  real stock fallback. No old live player/profile return or rendered mesh was
  captured, so the evidence is not overstated as a screenshot of that path.

After, exact new583EB848 EXE (PID115192,31.22seconds):

- Both patches mount in Survival at ranks206/207, ahead of stock.
- M14 has58legal rows/226overrides, including six pmove inputs.
- All44 added world/viewmodels returncomplete1 (skeleton, geometry, materials,
  mesh and core all1).
- Both actual native recipes resolve over `m14ebr_mp`, applying attachments:
  base/camo4 recipes select `wea_m14ebr_m14ebr_blk` and
  `wea_m14ebr_m14ebr_04`, respectively, with corresponding `_vmgun` viewmodels.
  The source missing-art fallback condition is no longer true for these
  complete recipes. Stock base definitions remain unchanged, as intended.
- Both runs have no script runtime or missing-default-table fatal.

This proves native composition/model availability rather than merely successful
archive loading. It does not claim a new C1 visual acceptance session.
The original streamed models and hidden default-part art are not rebuilt here.

## Tests and build

`bash tools/test_category_patch.sh` passes ASAN/UBSAN on the actual
production enumerator with filesystem/cvar boundaries. It exercises all four
categories, both borrowed archives, case-insensitive names, rejected cross-mode
maps/UI/old aliases, alphabetical precedence, configured folder override, stock
collision and bounded capacity.

All four source and exact built-EXE resolver gates pass: Survival/Favela,
MP/war/Killhouse, Coop/training and Zombies/Prototype.

Windows build command (WSL, active native checkout):

    make -j8 LOCAL_TEST=0 OS=Windows_NT CC=i686-w64-mingw32-gcc CPP=i686-w64-mingw32-g++ AR=i686-w64-mingw32-ar OBJ_DIR=obj/codol-win32 bin/sh1x_dedrun.exe host-dll

Outputs:

- `bin/sh1x_dedrun.exe`: SHA256
  `583EB8484C5B73620C2E0650CBEF663E0EDB33C108AEEDFEA16EB7CA04571275`.
- `bin/sh1x_host.dll`: SHA256
  `60091BEB7E4C764675A364C94F3A728D6AD4DA6E54744FA36A2D594C9EFE9244`.

Sanitized per-model, recipe and mount receipts:
`tests/fixtures/survival_m14_composition_probe.json`. Full private probe logs and
multipacket command responses remain under `obj/survival_m14_before_full` and
`obj/survival_m14_after`. No deployment, commit, user input or process restart
was performed by this worker.
