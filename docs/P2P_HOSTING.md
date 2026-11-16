# Player-hosted (P2P) matches through Kernel33

**Offline-only fork (2026-11-13):** this is the design record of the online player-hosting work. What this client
still has is the in-process host on 127.0.0.1 for the built-in lobby (`docs/OFFLINE_LOBBY.md`). The host gate is
loopback only (`HostGateVouch` and every other vouching path were removed), and the intent / descriptor / rollout
files, the launcher startup protocols 1-3 and the verified-script service are gone - see `docs/OFFLINE_LOBBY.md`,
"Removed now that the client is offline only". Statements below about those are history.

Security follow-up: [public-RCE review and host-filter repairs](P2P_PUBLIC_RCE_AUDIT_20261028.md).
Remote player hosts remain unqualified. Lobby authentication is necessary but
does not make host-controlled packets safe for C1's unresolved native parsers.
The newer local-hosting integration and isolated backend prototypes do not clear
those parsers or authorize retiring production dedicated capacity.

Branch `feat/p2p-hosting` (Kernel33, off `origin/dev`) and the server branch of the same name
(`nucleur16/codol_1.1.1.13_alpha_server`, off `origin/main`). Status: stage 1. Stage 1 was built
and measured on one PC (see "Stage 1: what was measured"). Nothing here is installed in a
player's client.

Goal: a player hosts a match from their own client and other players join it. Today every
match runs on the sh1x server (`codol_dedrun.exe`, CoD4x-derived: `src/`, the retained IW3 asm
in `src/asmsource/`, CoDOL code in `src/codol/<area>/`, GSC in `bin/home/codol_rawfiles`). The
Python lobby (alpha0022) places Ready presses on it over loopback rcon and shares
`securitykey.bin` / `game_admissions.txt` with it through a state folder.

## Hard requirement: only lobby-authenticated hosts

**The client must not exchange game traffic with a host the lobby has not vouched for.** This
holds until the audit's open items below are resolved; arbitrary or self-announced hosts are out
of scope until then.

Why: the client security audit (`docs/SECURITY_AUDIT_CLIENT_20261026.md` on Kernel33 branch
`sec/client-audit-20261026`) found no RCE in Kernel33's own code. It could not clear the native
C1 paths that a hostile host reaches first:

| audit row | surface | state |
|---|---|---|
| 14 | `CL_ParseGamestate` configstring index/length | VMProtect-virtualized, **unverified**. This was one of IW4's worst client RCEs (fixed in IW4x). |
| 15 | `CL_ParseSnapshot` entity numbers, areamask | bounds only reasoned about, not proven |
| 16 | native connectionless handlers (infoResponse, statusResponse, connectResponse, print, disconnect) | reasoned about, not reverse-engineered |
| 17 | BD / DemonWare lobby, party, room and relay packets | **not audited**: the largest residual surface |
| 18 | zone / IWD parsing of downloaded content | not audited |

A dedicated server we run is trusted code; a player's machine is not. So every stage keeps the
rule "a host is reachable only after the lobby vouches for it", and stage 1 enforces it in the
client (`src/p2p/hostgate.c`):

- C1 does all its UDP through two import slots, `WSOCK32!sendto` (0xB42538) and
  `WS2_32!recvfrom` (0xB424E0). Kernel33 wraps both before the game starts.
- A datagram from an address that is not vouched is dropped before C1 parses it. A datagram to
  such an address is not sent.
- Vouched means: loopback (this PC); the lobby itself, i.e. the hosts in `dwauth.ini` (lines 1
  and 3) and `stun.ini`, resolved at startup and as C1 resolves them; and addresses added by
  `HostGateVouch()`. Today's dedicated servers run at the lobby's own address
  (`CODOL_MM_HOST_IP`), so they pass.
- Nothing calls `HostGateVouch()` yet. **A player-hosted server on another machine is refused
  in stage 1.** Stage 2 adds lobby-signed host tickets, the only way into that list.
- Only an explicit local-testing build accepts `+set k33_hostGate 1` for log-only
  diagnosis. Shipping always enforces; installation failure stops client startup.
  Every refused address is logged once to `Kernel33.log`.

Before anything wider than "the lobby vouches for the host": verify or replace the row-14 parser
(Kernel33 can pre-validate `svc_gamestate` the way `src/audio/voice.c` re-implements the `"v"` packet),
audit rows 15-17, and refuse downloads (row 18) from player hosts.

## Architecture decision (2026-10-27)

The user chose **in-process hosting** (option (b) below, run like a listen server). The sh1x server runs inside
the host's client on C1's game thread, with no second process or window. Stage 1's child process (option (a))
stays available as `mode = child` in `p2p_host.ini`. The costs listed under (b) are handled as described in
"In-process hosting" below, not by going back to (a).

## Options

### (a) Kernel33 starts the native server as a child of the host's client (recommended)

Kernel33 runs `codol_dedrun.exe` as a child process on its own UDP port and supervises it: a job
object, loopback rcon `sh1x_status`, and an exit with the client. The lobby advertises the host's
address in the session record it already builds (`_common_addr`). Players connect exactly as
they connect to a dedicated server today.

- **Isolation for free.** The server keeps its own address space, heap, CRT and globals. Nothing
  collides with C1.
- **Same binary as the dedicated server.** Every GSC, net and parity fix lands in both. The
  match server does not fork.
- **Already compatible with the lobby.** `match_instances.py` already places players on a server
  it did not start (the rollout file's `extra <port>`). For a remote host it needs an address
  per instance and a remote rcon/handoff replacement (see "Lobby").
- **Cost.** A second process uses about 600 MB of memory (the lobby's `CODOL_INSTANCE_MEM_MB`)
  and loads the zones a second time. The host's PC carries both. That is acceptable for a host
  who opted in; the lobby can refuse hosts below a memory or CPU floor.

### (b) The server as a DLL or static library loaded into C1 (chosen; see "In-process hosting")

Both are 32-bit MinGW/MSVC x86, so a DLL is technically loadable. It would run on its own thread.
It is not recommended for now:

- **Fixed addresses.** The retained IW3 asm (`src/asmsource/*.asm`) assumes its own data layout
  and absolute symbols. A DLL needs relocations, and the asm uses `[abs]` references and
  `section .data` globals that `ld` relocates. Most of it would relocate, but every hand-written
  absolute address and every `jmp [imm]` must be audited.
- **Collisions.** The code collides with C1 at the C level, not by symbol: one process with two
  CRTs (C1's MSVC CRT and MinGW's msvcrt). There would be two `Sys_*` signal/exception handlers
  (CoD4x's `sys_win32_crash.c` installs an unhandled-exception filter and `atexit` hooks;
  Kernel33 already owns a vectored handler), and `Com_Error` uses longjmp across the frame. Any
  `exit()`/`ExitProcess` in the server (`sys_win32.c:972`, `:1030`, `_exit(3)` in the crash
  path) would kill the player's client. A "library mode" would have to turn every one of those
  into a thread exit or an error return.
- **Memory.** The server's hunk/zone allocations and its zone loader (`db_load.cpp`, the
  1.1.1.13 zones from `fs_basepath`) would share C1's 2 GB 32-bit address space. C1 already holds
  1.2-1.6 GB in a match (the DMP files in k33-devtest are about 160 MB of *dump*; the working set
  is far larger). Two copies of the zones in one 32-bit process will not fit without
  `/LARGEADDRESSAWARE` and a careful budget.
- **Crash coupling.** A server fault would take the host's client down.

Revisit (b) only if a second process turns out to be the blocker on low-end hosts. The server
would then need a `codol_library` target with no `exit`, no global handlers, a host-provided
filesystem root, and a tick entry point driven by Kernel33.

### (c) C1's own listen-server code

C1 still has a listen server: the room's developer panel offers "Start LS", which runs
`devmap <map>` in the client itself (`docs/offline_dev.md` in alpha0022, `dw_devpanel.py`), and
`SV_AddTestClient` exists at 0x5D26E0. That is the stripped retail IW4-style server: no bot AI
(ERRORS.md, `BOT-DIFFICULTY-SELECTION-IS-THREE-PIECES`), and none of the sh1x server's GSC VM
work, killstreaks, zombies or co-op. Its scripts are the zone's compiled GSC, not
`codol_rawfiles`. Using it would mean re-doing in C1 everything the sh1x server already does. It
also gives up the authority split: the host's client would *be* the server, in the same process
as an unaudited client. Not recommended. It is still worth one measurement (does "Start LS"
reach a playable map?) as a fallback for very small hosts.

**Recommendation: (a).** It is the fastest working P2P and keeps one server codebase. It keeps
the process boundary between an unaudited client and the authoritative simulation, and it leaves
(b) open later.

## Ports and NAT

- The hosted server listens on UDP port 17760 by default (`p2p_host.ini` `port=`).
  17733-17741 stay for dedicated servers and lobby-spawned instances. Stage 1 refuses a port that
  is already bound (exclusive bind test before starting).
- **UPnP/NAT-PMP (stage 2).** Kernel33 maps the port on the router (IGD `AddPortMapping`, lease
  renewed while hosting, removed on stop). C1 already does DemonWare NAT detection (STUN to
  `stun.ini`, the lobby answers it). The host reports the mapped address and NAT type to the
  lobby with its registration.
- **Hole punching (stage 3).** C1's bdNATTrav (INTRO REQ type 13 / reply 12, already answered by
  the server, `codol_network.c`) is DemonWare's introducer protocol. The lobby can introduce the
  joiner and the host to each other (both send to each other's mapped address after the lobby's
  "introduce" push). That works for cone NATs.
- **Relay fallback (stage 3).** For symmetric NAT or CGNAT hosts, the lobby (or a relay sidecar on
  the VPS) forwards UDP between joiner and host: the joiner connects to `<vps>:<relay port>`, which
  the hostgate already trusts because it is the lobby's address. That costs latency and VPS
  bandwidth, but it is always available. A host whose port test fails is listed as "relayed".

## Lobby and matchmaking changes

Stage 1 changes nothing in the lobby: a lobby on the same PC (`CODOL_OFFLINE=1`) places players
on the hosted server through the rollout file Kernel33 writes. Remote hosts need:

1. **Host registration.** The host's client asks the lobby to host over its authenticated lobby
   connection (a new instant-message verb next to `mmbook`, or the launcher's HTTPS admission
   channel). It sends its port, mapped address, NAT result and build hash. The lobby probes the
   address (a `getstatus` or a bdQoS probe from the VPS), then records the host as an instance
   with its own address. `match_instances` today assumes `MM_HOST_IP` for every port; it needs
   `addr` per instance, and `_session_search_record(ds=...)` already takes a per-row address.
2. **Host ticket (the vouching).** On every placement onto a player host the lobby signs
   `{host ip:port, session id, expiry, joiner uid}` with its Ed25519 key (alpha0022 already holds
   `jwt_ed25519_public.pem`). It delivers the ticket to each joiner over the lobby connection.
   Kernel33 verifies the signature against a compiled-in public key and only then calls
   `HostGateVouch(host ip)` for that session. The vouch expires with the session.
3. **Admission keys / securitykey.** Today the server reads the 16-byte session keys from files
   the lobby writes in a shared folder. A remote host has no shared folder, so the lobby pushes the
   admission roster (seat uid -> bdSecurityKey candidates, the content of `game_admissions.txt`)
   to the host's Kernel33 over the authenticated lobby connection. Kernel33 writes it into the
   server's state folder; the server already re-reads both files. The host's own client generates
   the room's bdSecurityKey (createSession), so the host-side key is local anyway.
4. **rcon-free handoff.** The lobby cannot rcon a remote host (rcon is loopback-only by design,
   and an rcon password on the internet is a liability). The lobby sends the match selection
   (map, gametype, zone type, session map: the `startgame.txt` record) to the host's Kernel33 the
   same way. Kernel33 hands it to its child over loopback rcon, as stage 1 already does for
   status. The lobby's poll of `sh1x_status` becomes a status push from the host's Kernel33.
5. **Results.** Match reports (`rank_writeback`, the server's report spool) come from an untrusted
   host and must not be accepted as authoritative XP/GP. Stage 2 options: no progression on player
   hosts; or progression capped and cross-checked against the joiners' own client-side reports.
   The lobby stays the only writer of careers.

**The lobby VPS** keeps running dedicated servers and stays the default. Player hosting is an
extra row type in the room/server list ("hosted by <name>"). The VPS also provides the STUN,
introducer and relay roles above.

**Offline mode (`CODOL_OFFLINE=1`)** needs no tickets. The lobby is on 127.0.0.1, the host is on
the same PC, and hostgate already trusts loopback. Stage 1 is exactly this setup: the offline
lobby's state folder is the hosted server's state folder.

## Authority and anti-cheat

The host runs the authoritative simulation on their own machine. They can change the binary,
GSC, dvars or memory, so a player-hosted match is only as fair as its host. Mitigations, in order
of cost:

- A host's build hash is reported at registration. The lobby lists unknown builds as "modified"
  or refuses them. That is a weak check: a hostile host can lie.
- No ranked progression from player hosts (see Results). Player hosts are for private and custom
  matches.
- The joiner's safety does not depend on the host being honest. The hostgate, plus the audit's
  parser work, protect the joiner's machine. That protection is the non-negotiable part.
- Host migration is out of scope. When the host leaves, the match ends (the server follows its
  client: job object plus `codol_hostParentPid`) and players return to the lobby.

## Staged plan

| stage | what | lobby change | status |
|---|---|---|---|
| 1 | Kernel33 hosts the server as a child (`src/p2p/p2p.c`), the server follows its host (`codol_hostParentPid`), the client talks only to vouched hosts (`src/p2p/hostgate.c`), and the same-PC lobby places players through the rollout file | none | **this branch** |
| 2 | host registration, UPnP, signed host tickets -> `HostGateVouch`, admission roster and match selection pushed through the lobby connection, status push, no ranked progression on player hosts | yes | next |
| 3 | lobby introducer (bdNATTrav) and the UDP relay on the VPS; room-list rows for hosts; host quality floor | yes | later |
| 4 | gamestate pre-validation in Kernel33 (audit row 14), snapshot and OOB audit (15-16); only then hosts outside lobby vouching, if ever | no | blocked on audit |
| - | (b) in-process server; host migration | - | not planned |

Main risks: the unaudited native parsers (above); host machines too small to run client and
server together (measure memory on low-end hosts); NAT success rates without a relay;
progression abuse; and stage 2's key push. The stage 2 push must be authenticated end to end:
anyone who can forge it can admit themselves.

## Stage 1: what it does

`src/p2p/p2p.c` (included in every build; inert until asked):

- `p2phost [map] [gametype] [zonetype]` starts hosting, `p2pstop` stops (rcon `quit`, then the
  job), `p2pstatus` prints the state, address, pid, ready time and last `sh1x_status` line.
  `+set k33_p2pHost 1` on the command line starts hosting while the client loads.
- Settings come from `<exe dir>\p2p\p2p_host.ini` (`key=value`). The defaults keep everything
  under `<exe dir>\p2p\`. Keys: `exe`, `basepath`, `home`, `scripts`, `state`, `profile`,
  `rcon_file`, `map`, `gametype`, `zone_type`, `session_map`, `net_ip`, `port`,
  `bots`, `bot_difficulty`, `admission_required`, `developer`, `ready_timeout`, `extra`. Map,
  gametype and difficulty must be plain identifiers, the same rule as the lobby handoff, so they
  can never add console commands.
- The server runs with `CREATE_NO_WINDOW` in a job object with `KILL_ON_JOB_CLOSE`, plus
  `+set codol_hostParentPid <client pid>`. The server branch makes it quit cleanly once that
  process ends.
- The rcon secret is shared with the lobby through `<state>\native_server_rcon.txt` (read, or
  created with 16 random bytes). The host thread polls `sh1x_status` over loopback. (It used to write
  `<state>\p2p_host.txt` and, with `rollout=`, the line `extra <port>` into the lobby's
  `match_rollout.txt`; both were removed with the online paths.)

`src/p2p/hostgate.c`: see the hard requirement above.

### Setup: the server submodule

The server is a git submodule of this repo at `server/sh1x`
(`git@github.com:nucleur16/codol_1.1.1.13_alpha_server.git`), pinned to the server's
`feat/p2p-hosting` commit. Its sources are not copied here. `scripts/build.sh` runs
`git submodule update --init server/sh1x` (not recursive). The server's own submodules: only
`bin/home/codol_rawfiles` (CODOL-EMU/13-rawfiles) is needed, and only to stage a hosted server.
`plugins/cod4x_mysql_plugin` is a gitlink with no URL; a bare recursive update stops on it, so
never use `--recursive`.

```sh
git submodule update --init server/sh1x
git -C server/sh1x -c core.autocrlf=false submodule update --init -- bin/home/codol_rawfiles
git -C server/sh1x/bin/home/codol_rawfiles config core.autocrlf false
sh scripts/build-server.sh            # WSL Ubuntu-24.04 + i686-w64-mingw32, as the server's own docs
```

`scripts/build-server.sh` cross-builds `server/sh1x/bin/codol_dedrun.exe` in WSL (clean `obj/k33-p2p`
when asked with `K33_SERVER_CLEAN=1`). It stages the exe as `k33p2p_dedrun.exe` (its own image name, so an
image-name kill of `codol_dedrun.exe` aimed at a dedicated server cannot hit a hosted match; also p2p.c's default `exe=`), the server pack (the server's own
`tools/codol_stage_server_pack.py`) and the rawfiles into `$K33_P2P_STAGE` (default
`../../k33_p2p_staging`). The lib/*.a the server links (mbedtls, tomcrypt) come from the server
repo's own build (`K33_SERVER_LIBS` = a folder that holds them, default `D:\CoD\CoD4x_Server\lib`).

### Stage 1: what was measured

Measured on 2026-10-26/27 on one PC (24 logical CPUs). The setup:

- `D:\CoD\CODOL Builds\k33-p2ptest`: a hardlinked copy of the 1.1.1.13 install with this branch's
  `Kernel33_dev.dll` (local-testing profile).
- A copy of the offline lobby (alpha0022 at the offline worktree's HEAD, `CODOL_OFFLINE=1`) on
  127.0.0.1:3174 with its own state folder. `CODOL_INSTANCE_PORTS=17761` had nothing listening, so
  only the rollout registration could make it place players on 17760. The map was pinned to MP
  (`CODOL_FORCE_MAP=mp_killhouse_sh`).
- The server was built from `server/sh1x` and staged as `k33p2p_dedrun.exe`.

The user's lobby on 3074, the live server on 17733 and the user's own client were not touched.
C1 sends STUN to the stun.ini host on the fixed port 3074, so the test clients' STUN probes did
reach the user's lobby; those probes are stateless.

| check | result |
|---|---|
| host client started with `+set k33_p2pHost 1` | the server started about 6 s after the client; first `sh1x_status` 33.9 s after the start (cold disk), 23.0 s and 10.6 s on later starts |
| registration | `extra 17760` in the lobby's `match_rollout.txt`; the lobby's `lobby_status.json` showed 17760 answering |
| host joins its own server | Ready in a room: the lobby logged `instances: :17760 is free -- switching mp_killhouse_sh/war -> mp_killhouse_sh/combat` and RCONed it; the host client connected (`p2phost num 2 from 127.0.0.1:30002`) and played about 9.5 minutes (snapshots, splashes, PARITY lines) until the match ended and the server restarted the map |
| second client joins | client B (`p2pjoin`, second local instance): the lobby logged `instances: joining :17760 mp_killhouse_sh/combat (1 other player(s))`; B connected (`num 3`); then both played together, rcon `status` sampled every 10 s for 60 s: both players active, ping 6-22 ms, last message 0-34 ms |
| resources while both played | host client 1.38 GB working set / 1.79 GB private, 4 % CPU; joiner 1.50 / 1.77 GB, 4.5 %; hosted server 0.53 GB working set / 1.32 GB private, 0.9 % CPU. A second copy of the server's 1.3 GB private memory cannot sit inside C1's 32-bit address space, which is why option (b) waits. |
| `p2pstatus` | printed state, address, pid, ready time and the live `sh1x_status` (humans=2 bots=2) |
| `p2pstop` | the server quit with exit code 0 in 0.9 s; `extra 17760` and `p2p_host.txt` removed |
| host client killed (hard) | the job object ended the server at once (gone within 3 s). The rollout line and descriptor stay behind, because the host thread dies with the client; the lobby drops a silent instance after 10 s, and the next `p2phost` rewrites the line |
| `codol_hostParentPid` (server branch) | a server started by hand with its parent set to a throwaway process quit cleanly (`host client ... has ended; quitting`, exit code 0) 0.9 s after that process was killed |
| server fatal error | the lobby switched the hosted server to `zm_zombie_training` (an unpinned room pick): `script compile error, unknown function (maps/_zombiemode_tokens.gsc line 139)`. The process stays alive with its network shut down; the supervisor now treats 20 s without `sh1x_status` after ready as failed and ends it. The compile error itself is a server/rawfiles issue at `origin/main` 32da84d5 + 1c10bb3, not a P2P one |
| hostgate | both clients ran enforcing all session. Datagrams from 192.168.1.4 (this PC's LAN address, not vouched) to both clients' game ports were refused before C1 saw them (`hostgate: refused datagram from 192.168.1.4:55282`). The lobby, STUN and the hosted server (loopback) worked. The first build also refused UPnP (SSDP 239.255.255.250:1900), which delayed DW login by about 70 s while bdUPnP discovery timed out; SSDP and the default gateway are vouched now and login starts 11 s after launch |

Not measured: a host on another machine (refused by design until stage 2), NAT and UPnP mapping,
the release build (`scripts/build.sh`) in a client (it compiles and links), and a host machine with less
memory.

## In-process hosting (branch `feat/p2p-inprocess`)

`src/p2p/p2p_inproc.c` plus the server's host API (server branch `feat/p2p-inprocess`). `mode = inprocess` is the default.

Since `feat/server-in-kernel33` the server is linked into Kernel33.dll and kept for the client's run: the DLL
loading, the per-session fresh globals and the fault containment described below are history. See "The server
linked into Kernel33.dll" further down.

### How it runs
- **Server build.** The server repo's makefile target `host-dll` links the dedicated server's objects as
  `sh1x_host.dll`. Its API (`src/win32/sys_host.c`):
  - `Sh1xHost_Init(cmdline)`: Sys_Main's start-up without the loop.
  - `Sh1xHost_Frame()`: one `Com_Frame`.
  - `Sh1xHost_Command(text)`.
  - `Sh1xHost_ExitCode()`.
- **Loading.** Kernel33 copies the DLL to `%TEMP%\k33_sh1x_host_<pid>_<n>.dll` and loads that copy, so each
  session gets fresh globals. The client's own import table is untouched, and the hostgate does not see the
  server's sockets: the server has its own Winsock imports.
- **Ticking.** The server is ticked on the game thread from C1's `Com_EventLoop` call (0x595E9D, chained behind
  voice and discord). It is also ticked, throttled to every 2 ms, from C1's `Sleep`, `SleepEx` and
  `PeekMessageA` import slots.
  - The extra tick sources are required. Without them, C1's own map load (`DB_LoadXFile`, 22 s on
    mp_killhouse_sh) and its connect phase starve the server.
  - The first test without them failed: the host's NAT-traversal handshake to its own server timed out
    ("Unable to connect to host").
  - With them, the longest gap between ticks while the host loaded the map and connected was 745 ms. No gap
    exceeded 1 s.
- **Stack.** The server runs on its own fiber (32 MB reserve). C1's main thread reserves 640 KB, and one server
  frame overflowed a 2 MB stack in a harness (0xC00000FD).
- **Floating point.** The fiber is created with `FIBER_FLAG_FLOAT_SWITCH`, so the server's `Sys_SetFloatEnv`
  stays on its own fiber. The tick also restores the client's x87 control word and MXCSR.

### Containment

| cost | how it is handled | measured |
|---|---|---|
| exit / ExitProcess / Sys_Error | the host API turns every engine exit into an end of the server: a longjmp back into the running `Sh1xHost_*` call (server repo change) | `p2pstop` ended the server in 1.75 s and freed udp/17760; the client stayed responsive. A `Sys_Error` (PMem_Init, see below) ended only the server |
| global exception handlers | the hosted server never installs its crash filter (only WinMain does). Kernel33's vectored handler catches AV, stack overflow, illegal or privileged instruction and integer divide raised on the server fiber in sh1x_host.dll or msvcrt.dll, moves execution onto a rescue stack and switches back to the client's fiber | `p2pfaulttest` (test clients only) made the server read address 1 in `Cbuf_AddText`. Result: `server fault 0xC0000005 ... contained`; the client kept running and logging |
| watchdogs, console window, error dialog | hosted: no shutdown or load watchdog, which would `_exit` or interrupt the game thread; no win32 console window; no 60 s error dialog wait; `NET_Sleep` polls | no window or extra process seen; the server listens inside the client's pid |
| globals | each session is a fresh copy of the DLL's .data/.bss | first session ran 20 min |
| heap | the server allocates through msvcrt.dll and VirtualAlloc, never C1's static CRT heap | - |
| address space | `codoMP_client_shipRetail.exe` already has LARGE_ADDRESS_AWARE (characteristics 0x122), so the process gets 4 GB on 64-bit Windows; no change was needed | see memory below |

### Measured (2026-10-27, k33-p2ptest, offline lobby copy on 3174, mp_killhouse_sh)
- **Startup.** `+set k33_p2pHost 1`: `Sh1xHost_Init` took 2.3 s on the game thread. The server answered
  `sh1x_status` 17.5 s after the session started (the client was loading in parallel), and the lobby
  registration worked as in stage 1.
- **Host joins.** The host pressed Ready; the lobby switched the in-process server to `combat`. That switch is
  one 4.6 s server frame on the game thread. The host connected (`p2phost num 2`).
- **Second client joins.** A second local client (`p2pjoin`) joined through the lobby
  (`joining :17760 ... (1 other player(s))`).
- **Both playing.** rcon `status` every 10 s for 90 s: both active, ping 8-14 ms, last message 0-34 ms.
- **Game-thread cost of the server**, over 30 s windows:

  | state | mean per tick | max per tick | ticks per 30 s |
  |---|---|---|---|
  | idle, at the menu | 30 ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Âµs | 6-9 ms | about 38,000 |
  | 2 players + 2 bots | 184-270 ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Âµs | 7-29 ms | 8,000-16,000 |
  | map loads | - | 4.6-5.6 s (one frame) | - |

- **Memory of the host process, in a match with 2 players**: private 3,113 MB, virtual 3,671 MB of the 4,096 MB
  that LARGE_ADDRESS_AWARE allows. The joiner (no server) used 1,777 MB private and 2,266 MB virtual. At the
  menu, the session took the host from 160 MB to 2,366 MB private (the client was loading at the same time).
  - The server's share is about 1.3 GB:
    - .bss, about 323 MB (IW3 static arrays);
    - PMem, a 328 MB block committed at once;
    - hunk and zone memory, and the assets it loads itself.
  - **Headroom is about 400 MB.** A larger map than mp_killhouse_sh can exhaust the address space.
- **Re-hosting in the same client fails.** After `p2pstop`, a second `p2phost` failed cleanly:
  `Sys_Error: PMem_Init(): not enough virtual memory within a single block available` (contained). The first
  session's DLL copy, its 328 MB PMem block and its hunk are not released. Hosting again currently needs a
  client restart.

### The server linked into Kernel33.dll (branch `feat/server-in-kernel33`, 2026-11-16)
The match server is part of Kernel33.dll: no `p2p\sh1x_host.dll`, no DLL copy in `%TEMP%`, no `LoadLibrary`. The
server repo's makefile target `host-lib` (server branch `feat/server-in-kernel33`) builds `bin/libsh1x_host.a`;
`scripts/build.sh` links it and `src/p2p/p2p_inproc.c` calls the host API directly (`src/p2p/p2p_server_api.h`).
This replaces the first take on this branch (the DLL image packed into Kernel33.dll's resources) at the user's
request; the three costs that take was avoiding are handled as below.

**One server per client run, matches change on it.** The server cannot be initialised twice (its globals are spent
once it has ended), so it is started once, by the first match the player hosts, and kept for the rest of the run:
- First match: `Sh1xHost_Init` with the full command line (`+map`), as before.
- A match ends without ending the server. When it is over (offline: its level ended and its player left,
  `PiMatchOverPoll`) the finished level simply stays loaded, as a one-shot lobby worker keeps it; the player's
  `p2pstop` unloads it with `killserver` (the server then "stays down until the next map command").
- Next match: the same server. Its settings and map are queued with `Sh1xHost_Command`:
  `set codol_zoneType`, `set codol_sessionMap`, `set g_gametype`, `set g_difficulty`, `map <map>` (the latched
  dvars apply at that map load), as the lobby retargets a warm dedicated worker. The match's state goes to
  running only after the frame that ran the `map` has returned, so a status of the previous level never counts.
- Settings that are on the server's command line (port, address, paths, rcon file, admission, developer, PMem,
  C1 zone sharing, `extra`) cannot change: a match that asks for others is refused with "restart the client".
- The server keeps its port, sockets and threads between matches and is still ticked; the offline lobby no longer
  re-reserves the port after a match (`P2PInprocServerAlive`).
- Only an end of the server itself (a `quit`, a `Sys_Error`, a changed frame hook) is final for the run: the next
  host then says "restart the client to host again" (`P2PInprocUsed`).
- `p2p_inproc_retire.c` (the import-table tracking and unloading of a spent DLL copy) is gone: there is nothing to
  unload.

`p2p_inproc.c` keeps two states: `g_piServer` (not started / running / ended, for the run) and `g_piState` (the
current match). The monitor loop and its exit classification are unchanged (`tests/test_p2p_inproc_lifecycle.py`
still extracts them verbatim).

**Memory: the large state is allocated on the first host.** The server's .bss was 217 MB (MSYS2 build;
`svs` alone 159 MB). Linked as it was, every client would have reserved and committed it for its whole run, hosting
or not. Server `src/sys_ondemand.h` turns the 16 largest arrays into pointers to arrays that `Sys_OnDemandInit`
(start of `Sys_MainInit`) allocates; their uses are unchanged (`#define name (*sh1x_od_key)` keeps indexing, `&`
and `sizeof`):

| array | MB | array | MB |
|---|---|---|---|
| `svs` (sv_main.c) | 158.6 | `codol_pmtrace` | 2.4 |
| entity-state snapshot / archive extensions | 10.5 + 1.0 | script string freed-text log (`sl_freedText`) | 1.5 |
| player-state archive / frame extensions | 6.3 + 3.1 | script memory tree (`mt`) | 1.1 |
| async capture streams | 3.2 | bot A* heap / nodes | 1.1 + 0.9 |
| client-state snapshots / archives | 2.7 + 1.1 | client stats, skeleton memory | 1.0 + 1.0 |
| actor animation journal | 2.5 | | |

None of them is referenced by the retained assembly. The rest of the server's .bss is 19.4 MB (largest:
`odeGlob` 2.8 MB and `level_bgs` 0.7 MB, which the assembly defines; `g_entities` 1.2 MB; then a tail under 1 MB).

| Kernel33.dll (shipping profile) | file | SizeOfImage (address space it reserves) | .bss |
|---|---|---|---|
| before (main, server as a separate DLL) | 14,264,320 | 17.0 MB | 3.4 MB |
| server packed as a resource (first take) | 16,563,712 | 19.2 MB | 3.4 MB |
| server linked, its .bss as it was (computed) | about 19.2 MB | about 258 MB | about 221 MB |
| **server linked, large state on demand** | 19,189,760 | 41.0 MB | 22.7 MB |

So a client that never hosts reserves 24 MB more than before, not 241 MB. The first host allocates 202,631 KB
(`Sh1xHost_OnDemandBytes`, logged after each match) and keeps it for the run, like the hunk and zone memory.

**Symbols.** `host-lib` links the server's objects and its mbedtls/tomcrypt archives into one relocatable object
(`ld -r -d`) and makes every symbol local except the host API and the COMDAT sections
(`tools/sh1x_host_lib_symbols.py`; 10,395 local, 64 global). Without that, 58 of the server's globals clash with
Kernel33's static libraries (its own zlib, `MatrixMultiply`), and some would silently replace a library function
for all of Kernel33 instead of clashing (the server defines `__cxa_atexit`). Its `.drectve` (the CoD4x plugin API
exports) is dropped: Kernel33.dll exports nothing of the server.

**Compiler, CRT and allocator.** `scripts/build-server.sh` builds the library with MSYS2's mingw32 gcc 14 (the
one Kernel33 uses) and MSYS2's make and nasm, not the WSL cross gcc 13-win32: the objects, libstdc++ and libgcc
then agree (the WSL compiler's win32 thread model calls `__gthr_win32_*` functions that MSYS2's posix-model libgcc
does not have). Both are msvcrt.dll builds: the server's `malloc` is Kernel33's (C1 keeps its own static CRT heap),
its large blocks are VirtualAlloc. The server adds `-lcrypt32 -lwsock32` to the link.

**Loader lock.** The server's static initialisation in Kernel33's DllMain is one constructor (an empty
`std::vector<std::string>` in stringed_ingame.cpp, plus its `atexit`); it has no static TLS (`.tls` is the CRT's
own 8 bytes). `Sys_InitThreadContext` (its `TlsAlloc`), the on-demand allocation and everything else run in
`Sh1xHost_Init`, on the game thread, at the first host.

**Faults are not contained any more** (user decision: "the crashes as part of kernel33 are fine it'll make it
easier to debug when it's all in one place"). The vectored handler that moved a server fault onto a rescue stack
(it recognised the server by its separate image) and the test command `p2pfaulttest` are removed; the activation
transaction keeps its handler step as a no-op so its order holds. A fault in the server is a fault of the client:
Kernel33's `FaultHandler` logs it (diagnostic profile: the `AV ... eip=` lines with module offsets, now all in
Kernel33.dll) and the exception reaches C1's unhandled-exception filter (BugTrace dump) through the fiber's SEH
chain. The server never installs its own crash filter (only its WinMain does). The `longjmp` that turns a server
exit or `Sys_Error` into an end of the server is unchanged.

**Test command.** `p2psv <command>` (test clients only, `+set k33_cmdFile 1`): one console command for the running
server, queued with `Sh1xHost_Command` on its fiber (C1 splits at `;`: one command per `p2psv`).

**Measured** (2026-11-16, own full copy of k33-uistyle-offline-test without `p2p\sh1x_host.dll`, test-profile DLL,
headless `+set k33_cmdFile 1 +set codol_local_test_multi 1`, below-normal priority):
- At the menu, never hosted: 1,173 MB private, 1,860 MB virtual.
- `room.create` (a co-op room), `k33ready 1`: `starting the match server linked into Kernel33.dll (match 1, the
  first of this client run)`. The lobby stopped that start during its map load and relaunched (`C1's first
  datagram to the armed port`): `reusing the match server started 8 s ago (match 2)`, ready after 9.0 s, the player
  connected. C1 then dropped with `ERROR: 181 aa12_mp 0 2` while loading the gamestate: the relaunch asked for
  category `mp` for this co-op room. Not followed up: it is the lobby's relaunch, and the same configuration on a
  fresh server was not tried.
- `room.leave`, `room.match.cyborg`, `k33ready 1`: `reusing the match server started 78 s ago (match 3)`, ready
  after 5.8 s; the player played zm_factory_sh (round 1).
- Same server, multiplayer with bots: `p2psv set codol_zoneType mp`, `p2psv set codol_sessionMap
  mp_killhouse_sh`, `p2psv set g_gametype war`, `p2psv set scr_codol_bots 5`, `p2psv map mp_killhouse_sh`. The
  session followed the map change, and the player loaded Team Deathmatch on Killhouse with the Training bots.
- `p2pstop`: the match ended with `killserver`, and the server stayed up. The player was dropped ("connection
  timed out"), which closed the room.
- `k33ready 1` once more: match 4 on the same server, ready after 5.5 s. The lobby then stopped it because the
  room was gone.
- `starting the match server` appears once in the run. Address space after the multiplayer and zombies loads:
  3,378 MB of 4,096.
- Not done: two natural match-overs in a row (a zombies death, then Ready again with the player joining). The
  next client launches failed in Direct3D device creation (the display was not available) before the room step.

**Follow-up (same day): category, stop and back-to-back matches.**
- **The co-op `181 aa12_mp 0 2` is not specific to reuse.** The two launches of that test asked for different
  selections. The first was `mp_crash_tropical coop (coop)`, from the room UI's `p2p_room`. The second was
  `(mp)`, from the offline lobby's `K33LocalPlayAuto`: `dw_rooms.cpp HostSelection` takes `sel.category`, and a
  room made with the bare `room.create` test command has category `mp`. Re-arming for the new selection is what
  stopped match 1 (`P2PArmLegacyRoom`: `g_p2pStop`).
  - A fresh server given gametype `coop` with category `mp` resolves the same set
    (`SV_CoDOLResolveAssetCategory`): `+set codol_zoneType mp` is the default, so it is not taken as explicit,
    and an `mp_*` name gives `mp`.
  - The reused server took `mp` from the latched `codol_zoneType`. Both load the mp gametype zones under the
    co-op gametype, which C1 rejects.
  - The room UI's real co-op rooms carry `coop` on both sides.
- **A real reuse bug was found on the way and is fixed.** The resolver takes `codol_zoneType` as this match's
  only while its latch differs from the running value. A second co-op (or survival) match in a row therefore
  sent nothing explicit and resolved as `mp` from its `mp_*` level name. The retarget now starts with
  `set fs_game <category>`, the one-shot carrier the lobby itself sends before `map`
  (`FS_CoDOLNormalizeGameDir` / `FS_CoDOLConsumeGameCategory`).
- **The new match counts as running only once a new level is up.** The fiber queues the retarget once, then
  waits until `Sh1xHost_LevelSerial` (levels spawned, server `sv_main.c`) has moved. Before, it was enough that
  the frame that ran `map` had returned. This matters because the replaced level can be the same map, finished
  (one-shot), and a map change can wait for the previous match's report to become durable. A stop before the
  new level is up ends the match and keeps the server.
- **`p2pstop` returns the players to the lobby.** Before unloading the level, the fiber calls
  `Sh1xHost_ReturnPlayersToLobby`. This flushes the secure profiles and sends every human in the game
  `t "EXE_MATCHENDED"`, the leave a finished match sends (`codol_script_exitlevel_trace.c`). The server then
  keeps running frames for 1.5 s and only after that sends `killserver`.
  - Before this change, the player was dropped without a usable message: "connection timed out" 22 s later,
    which also closed the room.
  - The old per-session `quit` went through `SV_FinalMessage` the same way, so it was no cleaner.
- **More state on demand.** `physics_launches`, `s_animlist_refs`, the bot nav cut records and the capture
  publications (1.5 MB) are now allocated on demand too. The server's remaining .bss is 17.9 MB. Shipping
  Kernel33.dll: 19,189,760 bytes, SizeOfImage 39.5 MB, .bss 21.2 MB.
- **Live.** The client could create its Direct3D device again, but only at a 1,024 x 768 fallback. It
  presented no frames: screenshot requests stayed pending and the main thread sat in C1's inactive-window
  `Sleep` loop. The display was likely off or locked; two other game clients were running, possibly the
  user's. Results:
  - The first host started the server: ready after 10.7 s, `Sh1xHost_Init` 3.7 s.
  - C1 never connected. After 120 s the empty-host rule ended the match, and the server stayed up
    (204,163 KB on demand).
  - Not tested live: two natural match-overs in a row, `p2pstop`'s return to the lobby, and the `fs_game`
    retarget. These need a client that presents frames.

### Not done yet / next steps
1. **Reuse the zones and assets the client already loaded.**
   - Not done. The server's loader (`db_load.cpp`) builds IW3-layout xassets in its own memory. C1's are
     IW4-layout structures in C1's pools.
   - Sharing them means pointing the server's asset accessors (`DB_FindXAssetHeader` users: weapons, models,
     collision maps, rawfiles, string tables) at C1's loaded pools through layout adapters. That is a large
     reverse-engineering job.
   - The cheaper first win is memory: size PMem and the hunk to what the hosted map needs, instead of a fixed
     328 MB block.
2. **Release a session's memory** so the same client can host again:
   - VirtualFree PMem and the hunk on a hosted exit;
   - stop the server's log and capture threads;
   - FreeLibrary the copy.
   This needs a `Sh1xHost_Shutdown` in the server repo.
3. **Map loads hold the client's frame for 4-6 s.** A listen server has the same limit, so the load screen
   covers it. Moving the zone load off the game thread is the fix if that matters.
4. **Faults outside sh1x_host.dll and msvcrt.dll are not contained.** For example, a fault in ntdll's heap code
   after server heap corruption. After a contained fault the session's port stays bound until the client exits.

### Asset layout stage 1 and PMem sizing (2026-10-27)
The server branch `feat/iw4-asset-layout` (`docs/CODOL_IW4_ASSET_LAYOUT.md` there) measured where the
server's memory goes. Asset reuse and the IW4 layout are planned there in stages.

- **PMem.** The fastfile block was 328 MB, committed up front, but held 1.5 MB. It is now only reserved, and each
  allocation commits its own pages. Its size is `codol_pmemMegs`; p2p.c passes 32 (`pmem_megs=` in p2p_host.ini).
- **In-process host, before / after**, mp_killhouse_sh, 2 players:

  | measure | before | after |
  |---|---|---|
  | private, in a match | 3,113 MB | 2,783 MB |
  | address space, in a match | 3,671 MB | 3,416 MB of 4,096 MB |
  | private, at the menu once ready | 2,366 MB | 1,619 MB |

- **One hosted session per client run (user decision).** After `p2pstop`, a second `p2phost` prints
  "this client already hosted a match; restart the client to host again". Measured: the stop ended the server
  with exit code 0 and released udp/17760, and the client stayed responsive.

### Shared zones and assets (M2, 2026-10-28)
Server `feat/iw4-asset-layout` 944b522c (pinned here), Kernel33 `feat/p2p-inprocess`.

**What is shared.** A hosted map change waits until C1 holds the map's zones. The server then mounts no zone that
C1 already holds: the level, `common_gametype_*`, their patch and localized zones, and the co-op session zone.
Every asset the match needs from those zones comes from C1's own records, read by name:
- weapons, attachments, xmodels (skeleton, geometry, lod surfaces), materials, physics presets and collision
  maps, xanims, fx, sound alias lists and curves, vehicles;
- the level's clipMap and its dynamic entities, ComWorld, the glass map and groups, addon MapEnts;
- rawfiles and string tables.
Script strings in C1's records are read through C1's table at `0x02627E00 + 16 * handle + 4`
(`Sh1xHost_SetC1ScriptStrings`).

The server keeps its own copy of the zone as the fallback. A lookup that has no record path, or a zone C1 has
stopped holding, mounts the copy in place and logs `CoDOL C1-backed <zone>: type N 'name' ... (from <callers>)`.
Nothing the server keeps points into C1's pools: names, glass pieces, sound lists and curves are copied into the
zone's archive and freed with it. C1 unloads the old level only after the server's ShutdownGame.

Switches: `c1_zones` in p2p_host.ini (default 1; 0 mounts every zone as before). `c1_preload` (default 0) makes
the server send `preloadmap` when C1 is not loading the map.

**Measured** (k33-p2ptest; hosted rotation killhouse -> rust -> zm_factory -> coop crash_tropical, `c1_zones` 1
vs 0; private memory and address space after each load):

| map | private | address space |
|---|---|---|
| mp_rust | 1,786 vs 1,823 MB | 2,740 vs 2,924 MB |
| zm_factory_sh | 2,096 vs 2,116 MB | 2,869 vs 3,192 MB |
| coop crash_tropical | 2,331 vs 2,370 MB | 3,127 vs 3,490 MB |

A 3-cycle soak of rust, zm, coop and survival (`mp_abandon_sh`) ran 12 loads with the host connected
throughout. Every C1-held zone was shared (7/7 rust, 5/5 zm, 6/6 coop, 7/7 survival). The one exception was a
zm load where C1's loading paused, so the server mounted its own copy. Private memory and address space stayed
flat across cycles.

**Remaining caveats.**
- **The first map** of a hosted session (hosting from the menu) is mounted from the server's copy, because C1 is
  not loading a map then. `c1_preload 1` makes C1 load it, but that led to C1 menu crashes in earlier tests.
- **Survival** works hosted, but only `mp_abandon_sh` was measured.
- **If C1's load pauses** for 30 s without a zone change, the server stops waiting and mounts its own copy.
- **Loading clients.** A C1 client loading a map stays CS_CONNECTED and silent. It now gets `sv_connectTimeout`
  instead of 16 s (also on the deploy line), because zm loads take 20-85 s under memory pressure.
- **Diagnostics.** `p2passetcheck` only checks assets the server still defines itself. C1-held definitions have
  no server copy to compare.

## Match-lobby local hosting (2026-10-28)

**Superseded in part (2026-11-12, `docs/P2P_DEFERRED_HOST_START.md`).** Enabling local hosting no longer starts
the server, room changes no longer restart it, and Ready/Start no longer wait for it: the choice is only armed
(`state=armed launch=deferred`) and the embedded server starts when the match launches for the owner's client.
The readiness rule below still holds for guests and for a host that is already running; the lobby additionally
has to hand the match to the owner of an armed intent. The paragraphs below describe the earlier behaviour.

Multiplayer, Zombies and Survival room owners have one Scaleform toggle:
**LOCAL HOSTING: ENABLED / DISABLED**. It uses the lobby's selected map and
mode; bot settings stay under the existing lobby/RCON controls. There are no
additional hosting settings in the UI.

The user's command is `p2p_localhost 1` or `p2p_localhost 0`. The movie supplies
room context internally through `p2p_room`. The host handles saved paths, port,
process lifetime and readiness. Normal lobby hosting always starts the separate
server process on the host worker; it never loads the server through C1's frame
fiber. Changed room selections
retire the old host before starting the new one. Startup state/errors appear
below the toggle. The INI and lobby admission flow are preserved.

Before startup, native code atomically publishes owner-bound intent in
`p2p_host.<uid16hex>.txt` in the backend state directory, refreshing it every two
seconds. Enabled intent selects only that owner's ready server with the exact
map, session, gametype and category. Starting, failed or stale intent blocks
Start; it cannot fall back to an already running standalone dedicated server.
Party guests use their room owner's intent. Disabled hosting restores public
server allocation; private local ports never enter that allocation pool.
Readiness requires `running=1`, `parked=0` and the full requested match identity.
A transient file replacement failure with a concurrent state reader pauses
Ready and retries at most twice per second; successful publication clears it.

Survival uses `map=mp_*`, `session_map=mp_*` and `gametype=survival`. Historical
prefixed room IDs are accepted as input but are stripped before loading the
level. Its scripts and authored entity placements load through the Survival
ruleset. The installed map catalogue continues to gate availability.

The installed minimal Survival zone also needs its streamed images. The reported
client crash was a missing `~coop_evac_case_spc-rgb&coop_~abeda9c9.iwi` while
loading `common_gametype_survival`. `tools/build_survival_image_repair.py` ports
only references absent from the target using the existing AssetExplorer image
porter and original donor assets, with encrypted archive CRC readback and
decoded surface comparisons. The deployed `main/iw_model_18.iwd` adds 196
missing images (1,782 verified decoded surfaces); all 296 streamed dependencies
now resolve. Its JSON manifest and verification report are retained in `p2p/`.

The first live join also exposed a separate `Too many precached menus` failure
in `_hud_message::initNotifyMessage`. Survival legitimately fills C1's fixed
32-slot menu bank. Native popup closes now check the client's loading/deferred
state first; active unregistered splash closes temporarily borrow and restore
a slot through ordered reliable menu commands. The inherited table and true
precache limit stay intact. The pinned server includes this fix and its full-bank
and reliable-stream regressions.

### Campaign localhost default

Campaign story rooms (`MapType.COOP`, native mode `coop`) start a localhost host
when their selected room information arrives. The ActionScript sends the exact
selected `coop_<mission>_mp_<level>` session identifier to `p2p_room`, then enables `p2p_localhost 1`;
the native bridge resolves the level while retaining that mission/session ID.
It never substitutes a generic campaign map. Campaign supplies only its room context and the enable state. Native hosting
infers its remaining configuration; lobby RCON owns bot settings.

(Earlier behaviour; startup protocols 0-2 now proceed at once, see `docs/P2P_DEFERRED_HOST_START.md`. The
production protocol 3 keeps this gate.)
For every enabled local-host room, pressing the normal Start Match or Ready button waits for `ui_p2p_ready=1` and an
exact `ui_p2p_map` session match before issuing the original START_MATCH task.
Startup errors or a 180-second timeout display a room error and leave Start
available to retry. Leaving the room, changing its mission or losing host status
cancels the pending launch. A failed local launch never falls back to requesting
a different server. Native authentication/admission and the existing room start
transaction remain responsible for joining the selected local server.
The visible Ready path keeps native Ready false until the host is ready, then
applies the original room-state change once. Unready remains immediate.
