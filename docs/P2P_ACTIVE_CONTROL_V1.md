# Private retained-SDK active control v1

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

CONTROL: LE32 length272..928, then magic8 `CPACv1\0\0`, operationU32@8
(1UPDATE/2REVOKE), totalU32@12, controlSequenceU64@16, helloNonce32@24,
bootstrapBinding32@56, manifest32@88, activeID16@120, handoffID32@136,
hardExpiresU64@168, pathEpochU64@176, pathID16@184, CPRBLengthU32@200,
reservedU32@204=0, CPRB SHA25632@208, binding32@240, CPRB@272. UPDATE CPRB
length112..656; REVOKE has length0 and SHA256(empty). IDs/nonces must be nonzero.
Reject trailing bytes, reserved/unknown operations and wrong captured scope.

Binding is SHA256 of ASCII `CODOL-P2P-ACTIVE-CONTROL-V1` plus one NUL, entire
HELLO192, CONTROL bytes0..239, then CPRB bytes. The digest field is excluded
from its own input.

ACK is raw96 bytes: magic8 `CPAKv1\0\0`, controlSequenceU64@8,
helloNonce32@16, binding32@48, resultI32@80, operationU32@84,
lastInstalledNativeSequenceU64@88. Result1 UPDATE requires the actual native
installer to return literal1 on the owning game fiber between Init/Frame calls,
with all permission checks still live. Native result2 is refusal. Result1
REVOKE requires actual quarantine plus the owner fiber observing native
Current==0; it reports denial, not engine exit or resource cleanup. Python must
keep dispatch paused until exact ACK and its separate active-controller check.

Control sequence and native update sequence advance exactly by1 without wrap.
Native epoch/counter/generation/owner and every UID/grant/source-endpoint entry
remain identical to the original CPRB. First successful promotion pins active
ID/handoff ID/hard expiry. Later changes refuse. CPRB expiry-issued<=30000ms,
expiry<=hard expiry, near-clock issuance and dual monotonic/wall expiry apply.
The earliest observed offered lease and initial hard monotonic budgets are
captured before native installation, reused across prepare/commit checks, and
never extended by late installation or a flat wall clock. Clock samples and
high-water updates share one lock; native calls run outside that lock.

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
the resulting HELLO/control/ACK. These are not Windows game qualification.
