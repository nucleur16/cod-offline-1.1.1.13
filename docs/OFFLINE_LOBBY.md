# Offline mode: the lobby inside Kernel33

Branch `k33-offline-lobby`. Status: built and tested offline (unit fixtures and differential tests
against the Python lobby); **not yet run in a live client**. Section "What still needs the live
client" lists exactly what that leaves open.

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

The lobby thread never calls into C1. `OfflineLobbyTick` (main thread) publishes the Local Play
state (enabled, port, room map / gametype / category) into a locked snapshot; the lobby reads the
snapshot. When the lobby answers a room start it tells the tick, which starts the embedded host
through `P2PHostCurrentRoom()` if the player's own click has not already started it. No intent file
is read by anything in offline mode (p2p.c still writes its `p2p_host.<uid>.txt`; nobody consumes it).

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
dw_career*.cpp  rank write-back from match_report.json, challenges, weapon levels, service 4
dw_embedded.cpp generated: the embedded resources
```

Rules of the port (details in `offline/PORTING.md`):

* **No dependency on C1 or Kernel33** anywhere under `offline/`; only `dw_util.cpp`, `dw_net.cpp`
  and `dw_api.cpp` touch the operating system. The same sources build `tests/offline/dwtest.exe`.
* C++11 with `-fno-exceptions -fno-rtti -fno-threadsafe-statics`, as `build-dev.sh` builds the vision
  editor. No exception or C++ object crosses `dw_api.h`.
* **One thread.** The Python lobby is one thread per connection plus a database writer thread and a
  report poller; the port is a single `select()` loop, so there are no locks inside the lobby. The
  only shared data is the Local Play snapshot and the log file, both in `offline_lobby.c`.
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

Offline: there is no external server. On a room start the lobby asks the host for the Local Play
route (`Host::GetLocalRoute`, the snapshot above). If Local Play is ticked it writes
`securitykey.bin` and the admission roster into `state\`, exports the player's `mpdata_s.<uid>` into
`match\`, pushes START_GAME addressed to `127.0.0.1:<host port>` and calls `Host::OnStartGame`. If it
is not ticked there is nothing to join: no START_GAME is pushed and the room stays usable. After the
match the embedded server writes `match\match_report.json`; the lobby's tick folds it into the
profile (XP, rank, challenges, weapon levels, GP for rank-ups) and the next `mpdata_s` fetch serves
the result. The file contracts with the match server (`securitykey.bin`, `game_admissions.txt`,
`mpdata_s.<uid>`, `match_report.json`) are unchanged, so the server needs no change.

## Request inventory

Every `(service, task)` the retail client sent in the captured sessions (430,000 log lines of the
Python lobby, `tests/offline/log_inventory.py`), and what the port does with it. "As Python" = the
Python lobby answers an empty success on purpose and so does the port.

INVENTORY_TABLE

## How it was tested

RESULTS_SECTION

## What still needs the live client

LIVE_SECTION

## Not done

NOTDONE_SECTION

## Decisions to confirm

DECISIONS_SECTION
