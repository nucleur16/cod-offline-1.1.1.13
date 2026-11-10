# Exact C1 bdPacket guard recheck, November 1

Target: `E:\codol-classic\C1.exe`, SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
This was read-only disassembly. No C1 process was launched and no malformed
network packet was sent.

At `0x00b18510`, the top-level parser reads the two-byte declared chunk
area at `0x00b18573`, then subtracts that value from the received size at
`0x00b18583..0x00b1858f` without a visible physical-length comparison. The
source-only `BdPacketDeserializeGuard` rejects a declared area larger than
the received `size - 2` before forwarding to this entry.

The exact type-2 entry `0x00b16460` reads one type byte through
`0x00b18cb0`, then one flags byte, one subtype byte and a two-byte channel
field through bounded `0x00a2f260` calls. It conditionally reads area and
external-tail lengths through `0x00a2f400`. That helper consumes one byte
for values below `0x80`, otherwise masks the high bit and combines the next
byte as `(first & 0x7f) << 8 | second` (`0x00a2f42b..0x00a2f489`). This
matches `BdReadVar16` and the guard's five-byte fixed header. The type-2
reader passes both parsed lengths and both source pointers into its child
constructor at `0x00b165d7..0x00b165f6`; the constructor reaches protected
copy routines at `0x0865144d` and `0x085bf0dc`. The source guard checks
both source spans against their supplied physical lengths before those calls.

The current `tests/bd_packet_guard_fixture.c` passed as a 32-bit Linux
AddressSanitizer process, including both installed private-page trampolines,
wrong-signature refusal, valid forwarding, and oversized/missing area or
tail bytes. The fixture compiled to a PE/i386 object with warnings treated
as errors. Linux fixture SHA256:
`c234569fedffbe9da495828d2ad39362e9e2c1360fed6344fcfbae278a3a2a81`;
PE object SHA256:
`ada59391ee8e3292f85bc3f0e50b3250160569ac0a809e01568ea9923da098fd`.
Source SHA256: `bd_packet_guard.c`
`b0ef795a5adcc3dfd448485e21692585dfb098e0efc5fb9a84c7472186c312c4`;
fixture source
`acd75407e4d75d42150a69ed0d0815985a9fa8fc121dd0793cc5d8342a70f61f`.

This confirms the guard's source-level layout against the pinned image. It
does not prove the hook loaded in C1, that `Readable` covers every received
allocation boundary, or that other DemonWare chunk readers and protected
parsers are safe. Actual C1 malformed-packet behavior remains a release gate.
