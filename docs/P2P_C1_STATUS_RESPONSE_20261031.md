# Exact C1 statusResponse append contour, October 31

Image: `E:\codol-classic\C1.exe`, SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
This was read-only disassembly; no client or service was launched.

The connectionless dispatcher compares the command with `statusResponse`
(`0xc480dc`) at `0x496b46–0x496b5a` and calls `0x49bf80` at `0x496b8b`.
That handler first searches a 16-record table (stride `0x202c`, from
`0x16fe330` to `0x171e5f0`) through `0x5a3700`. The comparator checks the
address type and, for type 4, the dword at `+0x14`; for type 5, it checks
the word at `+0x08` before address bytes
(`0x5a3700–0x5a37b1`). This is closer to endpoint matching than the
`connectResponse` base-address comparator, but is not a cryptographic
response token; the exact caller state and spoofing conditions remain open.

The handler reads the first line through `0x59f6d0` into a stack buffer at
`ebp-0x80c`. The reader checks the message cursor and stores at most `0x400`
bytes, then forces NUL at output offset `0x3ff` (`0x59f72a–0x59f74d`). The
handler formats that string into the selected record using capacity `0x2000`
at `0x49c077–0x49c083`. It copies status fields into a separate stack field
with index `<0x3ff` and a NUL terminator (`0x49c0d0–0x49c0ec`).

Later appends compute `0x2000 - strlen(record)` at `0x49c127–0x49c149`,
`0x49c192–0x49c1ba`, and `0x49c254–0x49c279`. The formatter at `0x6286a0`
forces NUL at `destination + size - 1`; the first write is capped at `0x2000`
and the subsequent appends preserve that terminator. Thus the observed
string append sizes stay between 1 and `0x2000` if the selected record has
its apparent `0x2000`-byte text field. The record stride is `0x202c`, and
metadata writes at `+0x2000..+0x2028` support that layout. This is not a
proof that every downstream status field or table lookup is safe.

The native server's fixed P-224 private scalar is a separate confidentiality
gate already recorded in the public RCE audit. Native key derivation uses
`Tiger192(shared_x || security_key)`, and the local DW code records recent
16-byte keys per seat, with some explicit owner-key fallbacks. A disclosed
seat key would permit reconstruction of that seat's past traffic from a
captured handshake because the server scalar is fixed. Whether a participant
can obtain another seat's key in the actual P2P handoff is not established.
Randomizing the scalar previously failed C1 record authentication, so this
inspection makes no compatibility-breaking crypto change.

No RCE was demonstrated or source patch warranted in the observed append
contour. Exact C1 response state, other connectionless commands, native
key rotation compatibility, live client behavior, and public/NAT qualification
remain open.
