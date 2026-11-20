# Throwing knife attachment

Parent-reviewed native code and retained missile callsites built successfully
without warnings; deployed SHA256
7D53C950B0D103DD1684964FEB937C500473913188085D213560C6E62CD942FE.
No automated tests; actual wall/body pickup behavior remains unverified.

The same missile settles at the trace-fraction impact position, retains its
weapon/model and pickup identity, and uses native G_EntLinkTo for a hit-bone
attachment. Bone names follow T5 hit locations and are checked against the
target DObj. Missing bones use the target-origin relative impact pose.
Damage receives the unchanged native G_Damage arguments before attachment.
Generation references protect against callbacks deleting/reusing either entity.

Corpse capture records the death generation and a serial. Synchronous lethal
damage can link directly to that corpse; later corpse creation transfers the
existing knife. Spawn detaches/transfers before the player origin is reset.
Entity free and map reset clear attachment state. Recovery keeps the previous
generation-safe trigger/death cleanup and one-round ammo transfer.

Direct wall hits use an explicit sh1x incidence policy, dot >0.7 (roughly45
degrees from the incoming surface normal); glancing hits retain bounce.
This meets the requested wall sticking behavior but is not claimed as
recovered proprietary server tuning. Body behavior is based on T5 native
link/corpse handling. C1 generic parent/tag evaluation has no missile exclusion;
the native snapshot link overlay applies after missile fields.

This deployment also includes reviewed IW4x map source updates: tropical
Favela fog/vision and the second palm's matching C1 animation tracks, plus
Killhouse's authored ambient_scoutsniper_ext0 alias present in C1 common_mp.
Shadow work remains closed following user confirmation.

## 2026-10-13: knives float after a kill, and could not be picked up

Measured on an isolated probe (mp_killhouse_sh war, two bots, the
`scr_equiptest "knife|knifeat|knifepick"` hooks, developer traces
`CoDOL knife:`).

- Floating. A lethal knife was linked to the corpse made by the same kill.
  That clone has no server DObj until the next G_RunFrame, so
  G_DObjGetWorldTagMatrix(corpse, j_spinelower) failed and link_target
  silently fell back to tag 0: a link to the corpse ORIGIN. The server's
  corpse bone then dropped from z 41 to z 10 while the knife held z 45.5,
  and C1 drew the child at origin + offset: in mid-air where the player had
  stood, never on the falling body.
- Fix: a body hit is never origin-linked. The knife is moved onto the hit
  location's ragdoll bone, backed off along its flight (T5
  AttachMissileToEntity / GetAttachedMissileOffset), and linked to that bone.
  A fresh corpse's bone is retried each frame (at most 1 s); a live carrier
  that dies moves the knife to the same bone of its corpse; a carrier that
  goes away (freed corpse, respawn without a corpse, bone never resolving)
  drops the knife to the floor. Probe: the knife now follows the corpse
  down (z 49 -> 20 with the body) and stays on it after the respawn; a
  knife in a surviving player's ankle moved to the corpse's ankle when the
  player died. A wall knife and a floor knife stay where they hit.
- Pickup. C1 throwingknife_mp is stickiness 5 (IW4 WEAPSTICKINESS_KNIFE).
  The 2026-10-06 "sticky grenades are never thrown back" filter in
  InventoryMissileUseAllowed treated every nonzero stickiness as a
  throwback grenade, so a knife was never a use candidate and
  RecoverMissileAmmo refused it. Knives are now exempt (they take the
  C1 528292 item-pickup branch and the ordinary pickup hint). Probe: holding
  use at a knife on a corpse, in a wall and on the floor returned it
  (ammo 0 -> 1) and removed it. Walking over it does not: C1's Touch_Item
  (528500) sends touched pickups to the ammo-leech path (5283e0), and only
  the use path (528270) recovers a class-9 knife.
