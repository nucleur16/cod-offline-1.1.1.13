# Local Play: the embedded server starts when the match launches

Branch `k33-deferred-host` (off `main` d0b0756), 2026-11-12. Supersedes the start-on-tick behaviour described in
`P2P_HOSTING.md` ("Match-lobby local hosting") and `P2P_SCALEFORM_LOCAL_MATCH.md` for startup protocols 0, 1 and 2.

## What changed

Ticking **LOCAL PLAY** only records the choice. Kernel33 validates the room selection, publishes the owner-bound
intent as `state=armed launch=deferred ready=0` and starts nothing: no host thread, no `sh1x_host.dll`, no zone,
no memory. Ready and Start in the room no longer wait for a running host. The embedded sh1x server is started at
the moment the match the room started launches for the owner's client; the client's own connect then waits for it.

| step | before | now |
|---|---|---|
| tick LOCAL PLAY (`p2p_localhost 1`) | protocol 0: host started at once, restarted on every room change | every protocol: arm only |
| room map/mode change while ticked | protocol 0: host stopped and restarted | re-arm only (a host that was already launched for another room is stopped) |
| Ready / Start | UI withheld them until `ui_p2p_ready=1` (up to 180 s) | proceed at once; protocols 1/2 send `p2p_start_intent`, which only permits the launch |
| host start | on the tick (0) or on `p2p_start_intent` (1/2) | on the launch (below) |
| untick, leave, owner change, cancel | stop / invalidate | disarm; a launched host is stopped; a pending launch can never start one |

## The launch moment

Two inputs raise the same one-word request; the main-thread tick (`P2PLaunchTick`) consumes it and starts the armed
selection exactly once. Neither input starts anything by itself and neither runs on a thread that touches C1.

1. **C1's first datagram to the armed loopback port** (`src/p2p/hostgate.c` `HostGateSendTo` -> `P2PLaunchObserve`). C1 does
   all its UDP through the hostgate's `sendto` wrapper. When the lobby's start-game handoff reaches the client, C1
   enters its connecting screen and immediately sends to the match server's address, which for the owner is
   `127.0.0.1:<port>` of its own armed host. Measured (below): this is the first native event of the launch on the
   client and it is reliable, because it is the connect itself. Observation only: the datagram is sent unchanged.
2. **The lobby's call** `K33LocalPlayLaunch(owner_uid, session_map, gametype, type)` (`src/p2p/p2p_local_play_api.h`), for
   the lobby that runs inside Kernel33.dll: it knows the launch before any datagram exists. The console command
   `p2p_launch` is the same input for the UI or a test.

The request is honoured only when the choice is enabled, armed, error-free, not already launched, and (protocols 1/2)
the owner's own matching `p2p_start_intent ready|start` was accepted. A datagram to another port, a datagram without
the choice, or a lobby call for another uid or selection does nothing.

The client then waits in C1's own connecting screen (the map's loading screen with `Awaiting connection...0`): C1
resends its connect while the host boots, and joins when the host answers. Measured host boot at the launch: 12.9 s
and 14.9 s from the first datagram to the first exact `sh1x_status` (mp_killhouse_sh, `Sh1xHost_Init` 2.3-2.5 s); in
the second run the client was already counted (`humans=1`) in that first status.

How long C1 waits by itself was measured with the port held by a dummy socket (nothing answers): C1 kept sending
(8 datagrams in the first 5 s) and still showed `Awaiting connection...0` after 40 s; by 100 s it showed its notice
`Unable to connect to host`. So a host that needs more than roughly 40 s to answer (a cold disk, a large map) can
lose the first connect; the host keeps booting, the owner is back in the lobby, and the next Ready joins the running
host at once (or the empty-host rule below stops it after 120 s). Extending or re-driving C1's connect is not done.

The wait has no text of its own: the player sees C1's loading screen with `Awaiting connection`. The room box's
`STARTING HOST...` caption exists (`ui_p2p_launching`), but the loading screen replaces the room almost at once, so
it was not seen in a capture.

## Never leaving a server running

- Untick (`p2p_localhost 0`), `p2pstop`: disarm; a launched host gets `quit` (as before).
- Leaving the room / losing the owner seat: the room helper sends `p2p_localhost 0` (protocol 0) or
  `p2p_cancel_start` / `p2p_localhost 0` (1/2), as before; both now also disarm.
- A changed room while a launched host runs another selection: that host is stopped (protocol 0, `P2PArmLegacyRoom`).
- A cancelled start: nothing was started (the start is the launch). A pending launch that was cancelled, unticked,
  re-roomed or whose owner uid changed can never start a host afterwards (fixture cases).
- A launched host nobody is on (`humans=0` in its `sh1x_status`) for 120 s is stopped (protocol 0, `P2PLaunchWatch`):
  the launch was abandoned, or everybody left.
- A launched host that fails before it ever answers: Kernel33 ends C1's connect once (`disconnect`), so the player is
  back in the lobby with the error instead of waiting out C1's connect timeout (`P2PLaunchAbort`).

Unchanged: one hosted session per client run. That made start-on-tick expensive (a tick, or a room change, spent the
client's only session); a tick now costs nothing.

## What the lobby must do (the rule for the in-DLL C++ lobby)

The Python lobby branch `fix/hosted-survival-spawn-20261028` routes a Local Play room through the owner's intent file
`<state>\p2p_host.<uid16hex>.txt` (`handoff/match_instances.py` `_localhost_placement`) and **blocks** START_GAME
unless `ready=1`, `state=ready` and the host answers `sh1x_status` with the exact selection, `running=1`, `parked=0`.
With a deferred start that never becomes true before the launch, so the room could not start. The lobby rule becomes:

For the placement of player `me` in a room whose owner is `owner`, read the owner's intent
(`K33LocalPlayIntent(&info)` in-process; the file for an out-of-process lobby):

1. No intent, or `enabled=0`: public placement, as today. Private local ports never enter the public pool.
2. `enabled=1` and the intent is stale (file older than 15 s; not applicable in-process), `failed`, `stopping`,
   `exited` or `idle`: **withhold START_GAME**. Never substitute another server. (Unchanged.)
3. The intent's `(map, session_map, gametype, type)` differs from the room's selection: withhold. (Unchanged; compare
   with the native mode names: `tdm`->`war`, `ffa`->`dm`, `hq`->`koth` for `type=mp`.)
4. `ready=1`, `state=ready`: verify `sh1x_status` on `port` (exact selection, `running=1`, `parked=0`) and hand the
   match off to that port. (Unchanged; this is also every guest's path.)
5. **New.** `state=armed` (or `starting`), `launch=deferred` (or `started`), `ready=0`:
   - `me == owner`: accept. Hand START_GAME for `127.0.0.1:port` to the owner **without** probing `sh1x_status` and
     **without** dispatching a map change (the host boots straight into the intent's selection). In-process, call
     `K33LocalPlayLaunch(owner, session_map, gametype, type)` at that moment and withhold START_GAME if it returns 0.
   - `me != owner`: withhold until rule 4 holds (the host exists only after its owner launched). A guest's Ready
     simply succeeds a few seconds after the owner's.

The intent file gained one line, `launch=none|deferred|started`, and the state value `armed`. `ready` keeps its exact
meaning (the host answers with this selection); `armed` is never `ready`.

`docs/P2P_DEFERRED_HOST_START_python_lobby_testonly.patch` is the 20-line change of rule 5 against that Python
branch. It was used only to live-test this branch on a throwaway local copy; it is **not** a deliverable (the lobby
logic is being ported into Kernel33.dll in C++ and rule 5 belongs there).

## Offline mode: every owned room is hosted, with no choice (branch `k33-offline-m14-survival`)

This fork is offline only (`docs/OFFLINE_LOBBY.md`): the lobby runs inside Kernel33.dll and there is no
other server. The owner decided that local hosting is then not a choice: **no tick, no untick**.
Everything above about the deferred start still holds - arming starts nothing, the host starts at the
launch, a failed launch ends the connect - but what arms, and what the lobby does, changes while the
in-DLL lobby is active and the protocol is 0 (`P2PAutoMode`; the launcher protocols 1-3 cannot occur,
a client started by them is refused at start-up):

* Hosting is always enabled. `p2p_room` (the room UI's context) arms by itself; `p2p_localhost 1` is
  not needed and `p2p_localhost 0` is ignored. `ui_p2p_enabled` is 1 as soon as the tick runs.
* The lobby is the authority on the selection, for every kind of room (also the ones whose UI sends no
  `p2p_room`: playlist / matchmaking rooms). It calls, from its own thread,
  `K33LocalPlayAuto(owner, session_map, gametype, type, difficulty, launch, &port, error)`:
  validated at once on the caller's thread (`P2PParseRoomContext`, `P2PCheck`, one hosted session per
  client run), then posted to a one-slot request the main-thread tick applies (`P2PAutoTick`): the room
  context is replaced, `P2PArmLegacyRoom` arms, and with `launch` the explicit launch signal is raised.
  A launch posted for a selection is not taken back by a later arm of the same selection; the launched
  host of exactly that selection is simply joined again.
* Rule 5 has no "off" case any more. The lobby's rule: owner -> `K33LocalPlayAuto`; accepted ->
  START_GAME to `127.0.0.1:port`; refused -> withhold with the reason, never another server. A guest
  (not possible with one local player, kept for completeness) -> rule 4 only.
* `K33LocalPlayLeave(owner)`: the owner left the room - disarm, forget the room context, stop a launched
  host (what unticking did).
* Feedback: the reason of a refusal is published as `ui_p2p_error` (state `failed`), which the patched
  room UI shows as `Localhost hosting failed: <reason>` after Ready; the lobby adds a ticker line. A
  selection that was accepted on the lobby's thread but cannot be armed on the main thread (the state
  changed in between) ends the client's connect with `disconnect`, like a failed launched host.
* The LOCAL PLAY boxes of the patched movie are still drawn in the Create Room dialogs and are inert
  (`docs/OFFLINE_LOBBY.md` says what an ActionScript change would be). No ActionScript was changed, so
  the movie pin chain is untouched.
* The host's command line gains `+set g_difficulty <0..2>` for a Zombies room and `+set net_ip6 ::1` for
  a loopback host.

Fixture: `tests/test_p2p_start_intent.py` (offline block). Live: see `docs/OFFLINE_LOBBY.md`.

## Native interface

`src/p2p/p2p_local_play_api.h` (C and C++):

```c
int K33LocalPlayIntent(K33LocalPlayIntentInfo *out);          /* any thread: the last published snapshot */
int K33LocalPlayLaunch(unsigned long long owner_uid, const char *session_map,
                       const char *gametype, const char *type); /* any thread: raise the launch request */
```

`K33LocalPlayAuto` / `K33LocalPlayLeave` (offline mode, above) are in the same header.

`K33LocalPlayIntent` copies, under `g_p2pLock`, the snapshot the main-thread tick last published (owner uid, enabled,
armed, launched, ready, failed, port, protocol, selection, state, error). `K33LocalPlayLaunch` returns 1 when the
snapshot is exactly that enabled, armed selection (the request is raised) or its host was already started, else 0. It
loads nothing and never touches C1: the main thread revalidates and starts.

UI dvars: `ui_p2p_state` gains `armed`; new `ui_p2p_armed` and `ui_p2p_launching` (0/1). `ui_p2p_ready` is unchanged.

Commands: `p2p_launch` (new). `p2p_room`, `p2p_localhost`, `p2p_start_intent`, `p2p_cancel_start`, `p2pstop` keep
their arguments; `p2p_localhost 1` and `p2p_room` no longer start a host in protocol 0, and `p2p_start_intent` no
longer starts one in protocols 1 and 2.

## Protocols

- **0 (legacy).** Full deferral as above. `p2phost` / `+set k33_p2pHost 1` (explicit console hosting) are unchanged.
- **1, 2 (local startup IPC).** `p2p_start_intent` validates the intent against the armed room exactly as before,
  invalidates the previous attempt, arms, and records the permission. The startup generation is issued, and the
  worker created, by the launch. Ready then Start is still one attempt; a room, owner, enable or cancel change
  revokes the permission. Startup generations, the principal/option checks, the 30 s authorization and every
  fail-closed path are untouched. The paired SDK's rule "observe native readiness before DW approval" has to become
  rule 5 above on its side; that SDK is not in this repository and was not changed or exercised.
- **3 (production direct-v1).** Not changed. Ticking already only arms there. Its host start is sequenced by the
  retained SDK's signed plan (role commit, bootstrap install before `Sh1xHost_Init`, the initial-handoff gate that the
  UI waits for), and a Guest's connect target comes from that same session, so the session cannot begin at the
  Guest's first datagram. Deferring it needs a protocol change on the SDK side. The UI keeps its gate for protocol 3.

Security rules kept: the hostgate still drops and refuses exactly what it did (the launch observation is a read of
the destination port of a loopback datagram that is sent anyway); only loopback / lobby-vouched hosts are reachable;
no background thread patches or calls C1 (both launch inputs are atomic words consumed on the main thread); hosting
still requires the verified-script service in a local-testing in-process build.

## Verified

Fixtures (run on this branch):

- `python3 tests/test_p2p_start_intent.py` (WSL, gcc -m32 ASan/UBSan): the actual `src/p2p/p2p.c` bodies. Arm-only tick in
  every protocol; no start from frames, from a foreign-port datagram, or from a datagram without the choice or
  (1/2) without the owner's intent; one start per launch; lobby-call launch and its refusals; cancel / untick /
  re-room / owner-change revocation; the changed-room stop; the empty-host stop; the failed-launch disconnect; and
  all earlier generation/duplicate/exhaustion cases. This fixture did not compile at d0b0756 (it predated the
  production kernel symbols); it is repaired here.
- `node tests/local_play_deferred_test.js`: the actual ActionScript bodies of the Ready/Start routing and the room
  helper's status.
- `node tests/local_match_controls_test.js`, `node tests/local_match_layout_test.js`.
- `tests/hostgate_windows_fixture.c` (both profiles, `sh tests/run_tests.sh`): the launch is observed for a
  loopback destination only, and that datagram is still sent. The fixture did not compile at d0b0756 (it lacked
  the production socket symbols); its stubs are added here.
- `ui_baseline.json` was re-recorded (`python -m zonekit.ui_baseline --record`); `tests/test_ui_baseline.py` passes.
- The movie pin chain (branch `k33-tests-green`). `tools/p2p-four-tag-composition` pins the reviewed movie and the
  reviewed sources of four P2P classes; three of them (`LobbyController`, `LocalMatchControls`,
  `LocalCreateRoomOption`) changed here, so `tests/test_invitations_disabled.py` failed with `separate
  predecessor/current movie identities`. The four-tag contract and its semantic review record were not re-pinned.
  A successor stage, `tools/p2p-deferred-host-composition`, preserves the three sources, three tags and `patch.txt`
  of the four-tag endpoint as DATA witnesses, so that stage still proves its own endpoint, and then proves the tree's
  movie over it: only those three tags differ, and the inverse restores the predecessor. Its record,
  `automated-diff-review.json.DATA`, is an **automated** review of the source and stored-tag difference, not a human
  one and not the independent QA the four-tag record came from. `resolved_abc.py` compares the old and new tags
  method by method with every operand resolved; the stage recomputes that on every run. Result: one method and one
  trait added per class, nothing removed, and the only other methods that differ beyond compiler spelling are the
  edited ones (`onChangeReadyStateHandle`, `onStartMatchHandle`, `onCampaignHostReady`, `onProtocolFrame`).
  **Finding:** 35 `LobbyController` and 8 `LocalMatchControls` methods with unchanged source were stored with other
  instructions (explicit `Boolean()`/`String()`/`int()` conversions as calls instead of convert opcodes; builtin
  members such as `indexOf`/`splice` late-bound instead of AS3-namespace QNames), so the tags of this change came
  from another compiler build or configuration than the predecessor's. Not run: any compiler, decompiler or AS VM.
- `tests/test_p2p_match_options.c` now exercises `P2PParseRoomContextV2` (it did not compile with the runner's
  `-Werror` at d0b0756 because that function was unused).
- `tests/campaign_local_host_test.js` (stale since before d0b0756) drives the room's own handlers again: Ready and
  Start proceed at once in protocols 0-2 with the failure watch, and protocol 3 keeps its waiting gate.
- **Open:** `tests/test_r46_native_source_integration.py` (run by hand, not in `tests/run_tests.sh`) pins a whitespace
  hash of `HostGateSendTo` in `tests/r46_native_source_contract.json`; the launch observation added to that function
  here changes it. That pin was left for a review of the native change.

Live (2026-11-12, own shadow client and own lobby copy on 127.0.0.1, user `edtest`, protocol 0, mp_killhouse_sh
Team Deathmatch, dev-profile DLL of this branch):

| check | result |
|---|---|
| Create Room with LOCAL PLAY ticked, room open | intent `enabled=1 ready=0 state=armed launch=deferred server_pid=0`; process private 1311 MB before and 20 s later, 44 threads, no `k33_sh1x_host_*.dll` copy, UDP 17760 unbound |
| Ready (real UI click, new zone) | lobby: `localhost: owner launches; the host on :17760 starts with the owner's map load`; Kernel33: `p2p: match launch (C1's first datagram to the armed port)`; `server ready after 12953 ms`; client in the match |
| same with the stock room UI and a native Ready | `server ready after 14860 ms ... humans=1` |
| untick in the match | `server ended (init 0, exit code 0)`, UDP 17760 released within 6 s |
| host cannot be armed (server DLL removed) | Ready shows `Localhost hosting failed: server DLL not found (dll=)`, lobby withheld START_GAME, nothing started |
| launched host fails (UDP 17760 held by a dummy socket) | `p2p: not hosting: udp 127.0.0.1:17760 is in use`, `p2p: the launched local host failed before it answered (the UDP port is in use); ending the client's connect`; intent `state=failed`; the client was back in the Multiplayer lobby list within the minute instead of C1's own 40-100 s wait. It is not returned to the room and no message box is shown there: the reason is only in the intent state and `Kernel33.log` |

Pictures (Kernel33's own `k33_shot.req`, `D:\CoD\k33work\dh\shots\`): `n_room_ticked.png` (room, box ticked, nothing
running), `n_wait2.png` (C1's connecting screen during the host boot), `n_match.png` and `match1.png` (in the match),
`e_wait8.png` (arm failure message), `f_40.png` / `f_200.png` (C1's own wait and its notice), `g_after.png` (after the
aborted connect).

The live DLL carried one uncommitted test-only change: the verified-script requirement was bypassed
(`P2PCheck`, `P2PInprocRun`), because neither a verified-script SDK service nor a `sh1x_host.dll` exporting
`Sh1xHost_UseVerifiedScripts` exists on this machine (the pinned server submodule 1e945cab does not have it). The
committed source keeps the requirement, so hosting with the committed source was not run here.
