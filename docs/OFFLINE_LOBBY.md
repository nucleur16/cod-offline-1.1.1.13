# Offline only: the lobby inside Kernel33

**This fork of Kernel33 is offline only.** There is no online mode and no switch: every build contains
the lobby and every run uses it. The client never contacts a Demonware lobby, a website or a launcher.

* **Start it:** run `codoMP_client_shipRetail.exe` with no arguments (optionally
  `+set dw_username <name>` to pick the profile). The game needs the install, `Kernel33.dll` and the
  staged in-process match server (`p2p\sh1x_host.dll`, `main\sh1x_rawfiles`, `p2p\survival_maps.txt`).
* **The profile** is the folder `<exe dir>\players\<name>\` (`mpdata`, `mpdata_s`, `account.txt`); the name
  is `dw_username`, else `name` of the optional `k33_offline.ini`, else `Player`.
* **Every match is hosted by the embedded server** on 127.0.0.1. There is no LOCAL PLAY choice: every
  room the player owns (Create Room, matchmaking / playlists, Zombies, Survival, Spec Ops) starts on it.

Branches: `k33-offline-lobby` (the port), `k33-offline-2` (no level gates on playing, a free lobby port,
the career as the game's own files in `players\`), `k33-offline-m14-survival` (the Python lobby's M14 and
Survival branches, always-local hosting, offline only). Sections "How it was tested" and "What still
needs the live client" say exactly what was and was not exercised.

The lobby logic is a C++ port of the Python Demonware emulator
(`CODOL-EMU/1.1.1.13-Demonware-Emulator-Python`, called "the Python lobby" below): its `main` (c641e5e)
for the first port, then `feat/mp-m14` (2d37c1e) and `fix/hosted-survival-spawn-20261028` (58db75a, which
contains `feat/survival-category`) - see "Ported from the Python lobby's M14 and Survival branches".
Python remains only the reference, the oracle of the differential tests and the language of build-time
generators.

## Starting it

Nothing has to be switched on. `sh scripts/build.sh` always compiles `src/offline/*.cpp` with g++ and
links them statically (the `K33_OFFLINE` build flag of earlier branches is gone; `CODOL_OFFLINE_LOBBY`
is fixed at 1 in `production_profile.h` and survives only so that a test fixture can compile one source
file without the lobby). At run time the lobby is started before C1's entry point in every run; the
`+set k33_offline 1` argument and the `enabled =` ini line of earlier builds are ignored (they are
neither needed nor an off switch).

`<exe dir>\k33_offline.ini` is an **optional** settings file:

| ini key | default | meaning |
|---|---|---|
| `name` | `Player` | profile name used when the command line has no `+set dw_username` (letters, digits, `_ - .`, at most 31, no leading or trailing dot). The name is the career's folder under `players\` |
| `port` | `0` | `0` = a free loopback port (the normal case). A number is an explicit lobby port, for tests |
| `alt_port` | `0` | the same for the alternate NAT-discovery port |
| `host_port` | `0` | `0` = the embedded match host's configured UDP port (`p2p\p2p_host.ini`, 17760) when it is free, else a free one. A number is used as it is, for tests |
| `level_gates` | `0` | `1` leaves C1's level gates on playing in place |
| `players` | `<exe dir>\players` | where the players' folders are |
| `runtime` | `<exe dir>\p2p\offline` | the run-time folder the lobby shares with the embedded match host |

If the lobby cannot be provided (no free port can be had, an explicit port is taken, C1's bytes at the
NAT-port site differ, the client was started by a launcher-managed Local Play session), the client
stops with a message box naming `Kernel33.log`, where the reason is. It never falls back to another
lobby: the route files on disk are not read, the host gate vouches loopback only, and `k33http.c` (the
HTTP client under the optional theatre / display-name services) accepts loopback URLs only.

## What happens at start-up (`src/offline_lobby.c`)

All of it in `TakeOver`, before C1's entry point:

1. **The match host's port.** The embedded sh1x host serves on the UDP port of `p2p\p2p_host.ini`
   (17760 by default) when that port is free on 127.0.0.1, else on a free port the operating system
   picks. The socket that proved the port free is **kept bound** until the host is about to bind it
   (`OfflineHostPortRelease`, one line in `src/p2p/p2p.c` before its own port check), so nothing else on the
   PC - a second offline client in particular - is given the same port between start-up and the match
   launch. `OfflineP2PConfig` writes the chosen port into every `P2PLoadIni` result, which is where
   the armed intent (`p2p_host.txt`, `K33LocalPlayIntent().port`), the launch observation
   (`P2PArmLaunch` / `P2PLaunchObserve` in hostgate's `sendto`), the host's own bind and the lobby's
   START_GAME address all take it from.
2. **The listener on a free port.** `DwOfflineStartServer` (C boundary `src/offline/dw_api.h`) starts one
   thread running the lobby's `select()` loop (`src/offline/dw_net.cpp`). The TCP listener binds
   `127.0.0.1:0`, i.e. the operating system picks a free port P; the UDP sockets `127.0.0.1:P` and
   `127.0.0.2:P` must bind too. If one does not, the next tries (up to 64) take a port drawn at
   random from the dynamic range 49152-65535 rather than the system's next choice: Windows excludes
   port blocks per protocol (Hyper-V / WSL; `netsh int ipv4 show excludedportrange`), hands TCP ports
   out one after another, and so offers whole runs of TCP ports whose UDP twin is refused (MEASURED,
   below). The alternate NAT-discovery pair `127.0.0.2:A` / `127.0.0.1:A` is found the same way.
   Every socket is on a loopback address with `SO_EXCLUSIVEADDRUSE`; connections and datagrams from
   a non-loopback address are dropped. Loopback binds do not raise a Windows firewall prompt.
   Nothing in offline mode binds 3074, 3078 or (unless it is free and configured) 17760.
3. **Routing C1 to it, in memory.** C1 reads `dwauth.ini` (auth host, auth port, lobby host, lobby
   port, title id) and `stun.ini` (up to ten NAT-discovery hosts) with `fopen` at `0x00630CE2` /
   `0x00630D70`; its CRT reaches `CreateFileA` through import slot `0xB421D8` (the slot
   `src/local_multi.c` already uses). In offline mode that slot gets a wrapper that answers a read of
   those two names with a delete-on-close temporary holding `127.0.0.1 / P / 127.0.0.1 / P / 3ea` and
   ten `127.0.0.1` lines. The files on disk are neither read nor written, so an install that also
   plays online keeps its real route.
4. **The NAT-discovery port.** C1 sends its probes to `m_natTravPort`, a constant 3074 written by
   `bdNetStartParams`' constructor (`mov ecx, 0xC02` at `0x00A50454`; nothing in the caller at
   `0x00705340` overrides it - MEASURED by disassembly). The immediate is rewritten to P after its
   nine bytes are verified; if they differ the listener is stopped again and offline mode is refused.
   (Only an explicit `port = 3074` leaves the constant alone.) Verified live: the probes arrive on P.
   The "your address" port inside a STUN reply stays the Python lobby's constant 3074: it describes
   the client, not the lobby.
5. **The profile name.** C1 derives the player id from `dw_username` alone (Python lobby,
   `docs/admission.md`): the name is the career. If the command line has none, the `GetCommandLineA`
   import slot (`0xB42284`) appends `+set dw_username <name>` from the ini. The name in use is also
   the player's folder (below).
6. **The level gates on playing** are lifted (`OflLiftGates`, "Level gates" below).
7. **The host gate stays on loopback.** `src/p2p/hostgate.c` vouches no lobby name, no default gateway, not
   the SSDP group, and ignores any later vouch while offline mode is active: UDP is exchanged with
   `127.0.0.0/8` only.
8. **The match host.** `P2PLoadIni` ends in `OfflineP2PConfig`, which points the embedded match host's
   `state` and `profile` folders at the run-time folder, whatever `p2p_host.ini` says, sets this
   run's port, forces `127.0.0.1`, in-process mode and admission, and clears `rollout` / `extra`.
   `P2PCheck` and `P2PInprocRun` accept hosting without the verified script service: the host compiles
   the GSC tree installed with the client (`codol_scriptPath`). The host is told `+set net_ip6 ::1`, so
   its IPv6 sockets are on loopback too (without it the server also bound `[::]:17760` - seen live).
   Hosting is always enabled: `p2p_localhost 0` is ignored and nothing has to be ticked ("The match
   handoff" below).

The lobby thread never calls into C1. It reads Kernel33's published local-host snapshot with
`K33LocalPlayIntent()`, names the selection of the owner's room with `K33LocalPlayAuto()` (arm, and
launch when START_GAME is handed over) and reports a left room with `K33LocalPlayLeave()`
(`p2p_local_play_api.h`, `docs/P2P_DEFERRED_HOST_START.md`); all are safe on any thread and start
nothing themselves. No intent file is read by anything (p2p.c still writes its `p2p_host.<uid>.txt`;
nobody consumes it).

## Level gates

A new profile is level 1 and stays level 1 until it earns XP: the stored career, the level shown in
the menus and the level a match reports are never raised to get past a gate. What is lifted instead
is the gate.

**Where the gates live.** Every lock of the lobby UI is one native question,
`AS_IsNotificationUnlocked("<notification>")` (`0x005AB300`, the Scaleform binding; its worker
`IsNotificationUnlocked` `0x005B5EB0` has that single caller): the row of `mp/uiNotifications.csv`
names an unlock flag, and the answer is whether that flag is in the unlock-flag set at `0x06ED0714`,
which the lobby serves in `getPlayerData` (service 80 task 30). The flags are `uf_rank_N` (granted
at level N) and challenge flags. Nothing in ActionScript, the playlists or the FFOTD decides it, so
**no ActionScript was changed** (the patched `codo.gfx` and its pin chain are untouched).

| notification | what it locks | flag (level) | in offline mode |
|---|---|---|---|
| `un_room_list` | Rooms | `uf_rank_5` | **lifted** |
| `un_mp_room_creation` | Create Room | `uf_rank_5` | **lifted** |
| `un_training_playlist` | training playlist | `uf_rank_5` | **lifted** |
| `un_combat_training`, `un_noob_tdm` | bot training, beginners' TDM | `uf_rank_10` | **lifted** |
| `un_mp_playlist` | the playlists | `uf_rank_15` | **lifted** |
| `un_specops`, `un_coop_room_creation` | Spec Ops challenge tab, co-op rooms | challenge `cf_ch_coop_take_mp_estate_tropical` | **lifted** (it cannot be earned without the room it locks) |
| `un_zm_asylum_sh`, `un_zm_prototype_sh` | zombies maps | challenges `cf_ch_zm_*_1` | **lifted** |
| `un_weapon_customization` | weapon customisation | `uf_rank_5` | kept |
| `un_avatar_customization`, `un_marketplace`, `un_mp_stattrack` | avatar, store, stat tracking | `uf_rank_10` / `5` / `15` | kept |
| map locks (`AS_IsMapUnlocked` `0x005AC260`) | maps in the room lists | per-map flag | **lifted** through C1's own bool dvar `2653`, which that function reads first |

**How.** The call at `0x005AB321` is redirected to `OflNotificationUnlocked`, which answers
"unlocked" for the play notifications of the table and passes every other name to C1's function. The
site is verified first (`E8` + a displacement that reaches `0x005B5EB0`); if it differs nothing is
written and the gates simply stay (logged, not fatal). The map list is opened by appending
`+set 2653 1` to the command line C1 sees (the dvar is registered at `0x005EB25D` with no flags; it
is not written to `config_mp.cfg` - checked - so it does not follow the install online).

**Why not the lobby's unlock-flag set.** Adding `uf_rank_5 / 10 / 15` to the served set would lift
the same gates with no native change, but those are also the flags of the loadout
(`mp/functionalUnlockTable.csv`: `uf_rank_5` = perk slot 3, `uf_rank_10` = killstreak slot 1, ...)
and of the store's rank requirements. Answering the notification itself touches nothing else.

**What stays level-gated** (not asked for, left as C1 has it): the loadout slots, perks, killstreaks,
deathstreak and equipment of `mp/functionalUnlockTable.csv` (levels 2 to 20; classes 4 and 5 at 69 /
70), weapon and attachment unlocks, store items with a rank requirement, weapon customisation, avatar
customisation, the store button and stat tracking. Mission progression is not a level gate and is
untouched: a new profile's Spec Ops map and co-op Create Room offer Boot Camp only.

**Side effect.** C1 announces every newly unlocked notification with a one-time tip ("Room Creation
Unlocked!", "Room List Unlocked!", ...): a new offline profile gets about eight of them at its first
login. The client records them in `mpdata` and they do not return.

## The lobby (`offline/`)

```
dw.h            core types: tagged values, writers, Reply, Config, Host, Conn, Server
dw_api.h/.cpp   the C boundary and the listener thread
dw_core.cpp     framing, bdAuthTicket, lobby connect, dispatch, STUN, services 12 / 6 / 71 / 102
dw_wire.cpp     the tagged-value codec and the 3DES-CBC frame
dw_crypto.cpp   Tiger/192 and 3DES (no OS crypto, no external library)
dw_net.cpp      the select() loop; free-port selection
dw_json.*       ordered JSON value
dw_store.*      what is kept on disk: the players' folders
dw_playerdata.* the mpdata / mpdata_s blob codec (structured data defs)
dw_profiles.*   the career profile <-> mpdata_s, rank table, legacy import
dw_storage.cpp  service 10
dw_marketplace* service 80
dw_session.cpp, dw_rooms.*, dw_matchmaking.cpp, dw_handoff.*, dw_devpanel.cpp, dw_pushes.cpp
                services 11 and 21, map selection, the match handoff, server pushes
dw_career.*, dw_stats.cpp
                rank write-back from match_report.json, challenges, weapon levels, service 4
dw_embedded.cpp generated: the embedded resources
```

Rules of the port (details in `offline/PORTING.md`):

* **No dependency on C1 or Kernel33** anywhere under `offline/`; only `dw_util.cpp`, `dw_net.cpp`
  and `dw_api.cpp` touch the operating system. The same sources build `tests/offline/dwtest.exe`.
* C++11 with `-fno-exceptions -fno-rtti -fno-threadsafe-statics`. No exception or C++ object crosses
  `dw_api.h`.
* **One thread.** The Python lobby is one thread per connection plus a database writer thread and a
  report poller; the port is a single `select()` loop, so there are no locks inside the lobby. The
  only data shared with another thread is the log file, in `src/offline_lobby.c`; the Local Play intent is
  read through p2p.c's own locked snapshot.
* The wire format is the Python lobby's, byte for byte; that is what the differential tests check.
  Its several hundred `CODOL_*` A/B switches are not ported: the port implements the default
  behaviour of `1-START-LOBBY.cmd` + `OFFLINE-DEV.cmd`.

### Transport

A loopback TCP listener with C1 pointed at it was chosen over hooking C1's socket calls: C1 stays
unmodified (one 4-byte constant aside), the protocol core can be tested with ordinary sockets against
the Python lobby, and a failure is visible instead of subtle. The cost is one listening port on
127.0.0.1, and since that port is whatever is free it costs nothing else: the offline client runs
next to a Python lobby, a real lobby route on 3074 or a second offline client.

### Store: the player's files in `players\`

There is no database and no store tree. A player is **one folder in the game's own `players\`
folder**, named after the profile (`dw_username`), holding the game's own two profile blobs exactly
as the client is served and uploads them, and one small text file:

```
<exe dir>\players\
  config_mp.cfg, settings_c.zip.iw4, settings_m.zip.iw4     C1's own files (it writes nothing else there)
  edtest\
    mpdata        423 B   the client-owned blob (classes, characters, tips already shown), as uploaded
    mpdata_s      431 B   THE CAREER: rank, XP, lifetime stats, challenge progress (the server-authored blob)
    account.txt   701 B   what neither blob has a field for
```

* **`mpdata_s` is the career.** What the client is served at login is this file. A match write-back
  rewrites it at once (temp file + atomic replace), in the same call that credits the report, so the
  file never lags. At the next start the lobby builds the career from it: total XP from
  `rank` + `experience`, the lifetime counters from their DDL members, challenge progress from
  `challenges[]`. **Deleting `mpdata_s` is a new career at level 1.** A file that is not a readable
  profile is moved aside as `mpdata_s.unreadable`, never overwritten in place. A copy of somebody's
  `mpdata_s` dropped into a new folder is that career.
* **`mpdata`** is the client's own upload, stored and served as it is (the serve-time filters of the
  Python lobby still apply to the served copy).
* **`account.txt`** is `key = value` lines, rewritten whole on a change:

  ```
  version = 1
  uid = 953451679202073159
  wallet.1 = 1168
  item = 1201 3 0 217 1794526000 purchase
  career.name = "edtest"
  career.kills = 15
  career.deaths = 3
  career.assists = 2
  career.matches = 6
  career.weapons = {"m4_mp": 900}
  career.weapon_uses = {"m4_mp": 12}
  career.rankup_gp_level = 3
  ```

  | line | what | why it cannot live in `mpdata` / `mpdata_s` |
  |---|---|---|
  | `uid` | the player id C1 derives from the name | it is how the folder is found again; the blobs do not carry it and the derivation is C1's |
  | `wallet.<n>` | marketplace balance of currency n (1 = GP) | the client reads balances from service 80 only; neither def has a balance member |
  | `item` | a purchased inventory row: item id, quantity, expiry, sku, time, source | inventory is service 80 data; `mpdata` holds what is *equipped*, not what is owned |
  | `career.weapons`, `career.weapon_uses` | per-weapon XP and use counters | `mpdata_s` has the weapon *level* only as the mastery challenge; C1 never reads `weaponStatistics[]`, and the Python lobby does not write it |
  | `career.rankup_gp_level` | the highest level whose rank-up GP was paid | bookkeeping against paying twice; no member |
  | `career.kills / deaths / assists / matches`, `career.name` | the lobby's own match counters (leaderboard rows) and the name | the DDL's `kills` etc. are the *lifetime* members, which accumulate differently (deltas reported by the server); `matches` has no member |
  | `career.collectibles`, `career.coop_missions` | story intel, co-op mission records | set into the *served* `mpdata` each time, but the client owns that blob and may upload it without them |
  | `career.lifetime`, `career.challenges`, `career.total_xp` | only the part the blob cannot hold exactly: an unknown member, a slot outside `challenges[1000]`, XP above the last rank's ceiling | the blob holds everything else of them |

  The `career.*` lines belong to the career in `mpdata_s`: without that file they are void (and are
  dropped). `wallet` and `item` are the account's and survive a deleted `mpdata_s`; deleting
  `account.txt` empties wallet and inventory and keeps the career; deleting the folder is a new
  player.
* **Not kept at all**: the login record, the purchase ledger and the XP history of the Python
  database, and `created` / `updated`. **In memory for the run only**: `Recentmetlist`, `blacklist`
  and every other storage file, and the shared (unkeyed) copies.
* **The folder.** At login the account takes the folder whose `account.txt` carries its uid; else
  the folder named like the profile, when no other uid owns it; else a folder named by the decimal
  uid (also when the name is not a usable folder name).

**Run-time files** - what the lobby and the embedded match host exchange, none of it a player's - are
in `<exe dir>\p2p\offline\` (the host's `state` and `codol_profilePath`); the folder can be deleted
whenever the client is not running:

```
p2p\offline\
  securitykey.bin, game_admissions.txt, native_server_rcon.txt     the match host's contract with the lobby
  p2p_host.txt, p2p_host.<uid>.txt, p2p_runtime.txt                p2p.c's intent / runtime files
  mpdata_s.<uid>, owned_items.<uid>.txt                            the player's blob and purchases, exported at a room start
  match_report*.json, match_report.journal.log                     the match host's reports, archived as they are folded
  match_report.state.json                                          which reports were applied (so none is credited twice)
  mpdata_s                                                         the unkeyed rank blob (the Python lobby's resources\mpdata_s)
  lobby.log                                                        the lobby's log of the current run
  resources\                                                       optional: a file here overrides its embedded copy
```

**The store of the first offline builds** (`<exe dir>\k33_offline\profile\`: `players\<uid>.json`,
`accounts\<uid>.json`, `storage\<uid>\`) is imported once at start-up when it is there
(`ProfilesImportLegacy`): each career is rendered into `players\<name>\` (the name it was played
under), with wallet, inventory and `mpdata`; a player who already has an `mpdata_s` is left alone;
`IMPORTED.txt` is left in the old folder, which is otherwise not changed and can be deleted.

### Resources

The Python lobby reads tables harvested from the client's zones, seed blobs and the publisher
`.info` files from `resources\`. The port embeds the ones it uses in the DLL
(`src/offline/tools/gen_resources.py` -> `src/offline/dw_embedded.cpp`, zlib-compressed, list in
`src/offline/resources.manifest`) rather than reading the game's zones at run time: the zone readers
(`zones/`, `catalogue/` generators, ~5k lines of Python, plus the fastfile decryption) are only
generators of those tables and the tables are fixed for build 1.1.1.13. Not embedded: the 5.9 MB
`map_spawns.proposed.json` and the weapon tables in the repository root, which only the retired
Python match server used. The only thing read from the install at run time is which map zones exist
(`client_shipRetail\chinese\<map>.ff`), for the map-availability check of the handoff.

### The match handoff

Python: a room start picks a match-server instance, sends `map` over RCON, writes `startgame.txt` and
`mpdata_s.<uid>`, pushes START_GAME at the instance and later folds `match_report.json` back.

Offline: there is no external server and no choice. **Every room the local player owns is hosted by
the embedded server** (one function, `RouteMatch` in `dw_rooms.cpp`; the native half is "Offline mode"
in `docs/P2P_DEFERRED_HOST_START.md`):

* While the owner is in its room the lobby hands Kernel33 the room's selection whenever the
  descriptor (createSession / updateSession) or the room's dwRoomInfo changes (`ArmOwnRoom` ->
  `K33LocalPlayAuto(..., launch = 0)`): the host is armed for what the room shows; nothing runs. The
  room UI's own `p2p_room` does the same for the rooms it knows. Changing the map or mode re-arms.
* On Ready / Start the lobby resolves the selection - the one SET_MAP is about to give the client -
  and names it to Kernel33 again. Accepted: write `securitykey.bin` and the admission roster, export
  the player's `mpdata_s.<uid>`, raise the launch (`launch = 1`) and push SET_MAP + START_GAME addressed
  to `127.0.0.1:<host port>` - no status probe, no map dispatch (the host boots straight into the
  selection). Refused - an unknown category or map, a missing zone or server DLL, a Survival room with
  no installed level, a host that already ran in this client run - START_GAME and SET_MAP are withheld
  and the room stays usable. Another server is never substituted.
* What the player sees when a start is refused: the room's own message box, `Localhost hosting failed:
  <reason>` (the patched room UI watches `ui_p2p_error` after Ready in every non-playlist room; seen
  live with the server DLL moved away). The lobby also pushes `Match not started: <reason>` as a ticker
  line (push 35) for the rooms that UI does not watch (playlist rooms); that line was not seen on
  screen in the room view and is unverified. A host that fails after the launch ends C1's connect
  (`disconnect`), so the player is back in the lobby (unchanged, `P2PLaunchAbort`).
* The owner leaves the room (LEAVE ROOM, or the connection closes): `K33LocalPlayLeave` - nothing stays
  armed and a launched host is stopped.
* The selection's spelling for the host: Survival is gametype `survival` on the MP level; the room UI's
  `tdm` / `ffa` / `hq` are `war` / `dm` / `koth`, and that native name is also what SET_MAP carries. A
  Zombies room's difficulty (desc[26]) reaches the host as `+set g_difficulty`.
* The owner's current room context as the room UI told Kernel33 (`p2p_room`) beats the lobby's own
  persisted room state when the two differ (Python 2089907); another player's context, or one that is
  not coherent for its category, is ignored.

After the match the embedded server writes `match_report.json`; the lobby's tick folds it into the
career (XP, rank, challenges, weapon levels, GP for rank-ups), which rewrites
`players\<name>\mpdata_s` and `account.txt` in that same step. The file contracts with the match
server (`securitykey.bin`, `game_admissions.txt`, `mpdata_s.<uid>`, `match_report.json`) are
unchanged, so the server needs no change.

## Ported from the Python lobby's M14 and Survival branches

Reference: the Python lobby's `origin/feat/mp-m14`, `origin/feat/survival-category` and
`origin/fix/hosted-survival-spawn-20261028` (which contains the second), all ahead of its `main`.

| Python commit | what | here |
|---|---|---|
| 2d37c1e (`feat/mp-m14`) catalogue: m14ebr_mp's attachment rows | one table, `catalogue/weapon_tables.py` `_ATTACH_RAW`, read only by the Python profile tools' legality check (`store/playerdata_validate.py`); nothing on a request path | **ported as data**: embedded resource `weapon_attachments.txt` (rendered from the module constant by `gen_resources.py`), with the tools' two lookups `PdWeaponRegistered` / `PdAttachmentLegal` in `dw_playerdata.cpp`. No request path reads it, as in Python. `tests/offline/test_m14_attach_table.py`: all 1915 rows of 52 weapons equal `zones/mp_codol_weapons` (the table the client and the host read); m14ebr_mp has its 58 |
| 8789d5f survival is its own category | coop descriptor naming `coop_survival_<level>` -> type / gametype `survival` on a level with survival data | **ported** (`dw_handoff.cpp` `HandoffSelect`) |
| 3742464 every level with survival data | the fourteen supported levels | **ported** (`kSurvivalLevels`, the Python constant) |
| 9853181 align with installed MP maps and placements | only levels whose zone and `main\sh1x_rawfiles\maps\mp\survival\<level>.gsc` are installed; another level -> the first available (not a rotation); every entry path (rooms, latch) carries the category | **ported** (`HandoffSurvivalLevels`). Product only: also cut to Kernel33's `p2p\survival_maps.txt` when installed, the list `P2PCheck` enforces. No level at all: the start is withheld with the reason (Python falls back to its killhouse record; reproduced under the differential flag) |
| 5b85e01 load MP map identity directly | session map = the MP level's own spelling; `survival` in the gametype vocabulary and the zone categories | **ported**: SET_MAP carries `<level>` / `survival` |
| 50fc5ef always grant cf_ch_zm_prototype_sh_1 | flag 668 in every granted-flag set, so the Zombies button opens the room list and creator | **ported** (`dw_marketplace.cpp`, `dw_career.cpp`) |
| f8235cd usermaps, zm room picks, Zombies difficulty | usermaps and map-pack zones in the installed set and the crc vocabulary; desc[26] -> `set g_difficulty` before `map` | **ported**: `Usermaps` / `InstalledMaps` (the vocabulary is rebuilt when the set changes), `Selection::difficulty`; the RCON line becomes `+set g_difficulty` on the embedded host's command line (`p2p.c`) |
| 91c483c level zones need no _load.ff; latch of another category | | **ported** |
| 2ee8a84 survival in CODOL_MAP_TYPES (launchers) | launcher / Docker env | **not applicable**: the port has no zone-type gate. The differential reference (`tests/offline/run_pylobby.sh`) is started with it |
| 704b58a route enabled localhost rooms only to their owner's ready server | intent files `p2p_host.<uid>.txt`, 15 s staleness, `sh1x_status` probe, block instead of substituting | **already equivalent, by other means**: the offline rule is stricter (always the owner's host, never another server), reads the in-process snapshot, and - per the deferred start - does not wait for a ready host. The file polling, the staleness window and the probe are not reintroduced |
| 8381ddd canonical multiplayer mode names | `tdm` / `ffa` / `hq` compared as `war` / `dm` / `koth`; the selection sent on carries the host's gametype | compare: **already equivalent**; the host's name in SET_MAP: **ported** |
| 2089907 localhost joins from the owner's room context | the owner's fresh, coherent UI context overrides the persisted room state | **ported** (`LocalhostSelection`): in-process, the owner's published context; a guest is handed off only to the owner's running host of that selection |
| 58db75a reject stale or unrelated context (selftest only) | another user's context is not a pin; a stale one is ignored | **ported as tests** (`dwtest --selftest-rooms`: another player's and an incoherent context). Staleness has no file age in-process: the lobby clears the context when the owner leaves the room |
| `fix/reaper-fixed-ports` | dedicated-server instance management | not applicable offline; not looked at further |

**The M14 EBR offline.** The Kernel33 patch zones `mp_codol_weapons` / `mp_codol_weapons_postgfx_mp`
release m14ebr_mp (marketplace item 1057, shown as "MK14") with its tables and art. The store tables
have no sku for it (product 1365), so it could never be bought; the offline lobby therefore puts item
1057 in every profile's inventory (`kReleasedUnsoldWeapons`, product only - the Python lobby does not).
Its attachments are sold and level-gated by the client's own tables as for any weapon.

**The embedded server** (`server/sh1x`, pinned 1e945cab; the installed `sh1x_host.dll` and
`main\sh1x_rawfiles` of the test shadow are newer): it hosted category `survival` (mp_qadeem_sh), `mp`
(war and the playlist's `combat`), `zm` (with `g_difficulty 2`) and `coop` from these hand-offs with no
server change. The pinned submodule is not checked out in this worktree and was not inspected.

## Request inventory

Every `(service, task)` the retail client sent in the captured sessions (430,000 log lines of the
Python lobby, `tests/offline/log_inventory.py`), and what the port does with it. "As Python" = the
Python lobby answers an empty success on purpose and so does the port.

| svc | task | what | Python lobby | port |
|---:|---:|---|---|---|
| - | - | auth connection: `BD_AUTH_ACCOUNT_FOR_MMP_REQUEST` -> 128-byte bdAuthTicket | real | **ported**, byte-identical |
| - | - | lobby connection: preamble, `BD_AUTH_SERVICE_ID` -> `BD_LOBBY_CONNECTION_ID`, keepalives | real | **ported**, byte-identical |
| - | - | STUN: IP discovery 30/31, NAT type 20/21 (three change-request variants), keepalive 14/15 | real | **ported** (self-tested on free ports; exercised live) |
| 3 | 17, 19, 24 | team management polls | empty success | as Python |
| 4 | 3, 4 | leaderboard rows | real | **ported** |
| 6 | 1, 5 | mail / message pages | empty success (record shape unknown) | as Python |
| 6 | (instant message) | developer panel `mmbook` / `mmstart` | real (offline dev only) | **ported**; in the product the list holds the local host only |
| 9 | 9, 12, 14, 16, 26 | friends lists, recent-players write | empty success | as Python |
| 10 | 1 | UPLOAD (`mpdata`, `Recentmetlist`) | real | **ported** (`mpdata` is the player's file; `Recentmetlist` is kept for the run) |
| 10 | 3 | GETFILE `blacklist`, `Recentmetlist`, `mpdata`, `mpdata_s` | real (seeds, per-player store, rendered rank) | **ported** (`mpdata` / `mpdata_s` are the player's files) |
| 10 | 6 | LISTALLPUBFILES | real | **ported** |
| 10 | 7 | GETPUBFILE playlists, `configs_s`, `questionnaire`, `activities`, `mp_rewardlists`, `broadcast`, `ffotd_client.ff` | real | **ported**; `broadcast.info` and `ffotd_client.ff` are not shipped (BD_NO_FILE, as against a fresh Python lobby) |
| 11 | 2, 4, 11, 13, 15 | leave, message batch + relay + Ready, join, create, member list | real | **ported**; the join-fix variant (off by default in Python) is not |
| 11 | pushes | relay, SET_MAP (typeId 5 / 15), START_GAME (typeId 3) | real | **ported**; START_GAME always addresses the embedded host |
| 12 | 6 | getServerTime | real | **ported** |
| 21 | 1, 12 | createSession, updateSession (descriptor, security key) | real | **ported** |
| 21 | 3 | delete session | empty success | as Python |
| 21 | 5, 13 | findSessions, findSessionsPaged | real | **ported** |
| 28 | 1, 4 | group counts | empty success | as Python |
| 71 | 2, 3, 7, 9 | Tencent telemetry (TLOG, UI clicks) | empty success | as Python (nothing is stored or sent anywhere) |
| 71 | 4, 10 | registerCodoID, claimRewards | empty success | as Python |
| 80 | 3, 6 | getProducts, getSkus | real | **ported** |
| 80 | 10, 13 | getInventory, getPlayersInventory | real | **ported** |
| 80 | 12 | consume item | real | **ported** |
| 80 | 20 | coupon list | empty success | as Python |
| 80 | 26 | purchaseSkus | real | **ported** (the optional receipt record, off by default, is not) |
| 80 | 30, 32, 2, 1 | getPlayerData, getBalance, deposit | real | **ported**; the unlock-flag set is by the player's real level |
| 80 | pushes | inventory refresh (40), ticker (35) | off by default / needs notices | **ported**; never fire without an operator trigger or notice. The ticker push also carries the reason of a withheld room start |
| 102 | 1, 3 | chat channel state, chat send | stored, empty success | as Python |
| - | - | rank write-back from `match_report.json` (XP, rank, GP, lifetime, challenges, weapon levels, co-op, intel) | real | **ported** |
| - | - | website admission (`/admit`, launch tokens), operator status file, wire tap, notices table and its editor, multi-instance placement / spawning, RCON dispatch, SQLite and its migration tools, zone readers and table generators, fastfile writer | real | **not needed offline**, not ported |


## How it was tested

**Differential tests against the Python lobby.** `tests/offline/difftest.sh` starts a fresh private
copy of the Python lobby and a fresh `dwtest.exe` (the same `src/offline/*.cpp`, built standalone) on
private loopback ports, replays captured client byte streams against both (`dwdiff.py`: the raw
bytes the retail client sent, cut out of a Python-lobby log) and compares every decrypted reply and
push. Captures hold real player ids and are not in the repository. Rerun on `k33-offline-2` with the
players-folder store:

| capture | requests | differ |
|---|---:|---:|
| one login session (2 connections) | 209 | 0 |
| 64 connections: logins, creates, joins, starts, zombies / co-op rooms, purchases, leaderboards | 2210 | 42, all `(11, 4)` |

The differences of the 64-connection capture (4 to 16 in the runs of the first branch, 42 in this
one, which shared the PC with three DLL builds, the source tests and a running client; the count
moves with timing) are all `(11, 4)` and are timing, not content: a join-request storm crossing the lobby's 3 s
relay throttle on two independent clocks. Per service on the 64-connection capture: every `(10, x)`,
`(21, x)`, `(80, x)`, `(4, x)`, `(12, 6)` row is 0. Comparison notes: the frame's messageCount and
padding are normalised, getServerTime's seconds are masked, idle keepalives and idle closes on other
connections are ignored, `mpdata_s` is compared inflated with its re-stamped timestamp and CRC
blanked, and a rental expiry may differ by 2 s. (The 55 hand-built service-80 requests of the first
branch - 264 requests, 0 differ - were not rerun; the marketplace code did not change.)

**Rank write-back** (`tests/offline/writeback_diff.py`): 33 synthetic `match_report.json` scenarios
(win / loss, rank-ups paying GP, last rank, lifetime counters, challenges, weapon XP and level-ups,
co-op missions, zombies, intel, duplicates, malformed reports, unknown ids, bots, spool files) run
through the Python lobby's own `rank_writeback` and through the port with both clocks pinned. The
port's side is **read back from its files between every two steps** (each step is a new process that
has only `players\<uid>\mpdata_s` and `account.txt`): every career member, the wallet, the retry
state and the 32768-byte `mpdata_s` buffer are identical in all 33. Not compared, because the port
does not keep them: the Python profile's `history` and `seeded_from`. Three Python bugs are
deliberately not reproduced in the product (a report dropped again after a newer one is credited
twice; a row with an unusable collectible is re-credited on every poll; a bad challenge ref loops) -
they are reproduced only under the differential flag.

**Self-tests** in `dwtest.exe`, now part of `sh tests/run_tests.sh` (on the embedded resources):
`--selftest` (Tiger against the client's own digests, 3DES known answer), `--selftest-net` (40 rounds
of two listeners at once on free ports: distinct ports, every STUN socket answers on the ports
obtained, an explicit port that is taken is refused, every second round without the system's own
choice of port), `--selftest-store` (259 checks: folders and
their names, wallet / inventory, the three files and nothing else, the career read back from
`mpdata_s`, a deleted `account.txt`, a deleted `mpdata_s`, an unreadable one, the seed's own
`objectives` value, the legacy import), `--selftest-market` (needs the Python lobby's resources),
`--selftest-career` (129, with the always-granted Zombies flag and the weapon attachment table),
`--selftest-rooms` (178: for mp, mp-tdm, zm, survival and coop rooms - the selection is handed to the
embedder at createSession; Ready starts on 127.0.0.1 with one launch and nothing ticked; a refused
selection and a refused launch are withheld with a ticker line; the owner's room context beats the
lobby's, another player's and an incoherent one do not; a guest is handed off only to the owner's
running host; the owner leaving tells the embedder; and the selection itself: Survival levels, usermaps,
the latch's category, the Zombies difficulty). `dwtest --profile <copy of a players folder> --career`
prints the careers of a folder as the lobby reads them.

**Differential tests of this branch** (private copy of the Python lobby at
`fix/hosted-survival-spawn-20261028`, loopback ports 3184 / 3284, 2026-11-12):

| test | result |
|---|---|
| `tests/offline/select_diff.py`: 45 descriptors (rooms, playlists, co-op, Survival by descriptor and by gametype, Zombies difficulties, usermaps, room latches of each category) x 2 installs (none; a made-up one with usermaps and Survival placements), Python `select()` against `HandoffSelect` | 90 selections, 0 differ |
| `difftest.sh`, the login capture | 209 requests, 0 differ |
| `difftest.sh`, the 64-connection capture | 2210 requests, 13 differ: 12 x `(11, 4)` (the 860-byte join-request storm against the 3 s relay throttle, timing, as before) and one idle 24-byte plain frame; every `(80, x)` (the flag set with 668 included), `(21, x)`, `(10, x)`, `(4, x)` row is 0 |
| `tests/offline/test_m14_attach_table.py` | 1915 rows, lobby == `zones/mp_codol_weapons` |

The always-local hand-off and the ticker line are product policy with no Python counterpart; they are
covered by the self-tests and the native fixture, not by a differential test.

**Kernel33 side**: `tests/test_p2p_start_intent.py` (WSL, gcc -m32 ASan/UBSan, the actual `p2p.c`
bodies: hosting always on, `p2p_room` arms without a tick, `p2p_localhost 0` ignored, the lobby's
selection armed and launched, replacing the room UI's; refusals; the leave; the checkbox protocols
unchanged) and `tests/offline_lobby_fixture.c` (16 scenarios: no switch needed and the old one ignored; a free lobby port in
the in-memory route files and in the NAT constant; explicit ports; the profile name and a name that
is not a folder; the players and run-time folders; the gates lifted, kept by the ini, and a call site
that differs; the match host port free, busy, explicit, reserved until released; a refused start
that holds nothing; launcher sessions refused; the local-host API calls) and the `offline` scenario
of `tests/hostgate_windows_fixture.c`, both in `tests/run_tests.sh`.

**Live, the earlier branches** (when offline mode still had a build flag, a run-time switch and the LOCAL PLAY tick; 2026-11-12, own link-based shadow client `D:\CoD\k33work\ofl2\client`, `K33_OFFLINE=1
K33_LOCAL_TESTING=1 K33_DIAGNOSTICS=1` build of `k33-offline-2`; no Python, no lobby process, no
server process, no `p2p_host.ini`; the shadow's `dwauth.ini` / `stun.ini` on disk point at an
unroutable address on purpose; pictures through Kernel33's `k33_shot.req`):

| step | result |
|---|---|
| start, brand-new profile `edtest`, with dummy loopback sockets holding tcp+udp 3074, udp 3078 and udp 17760 | `offline lobby: ACTIVE on 127.0.0.1:58215 (free port) (stun alt 63826), match host udp 63825 (free port: the configured one is in use)`; login 8 s later; the dummy sockets received nothing in the whole run |
| first login at level 1 | eight "... Unlocked!" tips, key-layout and tutorial prompts, then the menus: `edtest`, Recruit, level 1, 0 CP / 0 GP; `players\edtest\mpdata_s` + `account.txt` exist |
| Multiplayer > Start | Rooms and Create Room are usable at level 1 (greyed out without the gate lift: same client, earlier build); Create Room offers the mode list and maps |
| Create Room, LOCAL PLAY ticked | room open, intent `enabled=1 state=armed port=63825 launch=deferred`, nothing started |
| Ready | lobby: `local play: owner launches ... 127.0.0.1:63825`, START_GAME `addr=127.0.0.1:63825`; Kernel33: `p2p: match launch (lobby call)`; `server ready after 17125 ms`; netstat: the host on `127.0.0.1:63825`; the client loaded Killhouse (as a spectator until a class was chosen by a posted menu reply), five bots added over loopback rcon |
| match end | two reports folded within a second: `+0 XP ... +120 GP (balance 120)`; `mpdata_s` rewritten (ties, losses, totalShots, timePlayedTotal), `account.txt` `wallet.1 = 120` |
| restart | lobby on another free port (61283); `NAT discovery 20->21` and `IP discovery 30->31` arrive on it from C1 (the NAT-port patch); no first-run prompts; the menus show level 1 and 120 GP: the career read back from the files |
| Zombies > Create Room, LOCAL PLAY, Ready | `local host for zm_prototype_sh zm (zm)`, `server ready after 13563 ms`, round 1 of Outpost; four zombies shot; "Defeat!" summary; report folded (`+4 GP`, kills in `account.txt`, challenge 702 `zm_powerups` = 1 in `mpdata_s`) |
| restart without the dummy sockets | `match host udp 17760` (the configured port, free); Free-for-all room at level 1 with five bots, played to the time limit, `+44 GP` |
| a hand-written `match_report.json` (4300 XP, labelled as such) dropped into `p2p\offline` of the running client | folded within a second: `+4300 XP level 1->3 ... *** RANK UP *** ... +1000 GP`; after a restart the menus show level 3 and 1168 GP |
| `mpdata_s` deleted, client started by `k33_offline.ini` (`enabled = 1`, `name = edtest`) | level 1 again, 1168 GP kept, a new `mpdata_s`, no `career.` line left in `account.txt` |
| Spec Ops, Survival (no match started) | the co-op Rooms panel and its Create Room are usable at level 1; Survival opens its room at level 1 |
| final build; the store of the first offline builds copied into the shadow as `k33_offline\` (level 13, 24 GP), no `players\edtest` | `player_profiles: imported 1 career(s) of the old store ...: 953451679202073159 (level 13) -> edtest`; the menus show level 13 and 24 GP (and, at that level, the store items and the Character / Class tabs C1 gates by level are open); Create Room, LOCAL PLAY, Ready: host on `127.0.0.1:17760`, `server ready after 14359 ms` |

**Live, this branch** (2026-11-12, own shadow `D:\CoD\k33work\m14surv\client` with real copies of every
small or writable file, diagnostic build of the merged tree (`src/` layout), started with **no offline
argument**: `codoMP_client_shipRetail.exe +set dw_username edtest` plus the test harness dvars; pictures
through Kernel33's `k33_shot.req` in `D:\CoD\k33work\m14surv\shots\`):

| step | result |
|---|---|
| start, brand-new profile | `offline lobby: ACTIVE on 127.0.0.1:58787 (free port)`, login, first-run prompts, main menu (`a05`) |
| Survival button | room on Favela with no box anywhere; lobby: `room selection mp_fav_tropical/survival/survival (createSession) -> armed` (`b01`) |
| next map in the room | `updateSession` -> `mp_qadeem_sh/survival/survival -> armed`; intent file `state=armed map=mp_qadeem_sh type=survival` |
| Ready | `local host launch raised for mp_qadeem_sh survival (survival)`, `server ready after 20985 ms ... gametype=survival type=survival humans=1`; class chosen in the pause menu, spawned: `Wave 1`, `4 enemies left` (`b05`); four kills, then killed: `Defeat!` (`b06`) |
| report | `+0 XP ... +19 GP (balance 19) (report gameended)`; `mpdata_s` rewritten; `account.txt`: `career.kills = 4`, `career.coop_missions = {"coop_survival_mp_qadeem_sh": {"completed": 0, "failed": 1}}`; Statistics screen shows 19 GP (`b07`); `server ended` |
| hand-written report (labelled as such, 400000 XP, m14ebr_mp weapon XP) | `level 1->29 ... 23 weapon level(s) ... +86800 GP` |
| class editor after a restart | Inventory lists **MK14** (the M14 EBR) first, shop row "MK14 ... Owned" (`c09`); Customize: MK14 LV 24, shop attachments Angled Foregrip, Hand Stop, FMJ Rounds, RMR Red Dot Sight, Reflex ... (`c10`) |
| buy Angled Foregrip (7 days, 2000 GP), Buy and Equip | `purchaseSkus ... sku 2316 -> wat/for_gripafg`, "Purchase successful!", `wallet.1 = 84819`, `item = 2223 ...`; the gun shows the grip (`c13`) |
| Create Room (Killhouse, Team Deathmatch), nothing ticked, Ready | `room selection mp_killhouse_sh/war/mp -> armed`, launch raised, `server ready`; pause menu class 1 = MK14 LV 24 with the grip (`c20`); spawned holding the M14 (`c21`) |
| Back to Lobby | `LEAVE ROOM`, `p2p: the owner left the room (lobby); disarmed, stopping the local host`, `server ended`; report folded (`+21 GP`) |
| Matchmaking > Search (playlist 1, no room UI context) | room `mp_killhouse_sh/combat/mp -> armed` from the lobby alone; Ready: host up, spawned with the M14 (`d02`) |
| Zombies > Create Room, Hard, nothing ticked | `zm_prototype_sh/zm/zm/2 -> armed`; Ready: `Round 1/17` (`e06`); `rcon g_difficulty` on the host: `2` |
| server DLL moved away, Create Room, Ready | message box `Localhost hosting failed: server DLL not found (dll=)` (`f01`); lobby: `START_GAME withheld`, ticker push sent; room still usable (`f02`) |
| Leave that room | `p2p: the owner left the room (lobby); disarmed`; intent `state=idle`, no selection, nothing running |
| Spec Ops > Boot Camp > Start > Ready | `coop_training_mp_killhouse_sh/coop/coop -> armed`, host up, in the mission (`g04`) |
| sockets of the client (netstat, by pid) | lobby and STUN on 127.0.0.1 / 127.0.0.2 only; the match host on 127.0.0.1:17760 and, after the `net_ip6 ::1` change, `[::1]:17760` (before it: `[::]:17760`, TCP and UDP); C1's own game socket `0.0.0.0:30001` |
| outbound traffic seen | no connection to any lobby host (`dwauth.ini` / `stun.ini` on disk point at 192.0.2.1 and were not used). `hostgate: refused datagram to 239.255.255.250:1900` (C1's UPnP search, refused, 7 times in one run). One UDP socket of the process is connected to `116.130.229.213:8000`: it is not C1's (C1's `sendto` goes through the host gate) - found later to be C1's own Tencent QoS (`ied-tqos.qq.com`), refused since by the egress gate ("Network egress" below) |

Not shown live on this branch: the ticker line of a refused start on screen; a refused start in a
playlist room; a second match in one client run (after the first match the client came back to a room
view with placeholder texts whose Ready did nothing - not investigated); Survival on a usermap.

**XP from a real match was not obtained** (earlier branches): the test client's player is driven by posted clicks and
key messages and made no multiplayer kill (the bots of this server build stand idle), and zombies
kills carry no XP in the server's report. The XP / rank path of the live run is therefore the
hand-written report above plus the 33 differential scenarios; everything else of the write-back (GP,
lifetime members, kill counters, challenge progress) came from the embedded server's own reports.

One of the six starts of the first build of this branch refused offline mode with `could not get a
free loopback port`. Cause, found afterwards: that build took the system's choice of a TCP port
sixteen times in a row; the system hands TCP ports out consecutively, and this PC has UDP port
exclusion ranges (e.g. 61312-61811) in which a TCP port binds and its UDP twin is refused with
WSAEACCES (checked by hand: tcp 127.0.0.1:61400 binds, udp 127.0.0.1:61400 does not). All sixteen
fell into one such block. The listener now draws every further try at random from the dynamic range
and logs which bind was refused with which error; the self-test runs that path every second round.
Seen working live: in five further starts of the final build, one logged `the free port was found at
try 2 (before it: udp 127.0.0.1 of port 51570 refused with error 10013)` and started normally. The
match host's free port (used only when the configured one is busy) is chosen with the same care: it
must bind for UDP and for TCP, because the sh1x host listens on both.

No verified-script bypass was needed: hosting ran with the committed source (offline mode's own
policy, "Decisions to confirm" 1).


## Weapons on the embedded host: what was looked at

Reported: in a multiplayer match on the embedded host "the weapons are the IW4 index 0 ones ... it
was the FAMAS, the Vector seems fine". Looked at live (same shadow, Killhouse, the profile's classes
read with the Python lobby's codec from a copy of `players\edtest\mpdata`):

* **The loadout reaches the host.** The host takes the public profile (`weaponClasses[]`) from the
  client itself and only `mpdata_s` from the lobby (`CoDOL secure profile for client 0 ...: attached
  from lobby file`), so the players-folder store is not on this path. Custom Class 1 (item 1010,
  `vector_mp`) chosen in C1's pause menu spawns with that class; a class edited to item 1002
  (`famas_mp`) spawns with a FAMAS. Neither logs a `CoDOL loadout:` fallback on the host.
* **A FAMAS nobody equipped comes from the class reply `class0`.** The scripts in use
  (`main\sh1x_rawfiles`, `_menus.gsc` / `_class.gsc`) map a `classN` reply to column N of the stock
  IW4 `mp/classTable.csv`, whose first column is `famas` + `spas12`; only `customN` reads the
  profile. Seen live twice: a `class0` reply spawned a FAMAS while the profile's class 1 held the
  Vector. `class0` is what the fixed-loadout modes and the bots use on purpose; for a human in a
  normal mode it is never right, because C1 offers no stock classes. `docs/SERVER_CLASS0_PROFILE_CLASS.patch`
  maps `classN` from a human to `custom<N+1>` in `menuClass`; with it the same reply spawns the
  profile's class 1 (seen live). It is a script change for the server repository and is **not
  applied anywhere but the test shadow**: the server submodule could not be branched from the
  isolated worktree this was written in, and the installed script tree is newer than the pinned
  submodule revision.
* **Not established: how a player gets `class0` in the normal UI.** In the test client a spectator
  spawns only through the pause menu's class list, which sends `customN`. C1 does contain a
  hard-coded `cmd mr %i 13 class0` (`0x0071A06E`), but menu slot 13 is not the class menu on this
  host (the reply does nothing), and when C1 sends it was not determined. So the patch fixes a real
  defect that matches the report's words; that it is the path the report came from is an inference.
* **Bots** (seen in the host log, not on screen): `CoDOL bot class N: mp/botclasstable.csv has no
  member loadoutPrimary; answering none` for every member, then `CoDOL weapon '_mp' is not in any
  mounted zone; using defaultweapon_mp`: in this shadow the bots are handed the default weapon.
  Whether a dedicated host run finds that table was not checked.
* Not done: a sweep of every primary weapon, and the comparison with a dedicated `codol_dedrun` run.


## What still needs the live client

* **XP from kills in a real match** (see above).
* **XP from kills on this branch**: the Survival match's four kills carried no XP in the server's
  report either (`+0 XP`).
* **Two offline clients at once on one PC**: the ports are proven free and distinct per listener
  (self-test, and live against dummy sockets on the default ports), but two real clients were not
  started together.
* **A second match in one client run**: unchanged limitation of the in-process host (one hosted
  session per client run).
* **Shipping profile** (`K33_LOCAL_TESTING=0`): builds; not run live.
* **The LOCAL PLAY box of the Create Room dialogs is still drawn** (an unlabelled `@K33UI_LOC...` box
  in the test shadow). It is inert offline: ticked or not, the room is hosted locally (ticking still
  sets the room's minimum players to 1 and shows the read-only box in the room). Removing it needs an
  ActionScript change: `MatchMiniRoomCreatorView` and `MatchMPCTRoomCreatorView_mpcreator` set
  `localCreateOption.visible` from the room type alone (no dvar is read), so `visible = false` there and
  `minPlayer = 1` for every created room, plus dropping `LocalMatchControls`' box from `InRoomBaseView`;
  that is an FFDec recompile of the patched `codo.gfx` with a pin-chain successor stage and was left
  for a follow-up.
* The room view after a finished match (placeholder texts, Ready without effect), see above.
* After leaving a zombies room, the Spec Ops room list showed that room as still waiting; not
  investigated.


## Network egress: nothing leaves loopback (`src/egress_gate.c`)

The process - C1, every DLL it loads from the install, the embedded match host and Windows' own HTTP
stacks inside it - reaches 127.0.0.0/8 and ::1 only. The gate is installed from `DllMain` (before
any other module of the install is loaded: the exe imports `Kernel33.dll` alone) in **every build
profile**; there is no dvar, ini key or command-line switch for it. If one of its required hooks cannot
be installed the client does not start.

* **Layer A, the entry points** (hooked inline, in the system DLL itself, so the caller does not matter -
  an import slot, a `GetProcAddress` result, a pointer captured earlier, another system DLL):
  ws2_32 `connect` / `WSAConnect` / `WSAConnectByName*` / `WSAConnectByList` / `WSAJoinLeaf` /
  `ConnectEx` (handed out by `WSAIoctl`) / `sendto` / `WSASendTo` / `WSASendMsg` (a destination that is
  not loopback is refused: a connect fails with `WSAENETUNREACH`, a datagram is reported sent and
  dropped); `bind` (an "any" or LAN address is bound to loopback instead; a socket that sends or
  connects unbound is bound to loopback first - nothing listens on the network, no firewall prompt);
  `setsockopt` multicast joins (refused); every name lookup (`gethostbyname`, `getaddrinfo`,
  `GetAddrInfoW/ExA/ExW`, `gethostbyaddr`, `getnameinfo`, `WSAAsyncGetHostBy*`,
  `WSALookupServiceBegin*`, dnsapi `DnsQuery_*` / `DnsQueryEx`): literals, `localhost` and this PC's
  own name pass (Windows answers them from the adapters), any other name fails at once with no DNS /
  LLMNR / NetBIOS query; WinINet `InternetConnect*` / `InternetOpenUrl*` / `InternetCheckConnection*` /
  `InternetAttemptConnect`, WinHTTP `WinHttpConnect` / proxy and WPAD lookups, `Netbios`, the ICMP
  echo functions, and `ShellExecute*` of a web address. DLLs that load later (WinINet, ...) are hooked
  from the loader's DLL notification.
* **Layer B, the backstop**: `ntdll!NtDeviceIoControlFile`. At start-up the gate performs each kind of
  socket operation once against 127.77.33.1 and learns which AFD ioctl carries the destination where
  (five on Windows 11 26200); an AFD ioctl with a non-loopback destination is then failed with
  `STATUS_NETWORK_UNREACHABLE`. It fires only if layer A missed a path, and says so in the log; in
  every run so far it never fired.
* **The log**: `<exe dir>\Kernel33_egress.log` (every profile; restarted above 512 KB): one line per
  distinct refused destination with the module that asked (the first caller outside Windows and
  Kernel33's own import-slot wrappers), one line per loopback address and module, every bind moved to
  loopback, and a summary at exit.
* Not covered, by nature: other processes, and a UNC path or remote pipe name handed to a file API
  (the SMB redirector is kernel code). Nothing in the install's modules names either.

**What tried to talk to where** (static scan of every PE in the install, then live runs of the
diagnostic and the shipping build, 2026-11-12, shadow `D:\CoD\k33work\ofl3\client`):

| who | what | before | now |
|---|---|---|---|
| C1, Tencent QoS (`tqos_*`; the call at `0x00725DC5` right before `Live_Init`) | `gethostbyname("ied-tqos.qq.com")` at start-up, then a UDP socket connected to the result | the socket of the process connected to `116.130.229.213:8000` seen in the last live run (C1's own `sendto` goes through the host gate, so the datagrams of a connected socket did not show there) | lookup refused (`by C1@0x00A535D2`), no socket is created |
| C1 / the lobby's `configs_s.info` | `[adnews] url = http://127.0.0.1:8787/feed/news` and the promotion / hot-item banner URLs (the Python lobby's admin UI) fetched by C1's libcurl at every front-end entry (`tcp connect 127.0.0.1:8787 by C1@0x009C9875`) | a refused loopback connect per front-end entry | the lobby serves its own `configs_s.info` without those sections (`src/offline/resources/`, embedded): no connection at all; the News pane stays on "Loading, please wait..." as before |
| C1, DemonWare UPnP (bdUPnP) | SSDP search to `239.255.255.250:1900` | refused by the host gate | still refused by the host gate (2 per run); `bind 0.0.0.0:30001` of C1's game socket is moved to `127.0.0.1:30001` |
| C1, DemonWare | `WSALookupServiceBegin` enumeration on the default name space (from `gethostbyname("")`) | - | passed (this PC's addresses, no query on the wire) |
| embedded match host (`k33_sh1x_host_<pid>_<n>.dll`) | `getaddrinfo(<this PC's name>)`, UDP to the client and the lobby on 127.0.0.1 | its IPv6 sockets were on `[::]` until `net_ip6 ::1` | loopback only; its heartbeat thread (`SV_SendHeartbeatThread`) never asked for a master server in these runs |
| Kernel33's rcon / status probes (`P2PRcon`) | unbound UDP sockets connected to the host | bound by Windows to `0.0.0.0:<ephemeral>` (seen with netstat) | bound to `127.0.0.1` first |
| TerSafe.dll, TenProxy.dll, GbSpy.dll (loaded by C1), steam_api.dll | anti-cheat / login proxy / crash spy | - | no socket, lookup or HTTP call at all in any run; kept loaded (C1 calls into them), every attempt they could make is refused by the gate |
| TenSLX.dll, BugTrace.dll (WinINet, `qq.com` URLs), SteamAPIUpdater.dll, UpdateDLLWrapper.dll | Tencent login, crash reporter, updaters | - | never loaded by this client (TenProtect's start routine, which loads them, is replaced by Kernel33's `TakeOver`) |

Live proof (diagnostic build: start, login, menus, Multiplayer front end with the store list, Create
Room, Ready, a Killhouse match with three training bots to its time limit, the report folded
(`+43 GP`), back to the lobby, leave the room, quit): `Kernel33_egress.log` lists one refused
destination (`ied-tqos.qq.com`), no backstop refusal, 20659 loopback operations and one moved bind;
a once-a-second netstat watch of the pid (`ofl3\logs\t1e_netwatch.log`) saw 18 socket lines, every one
on 127.0.0.1 / 127.0.0.2 / ::1. The shipping build's run to the menus logged the same refusal and the
same loopback lines.

## Dead or unreachable now that the client is offline only (left in the tree for a cleanup job)

Nothing below was deleted; none of it can take the client online.

* `src/p2p/hostgate.c`: `HostGateReadLobbyNames`, `HostGateAllowGateways`, `HostGateAllow` / `HostGateVouch`,
  the resolve thread's name list and the SSDP exception - all behind `!OfflineLobbyActive()`, which is
  never true once C1 runs. The non-offline scenarios of `tests/hostgate_windows_fixture.c` test them.
* The Local Play checkbox protocols: startup IPC 1 / 2 (`p2p_start_intent`, `p2p_cancel_start`,
  `p2p_startup_*`, `p2p_active_*`), the production protocol 3 (`p2p_production_*`, `src/production/`,
  `src/shared/p2p_win_pipe.c`, the retained SDK), the Guest binding route (`p2p_guest_binding*`): a
  client started by such a launcher session is refused at start-up (`InstallOfflineLobby`).
  `K33LocalPlayLaunch` has no caller left (the lobby uses `K33LocalPlayAuto`).
* The verified-script service path (`p2p_verified_scripts.c`): hosting never asks for it.
* The intent files `p2p_host.<uid>.txt` / `p2p_host.txt` and `P2PRollout`: written, read by nobody.
* `src/launcher_update_bridge.c` (inert without a compiled launcher pin), `src/gfx/news_refresh*` (the
  news pane stays on "Loading, please wait..."), invitations / friends / clan UI paths (answered empty
  by the lobby), `src/kernel/kernel_name_network.c` (opt-in build flag, names `codol.kewkew.org`).
* `src/theatre.c` and `src/name_display.c`'s service: their URLs are refused by `k33http.c` unless
  loopback, so the THEATRE button's recording service cannot be reached.
* `src/discord.c` talks to the local Discord client over its named pipe (no socket of its own); it is
  the one feature left that can make something else (Discord) show the player online.
* The Python lobby's `CODOL_*` online knobs were never ported; `scripts/build-controller.sh` does not
  link `src/offline` (it did not link `src/production` before either, i.e. it was already stale).
* In the UI: the LOCAL PLAY boxes (above), the DEV PANEL of the room view.

## Not done

* `broadcast.info` (ticker) and `ffotd_client.ff` are not shipped; the notices table is not ported.
* A committed, anonymised replay fixture: the differential tests need a private capture.
* Multi-player offline rooms: the room code keeps joins and fan-out (the captures need them), but the
  product was built and tested for one local player.
* Friends, mail, clan, chat relay, coupons: answered empty, as the Python lobby does.
* An import of a Python-lobby career (its `players\<uid>.json` has the schema of the old offline
  store, so placing it under `k33_offline\profile\players\` imports it; its SQLite wallet does not
  come along).


## Decisions to confirm

1. **Hosting without the verified script service in offline mode.** `P2PCheck` / `P2PInprocRun`
   accept the installed GSC tree when (and only when) offline mode is active: one player, loopback
   only, admission still required. Without this, local hosting cannot run self-contained at all (no
   `sh1x_host.dll` on this machine exports `Sh1xHost_UseVerifiedScripts`). Since the client is offline
   only, this is now the rule of every build.
2. **Which gates are lifted.** Playing is: rooms, room creation, playlists, co-op and zombies room
   types, maps. The store, avatar and weapon customisation and stat tracking keep their levels, and
   so does every loadout unlock. Moving a row is one line of `kOflPlayNotifications`.
3. **The one-time "Unlocked!" tips** at a new profile's first login are C1's own and were left.
4. **Store location**: `<exe dir>\players\<name>\` and `<exe dir>\p2p\offline\`.
5. **Not embedded on purpose**: the Python repository's `wallet.json` (balances keyed by real player
   ids) and `purchased_items.txt` (19 legacy purchases it gives every new account). Offline accounts
   start with an empty wallet and inventory plus the default items.
6. **UPnP**: offline mode vouches no gateway and no SSDP group. No delay was seen live; if a client
   is ever seen waiting on UPnP, the alternative is to vouch the gateway as online mode does.
7. **No LOCAL PLAY choice** (owner, 2026-11-12): every owned room is hosted locally; see "The match
   handoff". **The M14 EBR is owned by every profile** (it has no sku to buy it with).
8. **Tracked `Kernel33.dll`** was not rebuilt or committed; the offline profile is opt-in.
