# Exact C1 connectResponse address check, October 31

Image: `E:\codol-classic\C1.exe`, SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
This is read-only disassembly, with no packet injection or C1 launch.

The connectionless dispatcher matches `connectResponse` (`0xc47f78`)
at `0x4967eb–0x496806`. It proceeds only in client connection state 4
(`0x49680c–0x49682f`). It copies the pending remote address from the
client object at `+0x14..+0x28` and compares it with the packet source
through `0x5a3680` (`0x49682f–0x49689b`). The wrapper calls `0x5a3580`.

The `0x5a3580` comparator checks address type. For type 5, it compares
the ten bytes at `+0x0a..+0x13` but does not read the two bytes at `+0x08`.
For type 4, it compares the four bytes at `+0x04..+0x07`, also without
reading `+0x14`. By contrast, the `0x5a3700` comparator used for server
browser replies explicitly compares `+0x14` for type 4 and `+0x08` for
type 5 before their address bytes (`0x5a3718–0x5a373f`). These observations establish that
the connection reply uses a base-address comparison, not the same exact
endpoint comparison. The exact type-to-IP-family mapping and every native
address field still need independent validation.

On acceptance, C1 passes the packet source address into `0x5a2160` at
`0x496937–0x496997`. That function clears a `0x6cc`-byte channel object
and copies the supplied address fields into it. A peer that can send from
the same base address with a different port during state 4 could therefore
affect which endpoint is stored. The current private TURN bridge accepts
native Host packets only from its fixed `127.0.0.1:17760` socket and sends
Guest packets from its fixed `127.0.0.1:17761` socket; it does not grant
remote participants arbitrary local source ports. A separate local process
or future direct/NAT path has a different threat model.

The preserved local Round 9 receipt (`lobby/docs/LOCAL_C1_OVERNIGHT_HISTORY_20261029.md`)
observed the actual Guest C1 reaching `CA_ACTIVE` at proxy port 17761
through forced TURN and then closing on the original short permission.
That is positive honest-path endpoint evidence, not a malformed or
different-port `connectResponse` test. It does not establish whether C1's
address type in that run was 4 or 5; no port-field mapping is inferred
from the console string alone.

The related native server source names address types 4/5 as IPv4/IPv6 and
defines an exact comparison as base address plus port
(`src/sys_net.h`, `src/sys_net.c`). This corroborates the comparator roles,
but native and C1 structure offsets must not be conflated; C1's type/field
mapping still requires exact-image or runtime confirmation.

No RCE, out-of-bounds write, or remote exploitation was demonstrated.

The local client candidate now replaces the exact `call 0x5a3680` at
`0x496891` (`e8 ea cd 10 00`) with a strict comparison wrapper. C1 has
already pushed two 24-byte addresses by value: packet source first and
pending destination second. The wrapper calls both `0x5a3580` and
`0x5a3700` and returns true only if both report equality. This covers the
distinct type-4 fields that each comparator checks. The original caller's
branch and stack cleanup are unchanged. If the call bytes differ, candidate
startup refuses the hook.

The 32-bit Linux AddressSanitizer fixture called the actual wrapper with
identical, changed-base and changed-extra addresses, and checked exact-site
installation/refusal and the installed relative call target. Fixture SHA256:
`99ac247c3a8eabffe321c56eb42abe5eb02a69fccca4674fe66a124b4cce8e07`.
The same fixture passed with AddressSanitizer at the DLL candidate's `-O2`
optimization level, SHA256
`f637260438cb68fda171c22fa97827ba2b1d42c27c63273873b83dce9c6f9b24`.
The full Windows-target local DLL candidate compiled, SHA256
`021a8479cddaff430f61a2188f74918d1c7e9d5e29ea7c7864ca04851eb70877`.
The stable Host/Guest DLLs were not replaced or launched.

Port changes during legitimate direct NAT traversal have not been measured;
strict endpoint binding may reject such replies. The existing private relay
uses fixed local ports, but no C1 join with this candidate was run. Before
public direct P2P, qualify NAT behavior and use authenticated endpoint
binding appropriate to the chosen transport. Public P2P and dedicated
capacity retirement remain gated.
