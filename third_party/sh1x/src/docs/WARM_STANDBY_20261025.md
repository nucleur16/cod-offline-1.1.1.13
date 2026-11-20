# Prepared native matches

This branch adds the native foundation for a worker to load one selected match
before players request it, hold its simulation, then assign it once. It does
not yet implement an allocator-managed spare pool or establish faster loading.
The previously qualified server-efficiency package remains unchanged.

Both exact compiler profiles are now pinned with the warm implementation:
normal1719 source files and Zombies1731, each plus the four retained Linux
libraries. Zombies keeps its token APIs and category-specific code; MP Quick
implementation is not added to that profile. Its profile delta was ported and
reviewed against both prior source inventories, with unrelated inputs preserved.
The updated materializer reproduces both complete inventories and hashes.

## Native contract

An empty `warmPreparation` preserves ordinary operation. A worker may set
this immutable startup setting to a nonzero 32-character lowercase hexadecimal
preparation identifier before the first `map` command. The first completed map
then enters the prepared state. An unassigned reload or a changed prepared
selection invalidates that preparation; it cannot silently become another match.

`sh1x_status` adds `warm_phase`, `warm_preparation`, `warm_lease` and
`warm_selection`. Phase values are off0, loading1, prepared2, active3, invalid4.
The selection is SHA256 over the actual named native settings in
`warm_standby.c`, serialized in their fixed order as `name=value\n`.
It covers the loaded map/session/category, mode, playlist, difficulty, Endless,
Quick identity/rotation, seat/bot settings, match exit policy and reward rates.
These are actual launcher/native dvar names, checked against the existing launcher.

The worker must verify the actual prepared selection, own the process and port,
reserve its capacity, and atomically publish short-lived assignment tickets before
issuing the authenticated RCON command:

`sh1x_warm_activate <preparation> <lease> <selection-sha256>`

The native command recomputes the selection in the same game-thread transaction.
Its first success returns `accepted=1`; an exact retry returns2. A different
process, selection or lease is refused. Unexpected connected/primed humans
prevent activation. An activated world never re-enters the prepared state,
including after a normal active map rotation.

The additional roster line is:

`warm <port> <preparation:32hex> <lease:32hex> <uid:16hex> <expiry-unix-seconds>`

It is additional to normal authenticated DTLS/UID membership and any existing
Quick map-epoch/party checks. Tickets must be unexpired and at most120 seconds
ahead. A retained old key or roster line for another process/lease cannot open
this prepared process. These tickets do not themselves supply DTLS credentials.

The worker uses `warmAdmissionPath` for this private assignment file.
`admissionPath` continues to read the central lobby credential roster.
Native booking publishes some host-bound guest records after Ready. Waiting for
all guest credentials before Ready would deadlock; copying the central roster
once would miss later guest publication. The private file adds no DTLS keys or
host/member credential authority. Removing it denies new assignments without
erasing central credentials. Ticket expiry does not disconnect existing players.

## Freeze boundary

`SV_Frame` checks the prepared state before `SV_PreFrame`, game simulation,
snapshots, archives and post-frame publication. While held it clears game-time
residuals, leaves `svs.time` unchanged, and yields through `NET_Sleep(20000)`.
The ordinary outer command/packet pump remains reachable. The first resumed
frame discards the final warm wall-clock interval once, so waiting does not
become a catch-up burst. The existing `sv_paused` flag does not implement this.

If an unexpected human is already connected, the preparation is invalidated
and simulation proceeds to preserve that occupied game. The allocator must
retire it only after a fresh empty observation. No force-disconnect is added.

## Offline qualification

`python tools/test_warm_standby.py --output <new-evidence-directory>` runs
without starting a game client or dedicated server. On Windows it invokes WSL.

The retained r4 run passes31 policy assertions,22 actual roster-file checks
under ASan/UBSan,41 adapter/frame-boundary assertions in four scenarios and40
existing native Quick lifecycle/admission/status checks with the real warm
adapter disabled. Eight complete touched Linux translation units compile.
One policy assertion includes30,000 held polls; this is not a performance result.

The adapter fixture supplies engine globals, dvars, command dispatch,
`NET_Sleep` and the ticket-lookup result. Its hash provider wraps retained
mbedTLS SHA256. The frame gate is extracted from the real `SV_Frame` source,
with its private timestamp renamed for the fixture. The separate ticket test
uses the real bounded parser, atomic same-size replacement and removal. The
interface-only compile supplies an unrelated update-host configuration macro.

This is not a complete native link/boot, actual UDP health/admission, Windows
runtime, gameplay, measured CPU reduction or loading-time acceptance. Earlier
r1 stopped on a fixture locator assertion that matched a comment; native policy
and ticket checks had passed. The corrected r2 locator targets the actual call.
The r3 run passed the new roster separation cases but exposed the existing
legacy fixture's single-string-dvar assumption. Its provider now registers the
new, disabled-by-default path separately; r4 passes all19 steps.

The separate materialized-profile run passes28 build/run steps:41 actual warm
adapter/frame-boundary assertions and22 real parser/file assertions in each
profile, plus seven complete changed translation units per profile. That run
uses host GCC15 for interface/unit qualification only. Production Linux builds
still require their separate compatible target compiler and full runtime proof.

## Required allocator work before enabling

- Use an operator-verified native capability and exact package identity.
  Existing workers/launchers must continue without preparation arguments.
- Create the prepared lease before launch and keep its identity through claim,
  including the existing Quick session nonce. Match the whole launch selection.
- Count loading/prepared/activating jobs inside the same two-process capacity;
  one spare must never mean two active matches plus a third native process.
- Preserve claim idempotency, process/port ownership, durable admission-file
  readback, readiness and failure recovery before publishing a playable endpoint.
- Stop empty spares when maintenance/drain/capacity changes are pending, then
  inhibit refill throughout the continuous-empty interval and announced window.
  Unknown occupancy resets the gate; a permanent spare must not block maintenance.
- Retire a used world normally. Replenishment creates a fresh process; it does
  not clear players/scripts/entities by relabeling an old match.
- Measure real idle CPU/RSS, ready-to-assigned latency, both native profiles,
  failed/lost activation replies, stale tickets, drain and post-wait game clocks.
  Original-client join/gameplay acceptance remains separately required.

Native/client-coupled release checks and production maintenance gates still
apply. No new binary, service, selector or production configuration is installed.
