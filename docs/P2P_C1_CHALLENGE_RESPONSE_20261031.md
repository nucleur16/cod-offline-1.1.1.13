# Exact C1 challengeResponse sender review, October 31

Image: `E:\codol-classic\C1.exe`, SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
This is read-only exact-image disassembly. No packet was injected and no C1
process was launched.

The connectionless dispatcher compares token 0 with `challengeResponse`
(`0xc47f10`) at `0x4966dd–0x4966f8`. State must equal 3
(`0x4966fe–0x496705`). The branch reads token 1 through `0x436af0`, converts
it with `atoi` at `0x496737–0x496740`, and stores the resulting integer at
`[edi+0x138]`. It then requests state 4 at `0x496754–0x496759` and copies all
six dwords of the packet source into `[edi+0x14..+0x28]` at
`0x49675e–0x496785`. There is no source-address comparison or nonce check
within this branch before the overwrite.

The two direct calls into this dispatcher are at `0x4971ac` and `0x497289`.
The visible caller blocks at `0x49717a–0x4971ac` and
`0x497257–0x497289` copy the source fields into call arguments without an
address comparison. This narrows, but does not settle, the possibility of an
earlier filter in the wider packet receive path.

The earlier `0x493a10` source check in the same dispatcher is reached only
for the preceding `hostquit` and `roundsdone` branches
(`0x496565–0x496640`); it does not guard `challengeResponse`. The two
`0x4aaad0` calls at `0x496680` and `0x4966cd` are command dispatchers that
can consume a packet before this branch. Their fallthrough handlers
`0x4b3140` and `0x4b3d60` also search command tables. An unqualified
registered handler or an upstream receive filter could still impose a
condition, so this pass does not prove end-to-end reachability from an
arbitrary sender.

The current client `hostgate.c` allows only configured lobby IPs, gateway
IPs, loopback, and explicitly vouched host IPs, with any port on each IP.
`HostGateVouch` has no caller in this local source. The private relay sends
from fixed local sockets, and the preserved C1 Guest receipt proves an
honest connection through `127.0.0.1:17761`, not a spoofed response. Thus a
public unvouched IP cannot use this branch in the current enforcing build;
another local process or a future vouched direct host may have a different
reachability story.

The following `connectResponse` state-4 branch uses a base-address
comparison against the address stored here. A sender accepted at state 3
could therefore choose the address that the next branch treats as pending.
This is a connection-integrity concern, not a demonstrated memory corruption
or RCE.

The exact C1 sender path resolves the intended-target question. In connection
states 3, 4 or 6, `0x495a59–0x495a81` loads the same decoded global client
object as `EBX`; state 3 jumps to `0x49607b`. There it formats
`getchallenge 0 "%s"` (`0xc47c48`) and passes `[ebx+0x14..+0x28]` as the
destination to `0x5a3c20` (`0x496095–0x4960c7`). Thus the pending address
is available before the response overwrites it.

The local client source now places an exact-byte-checked hook at `0x496729`.
The displaced six-byte instruction is `8b 3d 68 80 6f 01`. In state 3, the
hook compares the packet source against `[client+0x14]` with both C1 address
comparators before the original token parser runs. Both are needed: for type
4, `0x5a3580` checks `+4..+7` while `0x5a3700` checks `+0x14`; for type 5,
the latter checks `+0x08` and address bytes. On mismatch the hook takes the
existing zero-result return at `0x496716`; on match it executes the displaced
MOV and continues at `0x49672f`. If the image bytes do not match, client
startup refuses the hook rather than silently running the old path. This
binds the local candidate to the pending endpoint; it does not provide
cryptographic sender authentication.

A 32-bit Linux AddressSanitizer fixture executed the actual hook assembly
with C1 comparator ABI stubs. It accepted an identical source; rejected a
changed base, changed extra field and changed address type; skipped the
second comparator when the first rejected; and refused a mismatched hook
site. It also decoded the installed relative jump and verified its target.
Fixture SHA256:
`99ac247c3a8eabffe321c56eb42abe5eb02a69fccca4674fe66a124b4cce8e07`.
The same fixture passed at `-O2` with AddressSanitizer, matching the local
DLL build's optimization level; its SHA256 is
`f637260438cb68fda171c22fa97827ba2b1d42c27c63273873b83dce9c6f9b24`.
The full Windows-target local DLL candidate compiled, SHA256
`021a8479cddaff430f61a2188f74918d1c7e9d5e29ea7c7864ca04851eb70877`.
No stable Host/Guest DLL was replaced and no C1 client was launched.

The same candidate also binds the state-4 `connectResponse` to the pending
endpoint before it can replace the channel address. See
`docs/P2P_C1_CONNECT_RESPONSE_20261031.md` for that exact call site and
the shared fixture.

Exact source-port preservation in the private relay is documented, but an
actual C1 join with this hook and direct/NAT rebinding have not been tested.
The exact C1 type/field mapping beyond the compared offsets remains partly
unqualified. This fix is local and source/fixture/build tested only. Public
P2P remains gated by C1 parser, native gameplay and NAT/security acceptance.
