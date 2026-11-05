# P2P startup adapter checkpoint

This is an isolated, uninstalled prototype. It does not qualify supported
production Local Play. The running test client and its runtime are unchanged.

The source union is `ad982ac4fbd4ba5c6e86ce4b3f75110901ae9357`, with exact parents
`b18a24f32729ac126d306eccea5e508596c29d94` and
`410d1ccfa60dc1377e00664b845f991b41533133`. The 13 reviewed native migration and
profile files remain unchanged. The union also keeps the narrow Pi exception
policy that defers to an active migrated SEH guard rather than rescuing its
nested CRT fault before fail-fast can run.

## Startup and ownership

The prototype requires `CODOL_LOCAL_TESTING=1` and an explicit
`CODOL_P2P_STARTUP_IPC_V1=1`. These are qualification gates, not the final product
configuration. The existing launcher/SDK does not enable or serve this protocol.
Mode zero retains legacy preparation; malformed selection fails closed.

In the new mode Create records the selected room but does not load the native
host. A real Ready/Start action emits a separate intent with selection hints.
The C main thread compares those hints with its armed context and issues a
monotonic local attempt token. That token is not a DW or principal epoch.
Context, owner, cancellation and terminal changes revoke it. Genuine DW Ready
and Start remain withheld until native readiness; no synthetic Ready is added.

The SDK must independently derive the authenticated DW room, admitted Host,
confirmed roster, selection, match, manifest, build and retained endpoints. UI
flags, intent files, addresses and claimed UIDs provide no authentication.

The new pipe client requires an inherited SDK process handle with SYNCHRONIZE
and PROCESS_QUERY_LIMITED_INFORMATION, plus the expected SDK PID. It duplicates
the actual handle rather than reopening a PID. Pipe-server PID, retained parent
liveness and current attempt are checked at boundaries. The future launcher must
use an explicit inherited handle list; that production integration is absent.

The versioned ABI is shared with Python `prototypes/p2p/startup_ipc.py`:

| Message | Fixed header | Binding |
| --- | ---: | --- |
| CPSQv1 request | 56 bytes | Fresh nonzero nonce and bounded selection |
| CPSEv1 envelope | 152 bytes | Request, lengths and SHA256 of CVSP + CPRB |
| CPSAv1 ACK | 76 bytes | Exact request nonce and envelope binding hash |

The ACK contains the two installers' combined actual result. Only two literal
results of one produce success. ACK is not engine Ready, DW approval, active
match authority or remote attestation of a hostile local process.

The worker fetches the envelope. The game-thread server fiber atomically claims
its inventory, calls both native installers, checks fresh principals, then calls
Init. The worker sends the actual acknowledgment; it never calls the installers
from Python or another process. A selected SDK must validate ACK and bounded EOF
before forwarding, and observe native readiness separately before DW approval.

The context is static and one-shot after a successful fetch. Cancellation wins
FETCHED to CANCELLED before reclaiming, or loses to FIBER_OWNED and retains that
single bounded inventory with the spent host. DONE is the fiber's last inventory
access. One worker owns reclamation. A never-published failed fetch alone may
release its reservation. No native pool reset, unload or onehost unlock is added.

## Expiry and cancellation

The original transfer authorization deadline is 30 seconds. CPRB's absolute
expiry and a fixed monotonic lifetime impose an earlier bound where applicable.
An observed refusal/expiry latches abort and cannot be revived by clock rollback.
Verification is rechecked after native calls before publishing freshness.
Cancellation drains pending I/O before releasing OVERLAPPED buffers/events; this
necessary drain is not a promise of return within exactly 30 wall-clock seconds.

Current native principal APIs are provided by separate native commit
`a89c230cacd245bdcf06e8a754c7494f601616e5`:

- `Sh1xHost_RelayPrincipalsCurrent` observes lease freshness only on the retained
  API owner thread between native calls. It does not report map/DW Ready.
- `Sh1xHost_QuarantineRelayPrincipals` is a terminal atomic deny operation that
  the owned worker or game thread can call during a cold/yielded native frame.

Local cancellation invokes quarantine immediately after releasing the intent
lock. It does not wait for the worker's polling interval. Init failure, expiry,
fault and terminal exit keep principals closed and preserve the onehost guard.

## Qualification limits and production gate

Portable wire tests, real native provider/parser/API fixtures and actual
extracted client fiber bodies run offline. Windows pipes, inherited handles,
fibers and engine calls still have doubled boundaries. Neither a compiled DLL
nor an ACK fixture proves a real Host/Guest match.

Still required before installing a coherent candidate or claiming release:

- Actual owned Windows duplex SDK server, retained C1/socket verification,
  signature/grant collection and inherited parent-handle launcher integration.
- Frozen-round post-Init approval replacing the legacy companion's periodic
  `prepare_room`, plus authenticated active principal continuation. Startup-only
  leases must not be stretched or revived to keep a match alive.
- A reviewed production feature/configuration matrix independent of
  CODOL_LOCAL_TESTING, QA helpers and development executables. Use the existing
  approved launcher/client paths. Protocol parsing in a local-test profile is
  not supported production enablement.
- Real cold-load, keyboard/mouse/controller, MP/Zombies, cancellation/recovery
  and two-network Host/Guest acceptance; public signaling/relay implementation.
- Actual principal retirement and Windows native reset before any rehost
  unlock. The staged-source coordinator's observer remains unbound.

The latest separate GSC parity source is
`66b7cd1911017275094ed165a2031308890b2747`, derived from the current published
Factory-admission Zombies package while preserving P2P/Survival source. It has a
new immutable local revision five; neither revision four nor runtime selection
was overwritten. This document grants no deployment, client restart or GUI input.
