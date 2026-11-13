# Offline mode: the lobby inside Kernel33

Branches `k33-offline-lobby` (the port, on `main` d319a61) and `k33-offline-2` (on `main` b145bbc:
no level gates on playing, a free lobby port, the career as the game's own files in `players\`).
Status: built, tested offline (unit fixtures, self-tests and differential tests against the Python
lobby) and run in a live shadow client from a brand-new level-1 profile: login on a free port with
3074 / 3078 / 17760 held by something else, Create Room at level 1, Local Play, match on the embedded
host, write-back into the profile files, restart and read-back. Sections "How it was tested" and
"What still needs the live client" say exactly what was and was not exercised.

Goal: a player with the game install, Kernel33 and the staged in-process match server
(`p2p\sh1x_host.dll`, `main\sh1x_rawfiles`) starts the client with no lobby process, no Python, no
website and no launcher; logs in as a local profile; sees menus, store and loadouts with a
persistent local career; creates a room (multiplayer, zombies, survival), ticks Local Play, starts,
plays on the embedded server with bots, and gets rank / XP / unlocks written back afterwards.

The lobby logic is a C++ port of the Python Demonware emulator
(`CODOL-EMU/1.1.1.13-Demonware-Emulator-Python`, called "the Python lobby" below). Python remains
only the reference, the oracle of the differential tests and the language of build-time generators.

## Switching it on

Offline mode is two switches, both off by default.

1. **Build flag.** `K33_OFFLINE=1 sh build.sh` (or `build-dev.sh`) defines `CODOL_OFFLINE_LOBBY=1`,
   compiles `offline/*.cpp` with g++ into `obj_offline/` and links them statically (libstdc++
   included; the DLL's import list does not change). Without the flag none of that code is compiled
   or linked and `OfflineLobbyActive()` is a constant `FALSE`. The flag is independent of
   `K33_LOCAL_TESTING` / `K33_DIAGNOSTICS`: it adds a feature and enables no diagnostics.
2. **Run-time switch**, read once before C1's imports are filled:
   * `+set k33_offline 1` on the command line, or
   * `<exe dir>\k33_offline.ini` with `enabled = 1`.

   | ini key | default | meaning |
   |---|---|---|
   | `enabled` | `0` | `1` switches offline mode on |
   | `name` | `Player` | profile name used when the command line has no `+set dw_username` (letters, digits, `_ - .`, at most 31, no leading or trailing dot). The name is the career's folder under `players\` |
   | `port` | `0` | `0` = a free loopback port (the normal case). A number is an explicit lobby port, for tests |
   | `alt_port` | `0` | the same for the alternate NAT-discovery port |
   | `host_port` | `0` | `0` = the embedded match host's configured UDP port (`p2p\p2p_host.ini`, 17760) when it is free, else a free one. A number is used as it is, for tests |
   | `level_gates` | `0` | `1` leaves C1's level gates on playing in place |
   | `players` | `<exe dir>\players` | where the players' folders are |
   | `runtime` | `<exe dir>\p2p\offline` | the run-time folder the lobby shares with the embedded match host |

With the run-time switch off, a build with the flag behaves exactly like one without it: no import
is wrapped, no thread is started, no byte of C1 is written, no socket is opened, no folder is created,
the host gate and Local Play follow their normal rules (`off` cases of
`tests/offline_lobby_fixture.c`).

If offline mode is requested and cannot be provided (no free port can be had, an explicit port is
taken, C1's bytes at the NAT-port site differ, the client was started by a launcher-managed Local
Play session), the client stops with a message instead of silently logging on to the real lobby.

## What happens at start-up (`offline_lobby.c`)

All of it in `TakeOver`, before C1's entry point:

1. **The match host's port.** The embedded sh1x host serves on the UDP port of `p2p\p2p_host.ini`
   (17760 by default) when that port is free on 127.0.0.1, else on a free port the operating system
   picks. The socket that proved the port free is **kept bound** until the host is about to bind it
   (`OfflineHostPortRelease`, one line in `p2p.c` before its own port check), so nothing else on the
   PC - a second offline client in particular - is given the same port between start-up and the match
   launch. `OfflineP2PConfig` writes the chosen port into every `P2PLoadIni` result, which is where
   the armed intent (`p2p_host.txt`, `K33LocalPlayIntent().port`), the launch observation
   (`P2PArmLaunch` / `P2PLaunchObserve` in hostgate's `sendto`), the host's own bind and the lobby's
   START_GAME address all take it from.
2. **The listener on a free port.** `DwOfflineStartServer` (C boundary `offline/dw_api.h`) starts one
   thread running the lobby's `select()` loop (`offline/dw_net.cpp`). The TCP listener binds
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
   `local_multi.c` already uses). In offline mode that slot gets a wrapper that answers a read of
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
7. **The host gate stays on loopback.** `hostgate.c` vouches no lobby name, no default gateway, not
   the SSDP group, and ignores any later vouch while offline mode is active: UDP is exchanged with
   `127.0.0.0/8` only.
8. **Local Play.** `P2PLoadIni` ends in `OfflineP2PConfig`, which points the embedded match host's
   `state` and `profile` folders at the run-time folder, whatever `p2p_host.ini` says, sets this
   run's port, forces `127.0.0.1`, in-process mode and admission, and clears `rollout` / `extra`.
   `P2PCheck` and `P2PInprocRun` accept hosting without the verified script service **only** while
   offline mode is active: the host then compiles the GSC tree installed with the client
   (`codol_scriptPath`).

The lobby thread never calls into C1. It reads the owner's Local Play intent with
`K33LocalPlayIntent()` and raises the host launch with `K33LocalPlayLaunch()` (`p2p_local_play_api.h`,
`docs/P2P_DEFERRED_HOST_START.md`); both are safe on any thread and start nothing themselves. No
intent file is read by anything in offline mode (p2p.c still writes its `p2p_host.<uid>.txt`; nobody
consumes it).

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
  only data shared with another thread is the log file, in `offline_lobby.c`; the Local Play intent is
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
(`offline/tools/gen_resources.py` -> `offline/dw_embedded.cpp`, zlib-compressed, list in
`offline/resources.manifest`) rather than reading the game's zones at run time: the zone readers
(`zones/`, `catalogue/` generators, ~5k lines of Python, plus the fastfile decryption) are only
generators of those tables and the tables are fixed for build 1.1.1.13. Not embedded: the 5.9 MB
`map_spawns.proposed.json` and the weapon tables in the repository root, which only the retired
Python match server used. The only thing read from the install at run time is which map zones exist
(`client_shipRetail\chinese\<map>.ff`), for the map-availability check of the handoff.

### The match handoff

Python: a room start picks a match-server instance, sends `map` over RCON, writes `startgame.txt` and
`mpdata_s.<uid>`, pushes START_GAME at the instance and later folds `match_report.json` back.

Offline: there is no external server, and the policy is the lobby rule of
`docs/P2P_DEFERRED_HOST_START.md` (one function, `RouteMatch` in `dw_rooms.cpp`). On a room start the
lobby reads the owner's intent. Not ticked, failed / stopping / exited / idle, or armed for another
selection than the room's: START_GAME is withheld (and so is SET_MAP), the room stays usable, no
other server is ever substituted. Ready with this selection: hand off to `127.0.0.1:<port>`. Armed
(rule 5), for the owner: write `securitykey.bin` and the admission roster, export the player's
`mpdata_s.<uid>`, call `K33LocalPlayLaunch(owner, session_map, gametype, type)` and, only if it
returns 1, push START_GAME addressed to `127.0.0.1:<port>` - no status probe, no map dispatch. The
port is the intent's, i.e. the match host port of this run. The room's selection is compared with the
intent in the host's own spelling (Survival = gametype `survival` on the MP level; `tdm`/`ffa`/`hq` =
`war`/`dm`/`koth`). After the match the embedded server writes `match_report.json`; the lobby's tick
folds it into the career (XP, rank, challenges, weapon levels, GP for rank-ups), which rewrites
`players\<name>\mpdata_s` and `account.txt` in that same step. The file contracts with the match
server (`securitykey.bin`, `game_admissions.txt`, `mpdata_s.<uid>`, `match_report.json`) are
unchanged, so the server needs no change.

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
| 11 | pushes | relay, SET_MAP (typeId 5 / 15), START_GAME (typeId 3) | real | **ported**; START_GAME follows the Local Play intent |
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
| 80 | pushes | inventory refresh (40), ticker (35) | off by default / needs notices | **ported**; never fire without an operator trigger or notice |
| 102 | 1, 3 | chat channel state, chat send | stored, empty success | as Python |
| - | - | rank write-back from `match_report.json` (XP, rank, GP, lifetime, challenges, weapon levels, co-op, intel) | real | **ported** |
| - | - | website admission (`/admit`, launch tokens), operator status file, wire tap, notices table and its editor, multi-instance placement / spawning, RCON dispatch, SQLite and its migration tools, zone readers and table generators, fastfile writer | real | **not needed offline**, not ported |


## How it was tested

**Differential tests against the Python lobby.** `tests/offline/difftest.sh` starts a fresh private
copy of the Python lobby and a fresh `dwtest.exe` (the same `offline/*.cpp`, built standalone) on
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
`--selftest-career` (115), `--selftest-rooms` (128: the Local Play policy - withheld when off /
failed / idle / other selection / guest / refused launch; armed and ready hand-offs for mp, zm,
survival and co-op rooms). `dwtest --profile <copy of a players folder> --career` prints the careers
of a folder as the lobby reads them.

**Kernel33 side**: `tests/offline_lobby_fixture.c` (15 scenarios: the switch; a free lobby port in
the in-memory route files and in the NAT constant; explicit ports; the profile name and a name that
is not a folder; the players and run-time folders; the gates lifted, kept by the ini, and a call site
that differs; the match host port free, busy, explicit, reserved until released; a refused start
that holds nothing; launcher sessions refused; the Local Play API calls) and the `offline` scenario
of `tests/hostgate_windows_fixture.c`, both in `tests/run_tests.sh`.

**Live** (2026-11-12, own link-based shadow client `D:\CoD\k33work\ofl2\client`, `K33_OFFLINE=1
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

**XP from a real match was not obtained**: the test client's player is driven by posted clicks and
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
* **Survival and co-op matches** live: the rooms open at level 1, but in the shadow used the Survival
  room and the Spec Ops create-room panel show no LOCAL PLAY box, so no match was started there (the
  handoff policy for them is unit-tested only).
* **Store purchase** live (differential-tested).
* **Two offline clients at once on one PC**: the ports are proven free and distinct per listener
  (self-test, and live against dummy sockets on the default ports), but two real clients were not
  started together.
* **A second match in one client run**: unchanged limitation of the in-process host (one hosted
  session per client run).
* **Shipping profile with `K33_OFFLINE=1`** (`K33_LOCAL_TESTING=0`): builds; not run live.
* The LOCAL PLAY label rendered as `@K33UI_LOCAL...` in the shadow used (its zones lack that
  string); unrelated to the lobby.
* After leaving a zombies room, the Spec Ops room list showed that room as still waiting; not
  investigated.


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
   only, admission still required. Without this, Local Play cannot run self-contained at all (no
   `sh1x_host.dll` on this machine exports `Sh1xHost_UseVerifiedScripts`). This is a policy change,
   compiled only with `K33_OFFLINE=1`.
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
7. **LOCAL PLAY stays an explicit tick** in offline mode; a room started without it is withheld with a
   log line and no on-screen message.
8. **Tracked `Kernel33.dll`** was not rebuilt or committed; the offline profile is opt-in.
