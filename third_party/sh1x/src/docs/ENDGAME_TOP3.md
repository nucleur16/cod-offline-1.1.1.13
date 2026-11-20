# End-of-match top players scene

Status (2026-10-07): first stage implemented and probed on the server. Not yet
seen in a real C1 view.

After the final killcam, every client looks through the map's
`top_players_camera`. The match's three best players walk up to their marks as
their own characters, pose, and their playercards are shown. Then the normal end
of match runs: intermission at `mp_global_intermission`, then `exitLevel`.

## Evidence

### Map entities (all 15 MP zones)

Every `mp_*` zone has:

- one `script_struct` with `targetname "top_players_camera"`. Its origin is 64
  units above the marks, about 250 units in front of them, with pitch 0, facing
  the marks.
- three `script_struct`s with `targetname "top_players_loc"`. Each has
  `script_index` 0, 1 or 2, with yaw = camera yaw + 180 (facing the camera).
  - Index 0 is the centre mark.
  - Index 1 is about 64 units to the camera's left.
  - Index 2 is about 64 units to the camera's right.

None of these ents has `target`, `fov` or `script_noteworthy` keys. zm and coop
zones have none of them.

The same zones also author two team-photo sets, each with 20 marks: 10
`script_parameters "stand"` and 10 `"crouch"`.

- `team_photo_allies` and `team_photo_axis`, each with two cameras
  (`team_photo_*_camera`) at `script_index 2` and `20`.
- `mp_estate_tropical` has no team photo.
- `mp_nuked` also has `endgame_camera_start` → `endgame_camera_end`.

None of the team-photo or `endgame_camera_*` ents are used yet. See "Open"
below.

Dump method: regex the inflated zones in `%TEMP%\codol_zones\*.zone` for the
entity-string blocks. The script is the scratchpad `entdump.py`, the same
approach as `ents.py`.

### Animations

`patch_common_mp` ships updated xanims `post_game_walkup_{1st,2nd,3rd}` and
`post_game_walkup_idle_{1st,2nd,3rd}`. `common_mp` also ships:

- `post_game_team_celebrate_{1st,2nd,3rd}`
- `post_game_ffa_beatdown_{1st,2nd,3rd}` and `post_game_ffa_beatdown_dead_{2nd,3rd}`
- `post_game_celebrate_01`

The zm zones ship pistol walk-ups (`cowboy`, `hiphop`, `toughgirl`, `lvbu`).

All of them are listed in `animtrees/player_3rd_person.atr`. The
`patch_common_mp` version adds the `_pistol` variants and the `menu_*_long`
idles. `animlists/common.animlist` (`code_post_gfx_mp`) registers them for
client script models under `=player_3rd_person`, which is client realm-0 tree
5. The server mounts the patched atr: the probe printed "CoDOL mounted animtree:
animtrees/player_3rd_person.atr (6158 bytes)". The server tree has 28 entries in
animlist order.

Measured on the server (killhouse and rust marks): the walk-up clips carry root
motion and no turn.

| Clip | Length | Distance |
| --- | --- | --- |
| 1st | 5.5 s | about 115 units |
| 2nd | 6.0 s | about 77 units |
| 3rd | 4.33 s | about 41 units |

So the marks are where each walk ends.

### C1: a script model drawn as a player (the key mechanism)

An ordinary eType-6 DObj is one xmodel (`0x443610`, second model always 0), so
a script model cannot show a CoDOL character, which is built from parts. C1 has
a dedicated path for it:

- **Server builtin `0x55b5e0`**, method table entry `0xcd3788`. The name is
  stripped, as for every C1 builtin. It takes an entity and stores client
  number + 1 at the mover's entityState `+0x64` (`0x55b6cd..0x55b6d0`). For an
  actor it stores the composition slot + 0x14 instead, and sets `+0x130 = 0x4000`
  (`0x55b6d5..0x55b6df`). It then links the source's world weapons, or the
  weapon names in an optional string or array argument, to the mover with
  `0x570a70` and returns them. The error strings are "Incorrect number of
  paramaters", "not an entity" and "All elements need to be strings (weapon
  names)".
- **Wire:** eType 6 field `0x064`, 7 bits (`entity_fields.inc`).
- **CG DObj build:** for eType 6, `0x43e809` reads `centity+0xcc`. That is
  currentState wire `+0x64`: `0x46b739..0x46b747` copies 0x1d dwords from
  `centity+0xec` (nextState wire `+0x08`) to `centity+0x70`. When the value is
  nonzero, it calls `0x4437c0(entnum, eType 6, value - 1)`.
- **`0x4437c0`** builds the DObj from that client's character record, with all
  of its parts. The tree comes from `0x443540`, which for eType 6/12/14 is the
  realm-0 tree named by `+094`. An index of 0x13 or more is read as an actor
  composition slot (`0x443809..0x443821`), so client clones stop at client 18.
  The mover's own model (`+05c`) is the fallback when the client is gone.

### C1: the playercards

`hud_720.txt` (`patch_common_mp`) loads the HUD menus
`playercard_top_players_1`, `_2` and `_3`. Decoding the expression entries
(12 bytes each, type 0 = operator, type 1 = operand) gives:

- Every element calls operator 172 as `op172(info, 0, N)`. N is 1, 2 or 3 in
  the three menus, 7 in `playercard_killedby`, 5 in `playercard_splash` and 6 in
  `playercard_spectator_hd`. So operator 172 is `getplayercardinfo(info, 0,
  slot)`, and the top players use **card slots 1, 2 and 3**. They are filled by
  `SetCardDisplaySlot(player, N)`, which sends C1 `L <client> <slot>`
  (receiver `0x469af1`).
- The labels are `@MP_1ST`, `@MP_2ND` and `@MP_3RD`. Rank 1 is centre, rank 2 is
  drawn on the left and rank 3 on the right, which matches the mark sides.
- The menu visibility is
  `!dvarbool(A) && dvarstring(B) != C && dvarstring(B) != D && dvarbool("cg_topPlayersCards")`.
  - A, B, C and D are stream references (block 3) that were not resolved; B is
    10 characters long. They read like a HUD-hidden flag and a gametype
    exclusion.
  - `cg_topPlayersCards` appears nowhere else in any zone, and C1's own dvar
    names are stripped. The server sets it with `setClientDvar`: C1's `s`
    handler `0x468a00` falls through to `0x625330`, which sets an existing dvar
    or creates it (`0x624740`).
- Each element is also gated on `getplayercardinfo(0, 0, N) != 0`, so an unset
  slot draws nothing.

### What follows the final killcam on this server

`_gamelogic::endGame` (IW4 order) runs these steps:

1. `displayGameEnd` (outcome notify)
2. the final killcam (`waittillFinalKillcamDone`)
3. `spawnIntermission` at `mp_global_intermission` for everyone
4. `processLobbyData` and `sendMatchData`
5. a 3 s wait, then `exitLevel`

The client then returns to the lobby, which shows the after-action summary.
The scene is inserted between steps 2 and 3. In this server's order the outcome
notify therefore comes before the scene. Where retail C1 put the outcome screen
relative to the scene was not measured.

### Reference scripts

IW4, IW5, T5 and BO1 have no equivalent scene. No CoDOL script for it exists in
any client zone: the MP gametype scripts are server-only, and no rawfile or
scriptfile references `top_players`. The GSC is therefore written from the
evidence above.

## Implementation

The scene logic is GSC. There is one small native builtin, because the clone
field is a wire field that GSC cannot reach.

### Native

- `src/entity.h`: `LerpEntityStateScriptMover.characterClient`
  (`anonymous.data[5]`). It is carried by the native script-mover delta table and
  by archived entities, so a delayed frame shows what it had.
- `src/online/snapshot/engine_state.c`: `OverlayEngineModelEntity`
  publishes it at eType 6 `+064` when nonzero (at most `0x13`).
- `src/online/script/script_character_clone.{c,h}`: the method
  `<script_model> codol_setCharacterClone( <player> )`. Calling it with no
  argument or `undefined` clears the field. It refuses a non-model mover, a
  non-player source, and client numbers of 19 or more. Registered in
  `scr_vm_main.c`, declared in `scr_vm_functions.h`. Weapons are not linked
  (C1's builtin does link them).
- `animmodelprobe` (developer) also prints `+064`, origin and yaw.

### GSC

`maps/mp/gametypes/_codol_top_players.gsc`:

- `init()` is called from `Callback_StartGameType`. It defines the dvars
  `scr_topplayers` (default 1) and `scr_topplayers_hold` (default
  6 s), and makes every connecting client reset `cg_topPlayersCards 0`, because
  the dvar outlives the match in the C1 process.
- `playScene()` is called from `endGame` after the final killcam:
  1. Find the camera and the contiguous marks from `level.struct`. With no
     marks or no camera, the scene is skipped with a console line.
  2. Pick the top players from `_gamescore::updatePlacement`: score, then
     fewer deaths. This applies to FFA and team modes alike, and bots are
     included.
  3. Put every client in intermission at the camera's exact origin and angles.
     The intermission view is the spawn point.
  4. For each rank, spawn a script_model at the walk's start. The start is the
     mark minus the clip's `getMoveDelta`, rotated by `getAngleDelta`. Then:
     - give the model the player's base model as the fallback;
     - call `codol_setCharacterClone( player )`;
     - play `setAnimKnobRestart( %post_game_walkup_<n> )`, which uses the
       animation-command channel from time 0;
     - `moveTo` the mark over the clip length;
     - switch to `post_game_walkup_idle_<n>`.
  5. Once the longest walk has finished, send `SetCardDisplaySlot( top[i], i+1 )`
     and `cg_topPlayersCards 1` to everyone. Late joiners get the camera view
     and the cards too.
  6. Hold for the configured time, then clear the cards and delete the models.
- Edge cases:
  - Fewer than three players: only the ranks that exist are shown.
  - A player who disconnects mid-scene: the clone is deleted.
  - A client numbered 19 or higher: the card is shown but no model, because C1
    cannot clone that client.
  - zm and coop: never called. They end through their own callbacks, their maps
    have no marks, and the menus exclude the gametype.

## Verified (private probe, udp/17769, developer 1)

- **mp_killhouse_sh dm, 5 bots:**
  - Every client was spawned in session state 3 (intermission) at the camera,
    (3886 740 68) yaw 256.
  - Three clones were bound to server tree 5 / client tree 5 and published with
    `+094=5`, `+060=0`, `+06c=0` and `+064 = client + 1` (3, 4, 5).
  - Mid-walk origins were between the start and the mark. After the walk, each
    clone was exactly on its mark, playing its `post_game_walkup_idle_*` with 7
    bootstrap commands for late viewers.
  - The intermission at `mp_global_intermission` and `exitLevel` followed.
    There were no script errors.
- **mp_rust war, 2 bots:** the two-rank scene ran (`+064=1`), followed by the
  normal end.
- Both builds pass, `test` passes, and the GSC resolver passes.

Not verified, because there is no real C1 view:

- what C1 actually draws;
- whether the card menus become visible (the A/B/C/D dvars are unresolved);
- the card slot contents for bots;
- whether the walk's root motion lines up visually with `moveTo`.

## Second stage (after the first C1 test)

The user saw the playercards draw in C1 and asked for four changes. They are
listed below with what was done for each.

### Ranking: top 3 overall, regardless of team

This was already the rule. `level.placement["all"]` is every allies and axis
player, sorted by score and then by fewer deaths. It is not split per team, and
it is not limited to the winning team. The script comment now says so.

### Weapons in hand

The native method now has the signature
`<script_model> codol_setCharacterClone( <player> [, <weapon name | array of names>] )`,
and does the weapon half of C1's `0x55b5e0`:

- **Server skeleton.** The clone gets the player's model and its tagless
  character parts on the server, so its server DObj has the hand tags.
  Nothing extra is published for a script mover.
- **Without names.** For each of the player's two live world guns (the owned
  children in `player_worldweapon.c`, which C1 keeps at
  client+0x3594/+0x3598), it links a new world gun to the clone with the same
  weapon (`+68`) at the same tag. This matches C1's `0x570a70`.
- **With names.** It links one gun per name, on the hand tag the world-gun
  update would choose. Lookup uses `BG_FindWeaponIndexForName`, which only
  finds and never registers. C1's error text is used for non-string elements.
- **Return value.** It returns the linked guns as an array.
- **Keeping them attached.** `UpdatePlayerWorldWeapons` keeps these guns
  linked every frame (`G_GeneralLink`), as it does for the owned ones. C1 draws
  them on its own clone DObj through the published link word (parent and tag).

The GSC spawns the clones first, while the players still hold their guns. If a
player holds none any more (for example, dead after the final killcam), it
passes `lastDroppableWeapon` or `primaryWeapon` instead. The guns are deleted
with their clone.

### Camera dolly

No map authors a start position for the top-players camera. `mp_nuked`'s
`endgame_camera_start`/`_end` pair is elsewhere in the map and only 10 units
long. So the shot starts `scr_topplayers_dolly` units behind
`top_players_camera` (default 64) along its view direction. A `tag_origin`
model moves onto the camera over the longest walk, using `moveTo` with ease in
(25%) and ease out (40%), so it arrives as the walks end.

Viewers are frozen spectators linked to that model with
`cameraLinkTo( cam, "tag_origin" )`, the same path `_remotemissile.gsc` uses.
C1 evaluates the entity's trajectory every frame, so the camera moves smoothly.

An intermission view cannot move smoothly. C1's `0x476848` draws `pm_type 6`
(intermission) straight from the snapshot origin, unless `0x4765c0` (a dvar
test) says otherwise, and returns before any camera link is applied. So the
viewers are spectators during the scene. They are unlinked before the normal
intermission spawn.

### Walk and movement in step

The walk clips carry their displacement as root motion. Neither the server
DObj nor C1 applies that motion to a script model's origin. Earlier, one
linear `moveTo` covered the whole clip.

Measured on the probe, the 1st-place walk barely moves for its first 0.5 s and
is still from about 4.5 s to 5.5 s. A linear move therefore slid the feet,
moving the model while the clip stood still.

`walkUp` now moves the model every 0.1 s to the position the clip's own
accumulated motion reaches at the end of that step: `getMoveDelta( walk, 0, t )`
for position and `getAngleDelta` for yaw. Each step is a linear `moveTo`
segment, which C1 interpolates.

The clip is restarted at time 0 (`setAnimKnobRestart`) on the same server frame
the first segment starts. The animation-command channel stamps commands with
snapshot server time (C1 `0x46b155`), and trajectories use the same clock, so
pose and path share one timeline.

The idle takes over with a 0.2 s blend on the frame the walk ends. The trace
(`scr_topplayers_trace 1`) prints the server origin against the clip's
position at every step.

### Verified (probe, udp/17769, mp_killhouse_sh war, 6 bots)

- **Walk.** The server origin tracks the clip's root position within 0.07 units
  at every step. Each walk ends exactly on its mark, and the idle switches on
  the same frame.
- **Weapons.** Each clone has one linked world gun (weapons 112/114) at
  `tag_weapon_right`, with a published link word (parent + 1, tag slot 2).
- **Camera.** Every client was a spectator (`pm_type 5`) camera-linked to
  entity 937, tag slot 4, flags 0x82. The camera moved from
  (3901.3 801.9 68) to `top_players_camera` (3886.1 739.7 68).
- **End of match.** Then the intermission at `mp_global_intermission` and
  `exitLevel`. There were no script errors.
- **Build and tests.** The build succeeds and all 128 `test` suites pass.

Not yet seen in C1:

- whether the camera link draws smoothly for a spectator;
- the spectator HUD;
- whether the gun sits correctly in the clip's hands.

## Third stage (after the second C1 test)

The user reported three problems in C1:

- the motion was "a tad choppy";
- the wrong top players were shown;
- the playercards appeared only at the end.

### Wrong top players: the clone is a copy of an entity's DObj

The ranking was not the cause. It is now rebuilt at scene start from
`level.players`: score, then fewer deaths, then the lower client number, as the
scoreboard ranks. With `scr_topplayers_trace`, every place is logged with
its client number, name, team, score and deaths. Rank N goes to mark N-1 and
card slot N.

The cause is how C1 builds a clone. `0x4437c0` does not read a character record
by client number:

1. It uses `+064 - 1` as an **entity number** into the client's entity-DObj
   map `0x25d9ed8`. This is the same map the animation commands use at
   `0x46b0f1`.
2. It copies the models of that entity's DObj.
3. If that entity has no DObj on the client, it falls back to the mover's own
   single xmodel (`0x443965`).
4. `0x42bdf0` treats a clone as up to date when its source DObj is missing. It
   rebuilds the clone only while the source exists and has changed.

The second stage made every player a spectator in the same frame the clones
appeared. A player who was not in a client's snapshot at that moment never had
a DObj there, so that client never got the right copy.

Fix:

- **New builtin.** `<entity> codol_sendToAll( ms )` sets `r.broadcastTime`, so
  the entity is sent to every client regardless of PVS.
- **Holding the sources.** The three top players stay players for the first
  1.5 s (`holdAsSource`). They are frozen 256 units below the camera, where
  the shot cannot see them, sent to everyone, and camera-linked like the other
  viewers. Only then do they become spectators.
- **Persistence.** The clone keeps its copy afterwards, because of point 4
  above.

Not covered:

- A viewer's own entity is never in its own snapshot. Whether C1 keeps a DObj
  for the local player, which its own clone would need, is unverified.
- Late joiners only get the fallback model.

### Choppy motion

The second stage moved each clone to the clip's position at the end of each
0.1 s step, so each step's segment ended exactly when it was replaced. A
snapshot that brought the next segment a frame late showed the model standing
still for that frame. Each 0.1 s segment also had its own velocity, so speed
changed abruptly every step.

Now every 0.1 s update aims at the clip's position one step further ahead
(0.2 s out). A segment is therefore always still moving when its replacement
arrives, and consecutive segments average overlapping windows. The last
segment ends exactly on the mark.

Measured on the probe at 60 Hz (`scr_topplayers_trace 2` samples every
server frame), for the 1st-place walk:

- the model was at most 0.33 units from the clip's root position (0.10 on
  average);
- the frame-to-frame speed change was at most 6.3 units/s, against the clip's
  own 3.0, at walking speeds of about 37 units/s.

Each clone receives exactly two animation commands, the walk at time 0 and the
idle at the end, so nothing restarts mid-walk.

### Cards for the whole scene

`showCards` (card slots 1-3 and `cg_topPlayersCards 1`) is now sent to each
player as it joins the shot: in `holdAsSource`, in `viewFromCamera` and for late
joiners. The cards are up from the first frame of the camera move until the end
of the scene, when the dvar is cleared.

### Verified (probe, udp/17769, mp_killhouse_sh war, 6 bots)

- **Ranking.** Places 1-3 were clients 2, 1 and 3 (220/0, 220/2, 220/2). The
  clones published `+064` = 3, 2 and 4, at marks 0, 1 and 2, in rank order.
- **Sources.** The three sources spawned as players at z -188, below the
  camera, and became spectators after 1.5 s. All six clients were camera-linked.
- **Weapons.** Each clone had a gun at `tag_weapon_right`.
- **End of match.** Normal intermission and `exitLevel`, with no script errors.
  All 128 `test` suites pass.

## Matches without a final killcam

The user reported that in modes without a final killcam the scene was black
and white, with the end-of-game score drawn on top of it.

### Cause

`endGame` sets the level vision to `mpOutro` (`visionSetNaked( "mpOutro", 0.5 )`)
and `displayGameEnd` draws the outcome for every player: the title, the reason,
and for team modes both team icons and scores (`teamOutcomeNotify`). `mpOutro`
in `common_mp` is the desaturated end-of-match vision. Both are applied in
every mode. What differs is what runs between them and the scene.

With a final killcam:

1. `visionSetNaked( "mpOutro" )`, then `displayGameEnd` shows the outcome and
   waits `postRoundTime`, which notifies `round_end_finished`.
2. `endGame` sends `reset_outcome` to every player, which destroys the outcome
   elements, and waits for the killcam.
3. `_damage::doFinalKillcam` gives every player the map's own vision
   (`VisionSetNakedForPlayer( mapname, 0 )`) and plays the killcam.
4. The scene runs in full colour with no outcome, then the intermission.

Without one:

1. The same `mpOutro` and outcome.
2. Nothing clears them. The scene ran in `mpOutro` under the outcome, and the
   outcome was only destroyed by the `reset_outcome` sent with the intermission.

A match has no final killcam when it does not end on a kill: a time limit, an
objective or bomb, a forfeit or a host end, a nuke, `scr_game_allowkillcam 0`,
and always in dom, koth, ctf, oneflag, sab and infect, which never mark a final
kill. Round-based modes (sd, sab, dd) take the same path on their last round:
earlier rounds `map_restart` before the scene, so it runs once.

### Fix

`_codol_top_players::sceneLook` does for each player what the killcam path
does: `reset_outcome` and `VisionSetNakedForPlayer( mapname, 0 )`. It runs as
each player joins the shot (`holdAsSource` and `viewFromCamera`, which also
covers late joiners, who get `mpOutro` from the gamestate). After the scene,
both cases are the same as before: the intermission keeps the map vision, and
the outcome was shown before the scene, as with a killcam.

## Fourth stage: native root motion (branch feat/scriptmodel-rootmotion)

The walk no longer uses GSC `moveTo` segments. The server applies the clip's
root motion itself.

### The method

`<script_model> codol_animScriptedModel( <notify>, <origin>, <angles>, %anim [, <rate> [, <blend time>]] )`
is `animscripted` for a script model (`src/online/script/script_anim.c`,
`script_model_rootmotion.{c,h}`):

- The clip plays from time 0, as `setflaggedanimknobrestart( notify, %anim, 1, blend, rate )`
  plays it. Its notetracks and `"end"` are raised on `<notify>`.
- The model is put at `<origin>`/`<angles>`.
- `RunScriptModelRootMotion` then runs once per server frame, from
  `G_RunFrame` just before the linked world guns follow their parents. It
  places the model at:
  - origin = origin + R(angles) * `XAnimGetRelDelta( anim, 0, f ).trans`
  - yaw = yaw + `RotationToYaw( rot )`
  - Here f is the clip's time in the entity's own server XAnim tree
    (`XAnimGetTime`), so pose and position share one clock.
- When f reaches 1, or the clip leaves the tree, the model holds the clip's
  last position.
- The model is released when the script moves it (`moveTo`, `rotateTo`,
  `.origin =`), when `codol_stopAnimScriptedModel()` is called, or when the
  entity is freed (slot reset through `ResetScriptModelTree`).
- Guns linked by `codol_setCharacterClone` follow, because
  `UpdatePlayerWorldWeapons` runs after the root-motion pass.

`_codol_top_players.gsc` calls it in `spawnWalker`. `walkUp` waits for
`"end"` (a 0.5 s timeout guards against a missing one) and blends to the idle.
`scr_topplayers_nowalk 1` still keeps the clone idle on its mark.
`scr_topplayers_trace` 1 and 2 still compare the server origin with the
clip. `rootMotionTrace` is the native trace: 1 prints begin and end, 2
prints every frame.

### Encoding: TR_LINEAR re-anchored every frame

- **An entity origin is an integer on C1's wire.** `delta.c` `position()`
  rounds it to whole units (the MSG_ReadOriginFloat shape). So neither
  `TR_STATIONARY` nor `TR_INTERPOLATE` (C1 0x443354 -> 0x442ac0, which lerps
  the two snapshot bases) can carry a walk of 20-40 units/s smoothly.
  `trDelta` (codec -80) and `trTime` (-72) are exact.
- **C1 evaluates a TR_LINEAR eType 6 every render frame.** The lerp dispatch
  0x443337 sends mode 2 to `BG_EvaluateTrajectory` 0x40ca40 (case 0x40ca73:
  `base + (cg.time - trTime) * 0.001 * delta`, from the current snapshot).
- **So each frame publishes:**
  - `trType` TR_LINEAR
  - `trTime` = now - K (K = 2000 ms)
  - `trBase` = round(p - v*K)
  - `trDelta` = (p - base) / K
  - `trDuration` = K + 60 s
- That line passes exactly through the clip position p at the frame's time,
  and moves at v to within 0.87/K units per ms.
- The long duration keeps `Reached_ScriptMover` from turning it into a stop.
- The hold re-anchors the same way with v = 0, so the final position keeps its
  sub-unit precision instead of snapping to an integer.
- v is the clip's mean velocity over the next 33 ms.

The 33 ms lookahead was chosen from the measured walks (60 Hz server). C1
extrapolates v until its next snapshot, so the step at a snapshot switch is
how far the clip's path bends in that time:

| Lookahead | Step at 60 Hz snapshots | Step at 20 Hz snapshots |
| --- | --- | --- |
| 17 ms (one frame) | 0.007 units | 0.31 units |
| 33 ms | 0.08 units | 0.09 units |
| 50 ms | 0.11 units | 0.003 units |

Loopback and LAN clients get every frame. Others get their `snaps` rate.

### Verified

**Probe udp/17753, mp_killhouse_sh war, 6 bots:**

- The server origin equals the clip's root position (`getMoveDelta`) at every
  trace sample.
- The three walks ended on frame 4350, 5500 and 6016 ms at the clip's end
  positions. The 1st-place walk covers 115 units from its mark.
- The per-frame extrapolation error was at most 0.012 units.

**C1 client (k33-devtest, codoMP_client_shipRetail.exe with the 1.1.1.13
Kernel33.dll), routed by the offline lobby to the probe, combat on
mp_killhouse_sh, dolly 0:**

- A 50 fps window capture over the scene shows the three clones walking
  forward from their marks.
- In 225 captured frames between 0.5 and 5 s, no frame was without motion in
  the 1st-place clone's region.
- The figure grows steadily as it nears the camera.

## Open

1. **Weapons on the clones.** Done in the second stage. Still open: whether C1
   also links offhand or stowed models, and C1's actor-source branch
   (slot + 0x14, `+0x130 = 0x4000`).
2. **Which clip set C1 used.** Candidates are `post_game_team_celebrate_*` for
   team modes, `post_game_ffa_beatdown_*` (with `_dead_` victims) for FFA, and
   the `_pistol` walk-ups. Only the generic walk-ups ship in
   `patch_common_mp`, which suggests they are the live set.
3. **The ranking rule.** Overall placement, as the user confirmed.
4. **Card menu visibility.** Resolve the menu's block-3 string references A/B/D
   and the operator ids for dvarbool/dvarstring (24/26) on a real client. Also
   check whether the HUD-hidden flag A must be cleared during the scene.
5. **Camera motion.** A dolly is implemented (second stage). Its distance and
   easing are this server's choice. C1's own values were not found.
6. **Team photo.** `team_photo_{allies,axis}` with stand/crouch marks, the two
   cameras at `script_index 2/20`, and the `pre_game_{st,cr}_*` clips in the
   same atr point to a pre-match team lineup scene, which is not implemented.
7. **zm.** The pistol walk-ups and `zombie_celebration_*` exist, but zm maps have
   no `top_players` ents. This would need its own placement source.
