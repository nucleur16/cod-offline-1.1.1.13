# Survival helicopters: rotor loop and hostile compass icon

## Cause and source change

Historical rawfiles commit `b625a14` implemented the actual IW4-style rotor loop
in `common_scripts/_heli_rotors.gsc`. That helper survives in current source,
but the later Survival `spawn_heli` factory never invoked it. The factory now
calls the existing helper for the little bird, Hind and jug-drop Mi-17. The
helper selects the model-specific mounted rotor clip, otherwise the common
`bh_rotors`, and sends real `setAnim(a,1,0,1)` on the vehicles tree. Its existing
once-only and `scr_heli_rotors=0` behavior is retained. It does not apply root
motion or invent procedural rotor geometry.

The ownerless factory also assigned only script field `team`. It now calls the
real `setVehicleTeam("axis")` producer. This alone cannot fix the C1 icon: the
actual C1 vehicle compass renderer `0x681010` selects default friendly/enemy
materials from the controlling player's team. The owner's absent initialization
is client0, which matches the first player's team and selects a friendly icon.
No fictitious enemy player/controller is substituted.

C1 already supports an explicit vehicle compass icon. `0x6811BF` reads centity
`+0x15C` (entityState `+0x78`); when nonzero, `0x6811E9` resolves the icon before
the automatic team branch at `0x681209`. This is a **four-bit** selector1–15 into
CS6085–6099: the lookup address `0x6D13040` corresponds to CS6084 plus selector,
not the general HUD material bank. Zero retains normal automatic selection.

The new real native method `codol_vehicleSetCompassIcon("enemy"|"friendly"|"")`
validates a live native vehicle and selects its mounted VehicleDef's actual
`compass_icon[1]` or `[0]`. It reuses the existing minimap registration producer,
now factored as `RegisterMinimapIcon`, including case-insensitive reuse,
15-slot bounds, name validation, gamestate and reliable configstring updates.
The selector is captured in the entityState extension, copied with snapshots
and archived with a four-bit value. Historical snapshots never look up the
current reused vehicle. The entity frame writer publishes this value at real
C1 `+0x78`; no objective/HUD proxy is spawned. An empty variant clears the
explicit icon, leaving normal owner-team behavior. Only Survival calls it.

## Hostile material and color evidence

A passive read of actual C1 PID84308's loaded VehicleDef table `0x02BC3018`
confirmed Cobra enemy material `compass_objpoint_helicopter_busy` and friendly
`compass_objpoint_helicopter_friendly`; Pavelow uses
`hud_minimap_pavelow_red`/`hud_minimap_pavelow_green`. The native method reads
these mounted definition members, rather than guessing a name or relabeling a
friendly asset.

The installed `iw_image_00.iwd` helicopter busy/friendly and littlebird busy IWI
images were decoded with the retained AssetExplorer TextureExtractor. The
512x128 original helicopter busy atlas has 12,884 pixels with alpha>100:
mean RGB `(255,163.66,132.63)`, 6,751 red-dominant pixels and zero green-dominant.
The friendly atlas's same glyph has mean `(168.07,255,174.59)`, zero red-dominant
and 6,749 green-dominant pixels. Littlebird busy likewise has 7,678 red-dominant
and zero green-dominant pixels. The enemy asset is authored orange-red with a
white glyph; it does not require a missing red team tint. In renderer681010 the
caller RGBA is copied at681116–68112A before icon selection; icon choice does
not introduce a separate team tint. The same color and selected material flow
to68136E–6813B5. Pavelow IWI was not separately found by name; no decoded
Pavelow pixel result is claimed. Compact sources/hashes/statistics are retained
in `tests/fixtures/survival_helicopter_evidence.json`; decoded images remain
local under `obj/heli_icon_pixels`.

## Verification

- `SANITIZE=1 bash tools/test_vehicle_compass.sh`: actual production GSC
  method and actual minimap registration pass ASAN/UBSAN. Captured archive
  history/clear and actual entity wire+78 tests pass. Other non-overridden
  entities retain their automatic behavior; invalid receiver/variant/material,
  out-of-range wire value and exhausted minimap bank reject safely.
- `node tests/survival_helicopter_test.js`: actual factory/helper bodies pass
  for all three Survival helicopter models, native axis/enemy selection,
  mounted clip and common fallback, idempotence, disabled option and failed
  spawn. Shared MP/Coop/Zombies rotor helpers were not modified.
- `python tests/test_c1_vehicle_compass.py --image <original C1.exe> -v`:
  3 tests pass against original selector instructions. Normal friendly/enemy
  cases choose their definition materials; explicit selector precedes a
  friendly controller and supplies the correct original minimap-name argument
  to registration type7. This bounded test stops before material registration;
  it does not substitute an engine function or claim GPU rendering.
- Source resolver gates pass Survival/Favela, MP/war/Killhouse,
  Coop/training and Zombies/Prototype. The new built EXE
  `35E4910FF4BB677E82771134AFE01F34B21C25D75368E35A1347DA3EC40D5A11`
  passes all four built-binary resolver gates above. Shared native EXE/DLL build was coordinated
  with the wounded-actor owner.

No user input, focus, restart, deployment or commit was performed. A real C1
rendered frame of the new rotor/icon behavior remains visual acceptance.

## Final owned native run

Rawfiles `f05ed0c` and final EXE above / host DLL
`C3850864C5E650CCAD113FF4CDB330150A9698AD79EB0988612CB80FECFED32B`
were verified in owned Favela UDP17761 PID104508. Startup, two boss helicopter
spawns and jug-drop spawn ran44.42seconds with no script runtime or missing-table
fatal; the process quit and exited. Both attack helicopters flew through
multiple positions and the transport delivered a real juggernaut.

Native logs show entities601 (little bird),818 (Hind) and826 (Mi-17) each bound
their real vehicles animtree (server2/client2,13entries), then invoked the shared
rotor helper's actual setAnim loop. The parent/user must still visually accept
rotor movement in C1; no native diagnostic animation-weight output is claimed.

A passive read of this exact owned process found axis native team1 on all three,
with controlling client0 retained. Entity extension base0BFF5100 (EXEbase00540000,
64-byte stride, compass selector+10) held11 for601/818 and12 for826. Effective
CS6095 is `compass_objpoint_helicopter_busy`; CS6096 is
`hud_minimap_pavelow_red`. Thus the explicit hostile icon is produced without
inventing an enemy controller. Wire+78 serialization and C1 selection are
independently executed by the production/x86 tests above; no packet capture or
rendered frame is claimed.

The first owned97372 probe also spawned all3models without fatal, but its reader
used the wrong file-local `_live_ext` because COFF contains both playerState and
entityState statics of that name. That zero-selector observation is discarded.
The verified reader follows the actual entity getter's CALL and resolver's live
record LEA, and resolves the correct relocated entity extension base. Both runs'
logs remain under `obj/survival_helicopter_probe*`; neither altered user processes.
