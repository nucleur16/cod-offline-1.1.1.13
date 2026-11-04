# Private retained-SDK active control v2

Source-only and local-testing only. This channel is separate from startup v1.
It does not authenticate HTTP, attest a hostile C1, authorize public routing,
prove native/map/DW Ready, or qualify world cleanup/rehost. No launch/runtime
enablement is added by this source change.

Client connects once to `CODOL_P2P_ACTIVE_PIPE`, restricted to
`\\.\pipe\CoDOLActiveControl-` plus32 lowercase hex characters. It retains the
original inherited SDK process capability and checks that process lifetime and
actual kernel pipe-server PID throughout. No reconnect or first-packet identity
adoption is allowed. The SDK writer must require its distinct verified active
plan token, actual retained endpoint/association ownership and paused dispatch.

All integers are little endian; no native struct packing is used.

HELLO: LE32 length192, then magic8 `CPAHv1\0\0`, flagsU32@8=0,
totalU32@12=192, helloNonce32@16, bootstrapNonce32@48, bootstrapBinding32@80,
manifest32@112, nativeEpoch16@144, counterU64@160, generationU64@168,
ownerUIDU64@176, initialNativeSequenceU64@184. Bootstrap values, including the
full original CPRB, are captured by value before the startup packet is erased.

CONTROL: LE32 length272..928, then magic8 `CPACv2\0\0`, operationU32@8
(1UPDATE/2REVOKE), totalU32@12, controlSequenceU64@16, helloNonce32@24,
bootstrapBinding32@56, manifest32@88, activeID16@120, handoffID32@136,
hardExpiresU64@168, pathEpochU64@176, pathID16@184, CPRBLengthU32@200,
ownerDeadlineLow32U32@204, CPRB SHA25632@208, binding32@240, CPRB@272. UPDATE CPRB
length112..656; REVOKE has length0 and SHA256(empty). IDs/nonces must be nonzero.
REVOKE requires ownerDeadlineLow32=0. UPDATE low32 zero can be valid across
wrap. Reject trailing bytes, unknown operations and wrong captured scope.

Binding is SHA256 of ASCII `CODOL-P2P-ACTIVE-CONTROL-V2` plus one NUL, entire
HELLO192, CONTROL bytes0..239, then CPRB bytes. The digest field is excluded
from its own input.

ACK is raw96 bytes: magic8 `CPAKv2\0\0`, controlSequenceU64@8,
helloNonce32@16, binding32@48, resultI32@80, operationU32@84,
lastInstalledNativeSequenceU64@88. Result1 UPDATE requires the actual native
active installer to return literal1 on the owning game fiber between Init/Frame calls,
with all permission checks still live. Native result2 is refusal. Result1
REVOKE requires actual quarantine plus the owner fiber observing native
Current==0; it reports denial, not engine exit or resource cleanup. Python must
keep dispatch paused until exact ACK and its separate active-controller check.
V1 CONTROL/ACK are not accepted; HELLO remains V1 with the same layout.

The active export is loaded separately:

```c
int __cdecl Sh1xHost_InstallActiveRelayPrincipals(const void *, unsigned,
                                               uint64_t owner_deadline_qpc_ms);
```

Startup retains the generic two-argument installer. Active updates never fall
back to it. An active channel with the new export absent fails closed.

Control sequence and native update sequence advance exactly by1 without wrap.
Native epoch/counter/generation/owner and every UID/grant/source-endpoint entry
remain identical to the original CPRB. First successful promotion pins active
ID/handoff ID/hard expiry. Later changes refuse. CPRB expiry-issued<=30000ms,
expiry<=hard expiry, near-clock issuance and dual monotonic/wall expiry apply.
The earliest observed offered lease and initial hard monotonic budgets are
captured before native installation, reused across prepare/commit checks, and
never extended by late installation or a flat wall clock. Clock samples and
high-water updates share one lock; native calls run outside that lock.

For UPDATE, compute `delta = (uint32_t)(low32 - (uint32_t)nowQpcMs)` and require
`0 < delta <= 30000` plus a checked full-width addition. The reconstructed
absolute owner deadline is pinned on the first prepare. The offered deadline
is the earliest of this owner deadline, the UTC-derived lease budget and the
original hard monotonic deadline. The worker, fiber and commit retain those
same bounds. The fiber snapshots the offered absolute deadline under the lock
and passes it as the native export's third argument. The native producer caps
its own first-sample monotonic lease before validation and cannot turn transit
or install delay into another 30 seconds of authority.

The clock namespace is same-machine global Windows QPC milliseconds, shared
with the retained SDK parent; it is not UTC, GetTickCount or process uptime.
Linux source fixtures use absolute CLOCK_MONOTONIC milliseconds. Actual Windows
parent/client/native QPC correspondence is a separate qualification gate.

The permission state is BOOTSTRAP→PROMOTING→ACTIVE or terminal DEAD. Promotion
starts only after the startup ACK worker completed successfully and while the
original startup/native lease remains live. Startup P2PSCurrent30s is unchanged;
expired bootstrap or cold Init cannot be salvaged. The obsolete startup timer
must win a CAS from BOOTSTRAP/PROMOTING before quarantine, so it cannot revoke
an already promoted ACTIVE state. Actual parent/control loss or user cancel
denies every state, including a promotion racing a successful native install.

One worker stages at most928 immutable bytes. Only the game fiber claims and
installs them; it publishes DONE after its final read. A timeout while owned
retains the static bounded context rather than erasing/reusing memory under a
native call. Idle prefix reads check parent/pipe/current permission every50ms
and remain bounded by the current lease, at most30s. After the length arrives,
payload/install/ACK share a5s exchange budget. Pending Windows IO is cancelled
and drained before its OVERLAPPED/event lifetime ends. Close/error is terminal.
Parent/control/deadline loss invokes atomic quarantine before cancellation or
any potentially blocking kernel drain; memory retention never retains authority.

Validation uses actual portable C/Python vectors, actual extracted client
fiber/transfer functions and actual native owner API/provider/CPRB parser under
i386 ASan/UBSan. Windows/fiber/pipe/Init/Frame calls are explicitly doubled.
Existing20 startup scenarios remain, alongside promotion/cancel/native-refusal/
deadline/parent-loss and fragmentation/cancellation/drain cases. The parent
golden probe also parses the actual startup envelope, reclaims it, and compares
the resulting HELLO/control/ACK. Active fixtures establish every native peer's
actual receive/bind claim using explicit deterministic handshake/DATA doubles,
then execute the real active installer and its actual private SHA256. Delayed
staging, flat-UTC owner expiry, low32 wrap and missing-export/native-peer cases
are included. These are not Windows game qualification.
