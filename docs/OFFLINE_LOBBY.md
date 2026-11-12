# Offline mode: the lobby inside Kernel33

Branch `k33-offline-lobby` (on `main` d319a61). Status: built, tested offline (unit fixtures and
differential tests against the Python lobby) and run once in a live shadow client for a multiplayer
room: login, menus, create room, Local Play, match on the embedded host, write-back. Sections "How it
was tested" and "What still needs the live client" say exactly what was and was not exercised.

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
   | `name` | `Player` | profile name used when the command line has no `+set dw_username` (letters, digits, `_ - .`, at most 31) |
   | `port` | `3074` | loopback TCP + UDP port of the lobby |
   | `alt_port` | `3078` | alternate NAT-discovery port |
   | `root` | `<exe dir>\k33_offline` | the offline store |

With the run-time switch off, a build with the flag behaves exactly like one without it: no import
is wrapped, no thread is started, the host gate and Local Play follow their normal rules.

If offline mode is requested and cannot be provided (the port is taken, C1's bytes at the one
patched site differ, the client was started by a launcher-managed Local Play session), the client
stops with a message instead of silently logging on to the real lobby.

## What happens at start-up (`offline_lobby.c`)

All of it in `TakeOver`, before C1's entry point:

1. **The listener.** `DwOfflineStartServer` (C boundary `offline/dw_api.h`) starts one thread running
   the lobby's `select()` loop. Every socket is bound to a loopback address:
   `127.0.0.1:<port>` TCP + UDP, and the three alternate UDP sockets C1's NAT-type discovery needs
   (`127.0.0.2:<alt_port>`, `127.0.0.2:<port>`, `127.0.0.1:<alt_port>`), exactly the layout the Python
   lobby uses. Connections and datagrams from a non-loopback address are dropped. Loopback binds do
   not raise a Windows firewall prompt.
2. **Routing C1 to it, in memory.** C1 reads `dwauth.ini` (auth host, auth port, lobby host, lobby
   port, title id) and `stun.ini` (up to ten NAT-discovery hosts) with `fopen` at `0x00630CE2` /
   `0x00630D70`; its CRT reaches `CreateFileA` through import slot `0xB421D8` (the slot
   `local_multi.c` already uses). In offline mode that slot gets a wrapper that answers a read of
   those two names with a delete-on-close temporary holding `127.0.0.1 / <port> / 127.0.0.1 / <port>
   / 3ea` and ten `127.0.0.1` lines. The files on disk are neither read nor written, so an install
   that also plays online keeps its real route.
3. **The NAT-discovery port.** C1 sends its probes to `m_natTravPort`, a constant 3074 written by
   `bdNetStartParams`' constructor (`mov ecx, 0xC02` at `0x00A50454`; nothing in the caller at
   `0x00705340` overrides it - MEASURED by disassembly). With the default port 3074 nothing is
   patched. With another port the immediate is rewritten after its nine bytes are verified; if they
   differ offline mode is refused.
4. **The profile name.** C1 derives the player id from `dw_username` alone (Python lobby,
   `docs/admission.md`): the name is the career. If the command line has none, the `GetCommandLineA`
   import slot (`0xB42284`) appends `+set dw_username <name>` from the ini.
5. **The host gate stays on loopback.** `hostgate.c` vouches no lobby name, no default gateway, not
   the SSDP group, and ignores any later vouch while offline mode is active: UDP is exchanged with
   `127.0.0.0/8` only. (The three changed functions are re-pinned in
   `tests/r46_native_source_contract.json`; that pin exists to force a review - see "Decisions to
   confirm".)
6. **Local Play.** `P2PLoadIni` ends in `OfflineP2PConfig`, which points the embedded match host's
   `state` and `profile` folders at the offline store, whatever `p2p_host.ini` says, forces
   `127.0.0.1`, in-process mode and admission, and clears `rollout` / `extra`. `P2PCheck` and
   `P2PInprocRun` accept hosting without the verified script service **only** while offline mode is
   active: the host then compiles the GSC tree installed with the client (`codol_scriptPath`). These
   are the only edits to `p2p.c` / `p2p_inproc.c` (four small hunks).

The lobby thread never calls into C1. It reads the owner's Local Play intent with
`K33LocalPlayIntent()` and raises the host launch with `K33LocalPlayLaunch()` (`p2p_local_play_api.h`,
`docs/P2P_DEFERRED_HOST_START.md`); both are safe on any thread and start nothing themselves. No
intent file is read by anything in offline mode (p2p.c still writes its `p2p_host.<uid>.txt`; nobody
consumes it).

## The lobby (`offline/`)

```
dw.h            core types: tagged values, writers, Reply, Config, Host, Conn, Server
dw_api.h/.cpp   the C boundary and the listener thread
dw_core.cpp     framing, bdAuthTicket, lobby connect, dispatch, STUN, services 12 / 6 / 71 / 102
dw_wire.cpp     the tagged-value codec and the 3DES-CBC frame
dw_crypto.cpp   Tiger/192 and 3DES (no OS crypto, no external library)
dw_net.cpp      the select() loop
dw_json.*       ordered JSON value
dw_store.*      the persistent store
dw_playerdata.* the mpdata / mpdata_s blob codec (structured data defs)
dw_profiles.*   career profiles, rank table, rendering a profile into mpdata_s
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
* C++11 with `-fno-exceptions -fno-rtti -fno-threadsafe-statics`, as `build-dev.sh` builds the vision
  editor. No exception or C++ object crosses `dw_api.h`.
* **One thread.** The Python lobby is one thread per connection plus a database writer thread and a
  report poller; the port is a single `select()` loop, so there are no locks inside the lobby. The
  only data shared with another thread is the log file, in `offline_lobby.c`; the Local Play intent is
  read through p2p.c's own locked snapshot.
* The wire format is the Python lobby's, byte for byte; that is what the differential tests check.
  Its several hundred `CODOL_*` A/B switches are not ported: the port implements the default
  behaviour of `1-START-LOBBY.cmd` + `OFFLINE-DEV.cmd`.

### Transport

A loopback TCP listener with C1 pointed at it was chosen over hooking C1's socket calls: C1 stays
unmodified (one optional 4-byte constant aside), the protocol core can be tested with ordinary
sockets against the Python lobby, and the failure mode is visible (a port that is taken) instead of
subtle. The cost is one listening port on 127.0.0.1.

### Store

`<exe dir>\k33_offline\` (the game folder, not `%LOCALAPPDATA%`): the career travels with the install
it belongs to, is easy to find and back up, and the folder is already writable (the client writes
`main\` and `p2p\` there). `root =` in the ini moves it.

```
k33_offline\
  profile\players\<uid>.json     the career: SAME schema as the Python lobby's players\<uid>.json
  profile\accounts\<uid>.json    wallet, inventory, purchase ledger, login record
  profile\storage\<uid>\<name>   mpdata, mpdata_s, Recentmetlist, blacklist
  profile\kv.json, reports.json  rank write-back bookkeeping, applied match reports
  state\                         securitykey.bin, game_admissions.txt, native_server_rcon.txt
  match\                         the match host's codol_profilePath: mpdata_s.<uid>, match_report.json
  resources\                     optional: a file here overrides its embedded copy
  lobby.log                      the lobby's log of the current run
```

The Python lobby keeps this in SQLite (`resources\lobby.sqlite`) mirrored to files. SQLite is not
linked into Kernel33: the store is a handful of small JSON documents and blobs, each written to a
temporary file and atomically renamed over the old one, cached in memory by the single lobby
thread. Every document carries `"version"`.

**Import of an existing career (planned, not built).** `players\<uid>.json` has the same schema on
purpose: copying a Python-lobby `players\<uid>.json` into `profile\players\` carries rank, XP,
challenges and weapon levels over, and the next login renders `mpdata_s` from it. The class /
character blob is `resources\mpdata.<uid>` there and `profile\storage\<uid>\mpdata` here (same
bytes). Wallet and inventory live only in the Python lobby's SQLite and need a small exporter
(`python -c` over `lobby_db`) writing `accounts\<uid>.json`; that tool is not written. The uid is the
same as long as the same `dw_username` is used.

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
(rule 5), for the owner: write `securitykey.bin` and the admission roster into `state\`, export the
player's `mpdata_s.<uid>` into `match\`, call `K33LocalPlayLaunch(owner, session_map, gametype, type)`
and, only if it returns 1, push START_GAME addressed to `127.0.0.1:<port>` - no status probe, no map
dispatch. The room's selection is compared with the intent in the host's own spelling (Survival =
gametype `survival` on the MP level; `tdm`/`ffa`/`hq` = `war`/`dm`/`koth`). After the
match the embedded server writes `match\match_report.json`; the lobby's tick folds it into the
profile (XP, rank, challenges, weapon levels, GP for rank-ups) and the next `mpdata_s` fetch serves
the result. The file contracts with the match server (`securitykey.bin`, `game_admissions.txt`,
`mpdata_s.<uid>`, `match_report.json`) are unchanged, so the server needs no change.

## Request inventory

Every `(service, task)` the retail client sent in the captured sessions (430,000 log lines of the
Python lobby, `tests/offline/log_inventory.py`), and what the port does with it. "As Python" = the
Python lobby answers an empty success on purpose and so does the port.

| svc | task | what | Python lobby | port |
|---:|---:|---|---|---|
| - | - | auth connection: `BD_AUTH_ACCOUNT_FOR_MMP_REQUEST` -> 128-byte bdAuthTicket | real | **ported**, byte-identical |
| - | - | lobby connection: preamble, `BD_AUTH_SERVICE_ID` -> `BD_LOBBY_CONNECTION_ID`, keepalives | real | **ported**, byte-identical |
| - | - | STUN: IP discovery 30/31, NAT type 20/21 (three change-request variants), keepalive 14/15 | real | **ported** (unit-compared by reading; exercised live) |
| 3 | 17, 19, 24 | team management polls | empty success | as Python |
| 4 | 3, 4 | leaderboard rows | real | **ported** |
| 6 | 1, 5 | mail / message pages | empty success (record shape unknown) | as Python |
| 6 | (instant message) | developer panel `mmbook` / `mmstart` | real (offline dev only) | **ported**; in the product the list holds the local host only |
| 9 | 9, 12, 14, 16, 26 | friends lists, recent-players write | empty success | as Python |
| 10 | 1 | UPLOAD (`mpdata`, `Recentmetlist`) | real | **ported** |
| 10 | 3 | GETFILE `blacklist`, `Recentmetlist`, `mpdata`, `mpdata_s` | real (seeds, per-player store, rendered rank) | **ported** |
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
| 80 | 30, 32, 2, 1 | getPlayerData, getBalance, deposit | real | **ported** |
| 80 | pushes | inventory refresh (40), ticker (35) | off by default / needs notices | **ported**; never fire without an operator trigger or notice |
| 102 | 1, 3 | chat channel state, chat send | stored, empty success | as Python |
| - | - | rank write-back from `match_report.json` (XP, rank, GP, lifetime, challenges, weapon levels, co-op, intel) | real | **ported** |
| - | - | website admission (`/admit`, launch tokens), operator status file, wire tap, notices table and its editor, multi-instance placement / spawning, RCON dispatch, SQLite migration tools, zone readers and table generators, fastfile writer | real | **not needed offline**, not ported |


## How it was tested

**Differential tests against the Python lobby.** `tests/offline/difftest.sh` starts a fresh private
copy of the Python lobby and a fresh `dwtest.exe` (the same `offline/*.cpp`, built standalone) on
private loopback ports, replays captured client byte streams against both (`dwdiff.py`: the raw
bytes the retail client sent, cut out of a Python-lobby log) and compares every decrypted reply and
push. Captures hold real player ids and are not in the repository.

| capture | requests | differ |
|---|---:|---:|
| one login session (2 connections) | 209 | 0 |
| 64 connections: logins, creates, joins, starts, zombies / co-op rooms, purchases, leaderboards | 2210 | 4 |
| the same plus 55 hand-built service-80 requests (deposit, consume, stacking, rentals, refusals) | 264 | 0 |

The 4 (16 in an earlier full run; the count moves with timing) are all `(11, 4)` and are timing, not
content: a join-request storm crossing the lobby's 3 s
relay throttle on two independent clocks (with the throttle set to 0 on both sides the run has 1
difference, an idle connection closed 35 ms apart). Per service on the 64-connection capture: every
`(10, x)`, `(21, x)`, `(80, x)`, `(4, x)`, `(12, 6)` row is 0. Comparison notes: the frame's
messageCount and padding are normalised, getServerTime's seconds are masked, idle keepalives and
idle closes on other connections are ignored, `mpdata_s` is compared inflated with its re-stamped
timestamp and CRC blanked, and a rental expiry may differ by 2 s.

**Rank write-back** (`tests/offline/writeback_diff.py`): 33 synthetic `match_report.json` scenarios
(win / loss, rank-ups paying GP, last rank, lifetime counters, challenges, weapon XP and level-ups,
co-op missions, zombies, intel, duplicates, malformed reports, unknown ids, bots, spool files) run
through the Python lobby's own `rank_writeback` and through the port with both clocks pinned: every
profile field, the wallet, the retry state and the 32768-byte `mpdata_s` buffer are identical. Three
Python bugs are deliberately not reproduced in the product (a report dropped again after a newer
one is credited twice; a row with an unusable collectible is re-credited on every poll; a bad
challenge ref loops) - they are reproduced only under the differential flag.

**Self-tests** in `dwtest.exe`: `--selftest` (Tiger against the client's own digests, 3DES known
answer), `--selftest-store` (188 checks: store round trips, atomic rewrite, playerdata codec against
Python's layout), `--selftest-market`, `--selftest-career` (115), `--selftest-rooms` (128: the Local
Play policy - withheld when off / failed / idle / other selection / guest / refused launch; armed and
ready hand-offs for mp, zm, survival and co-op rooms).

**Kernel33 side**: `tests/offline_lobby_fixture.c` (9 scenarios: switch, in-memory route files,
profile name, NAT port patch and its byte check, host folders, launcher sessions refused, the Local
Play API calls) and the `offline` scenario of `tests/hostgate_windows_fixture.c`, both in
`tests/run_tests.sh`.

**Live** (2026-11-12, own link-based shadow client, `K33_OFFLINE=1 K33_LOCAL_TESTING=1
K33_DIAGNOSTICS=1` build of this branch, `+set k33_offline 1 +set dw_username edtest`; no Python,
no lobby process, no server process, no `p2p_host.ini`; the shadow's `dwauth.ini` / `stun.ini` on
disk pointed at an unroutable address on purpose):

| step | result |
|---|---|
| start | `offline lobby: ACTIVE on 127.0.0.1:3074`; the client logged on to it 25 s after launch; the SSDP datagram was refused by the host gate and caused no visible delay |
| first login of a new profile | key-layout and tutorial prompts, main menu, multiplayer home with the character, challenges, store tiles and 0 CP / 0 GP |
| second run | the stored career is served (level, name); the first-run key-layout prompt does not return |
| Create Room, LOCAL PLAY ticked | room open, intent `enabled=1 state=armed launch=deferred`, nothing started |
| Ready | lobby: `local play: owner launches ... 127.0.0.1:17760`, `Local Play launch raised`; Kernel33: `p2p: match launch (lobby call)`; `server ready after 14625 ms`; the client loaded Killhouse and spawned (`humans=1`) |
| match end (time limit set over loopback rcon) | the embedded server wrote two reports into `match\`; the lobby folded both within a second (`+24 GP (balance 24)`, matches 2, lifetime counters), re-rendered `mpdata_s`; the client came back to the room with the stats panel and the next `getBalance` answered 24 GP |

No verified-script bypass was needed: hosting ran with the committed source (offline mode's own
policy, "Decisions to confirm" 1).


## What still needs the live client

* **XP from kills.** The live match had no bots and no kills, so the folded report carried 0 XP
  (24 GP and lifetime counters). XP / rank-up / unlock write-back is covered by the 33 differential
  scenarios, not by a live match.
* **Zombies, Survival and co-op rooms** live (the handoff policy for them is unit-tested only).
* **Store purchase** live (no GP on a fresh account; differential-tested).
* **A lobby port other than 3074** (the NAT-traversal constant patch; fixture-tested only).
* **The ini switch and the ini profile name** in a real client (fixture-tested; the live run used the
  command line).
* **A second match in one client run**: unchanged limitation of the in-process host (one hosted
  session per client run).
* **Shipping profile with `K33_OFFLINE=1`** (`K33_LOCAL_TESTING=0`): builds; not run live.
* The LOCAL PLAY label rendered as `@K33UI_LOCAL...` in the shadow used (its zones lack that
  string); unrelated to the lobby.


## Not done

* The career import tool for wallet / inventory out of the Python lobby's SQLite (profiles and
  `mpdata` import by copying, see "Store").
* `broadcast.info` (ticker) and `ffotd_client.ff` (pre-match menu unlock) are not shipped; the
  notices table is not ported.
* A committed, anonymised replay fixture: the differential tests need a private capture.
* Multi-player offline rooms: the room code keeps joins and fan-out (the captures need them), but the
  product was built and tested for one local player.
* Friends, mail, clan, chat relay, coupons: answered empty, as the Python lobby does.


## Decisions to confirm

1. **Hosting without the verified script service in offline mode.** `P2PCheck` / `P2PInprocRun`
   accept the installed GSC tree when (and only when) offline mode is active: one player, loopback
   only, admission still required. Without this, Local Play cannot run self-contained at all (no
   `sh1x_host.dll` on this machine exports `Sh1xHost_UseVerifiedScripts`). This is a policy change,
   compiled only with `K33_OFFLINE=1`.
2. **`hostgate.c` re-pin.** `HostGateAllow`, `HostGateOk` and `InstallHostGate` changed (loopback-only
   in offline mode) and were re-pinned in `tests/r46_native_source_contract.json`. `HostGateSendTo`'s
   pin was already stale on `main` d319a61 (the launch observation) and was left alone, so
   `tests/test_r46_native_source_integration.py` fails on that one function on `main` and here.
3. **New accounts start at level 1, and Create Room unlocks at level 5** (client rule). A fresh offline
   profile can therefore only use Matchmaking, which has no LOCAL PLAY box: it cannot host its first
   match. Options: start offline profiles at level 5, ship the `ffotd_client.ff` menu unlock, or give
   matchmaking rooms a Local Play route. The live test raised the test profile's XP by hand.
4. **Store location**: `<exe dir>\k33_offline\` rather than `%LOCALAPPDATA%`.
5. **Default port 3074** (no C1 byte is patched; cannot run next to another lobby on the same PC)
   versus another default with the NAT-port patch.
6. **Not embedded on purpose**: the Python repository's `wallet.json` (balances keyed by real player
   ids) and `purchased_items.txt` (19 legacy purchases it gives every new account). Offline accounts
   start with an empty wallet and inventory plus the default items.
7. **UPnP**: offline mode vouches no gateway and no SSDP group. No delay was seen live; if a client
   is ever seen waiting on UPnP, the alternative is to vouch the gateway as online mode does.
8. **LOCAL PLAY stays an explicit tick** in offline mode; a room started without it is withheld with a
   log line and no on-screen message.
9. **Tracked `Kernel33.dll`** was not rebuilt or committed; the offline profile is opt-in.

