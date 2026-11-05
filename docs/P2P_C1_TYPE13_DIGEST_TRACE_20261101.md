# Exact C1 type-13 digest follow-up, November 1

Target: `E:\codol-classic\C1.exe`, SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
This is read-only disassembly of the exact image. No C1 process or new Windows
executable was launched, and no malformed packet was sent.

The type-13 object's reader at `0x00b17790` first requires more than 20 bytes
remaining (`0x00b177d5..0x00b177e0`), then points at the received 20-byte
digest. It asks `0x00b17780` for the object's serialized size, subtracts 20,
and calls `0x00b1a000` over the received bytes after the digest
(`0x00b1780f..0x00b17828`). It compares a generated 20-byte result with the
received digest at `0x00b17853..0x00b17864` before reading the following
32-bit fields through the bounded helper `0x00a2f260`.

`0x00b17780` calls the count-only serializer `0x00b175d0` with null output.
That serializer initializes its cursor to 20, then accounts for five four-byte
fields with `0x00a2f1d0`. The helper advances the cursor by the requested
size even when no output buffer is present. This trace accounts for a
40-byte total and a 20-byte digest input, rather than an attacker-selected
negative length from the `subtract 20` instruction. It does not cover every
state and later consumer of the type-13 object.

`0x00b1a000` delegates its update to `0x00b1d540`. The latter validates an
algorithm index at `0x00a6e2a0` before using the algorithm table; the check
rejects indices above 31 and missing entries. The visible update code includes
an XOR-with-`0x5c` pass and two hash function dispatches, consistent with an
HMAC-style construction. The digest object copies 20 bytes from global
`0x014ffed8`; its initialization at `0x00b17a3d` calls protected/virtualized
code (`0x085ab9f0`, `0x08557f76`). The origin, secrecy, and per-session
binding of those bytes are not established. Therefore the digest comparison
is **not** proof of player authentication or of RCE safety.

The outer type-13 reader at `0x00b17d70..0x00b17f03` reads an optional
two-byte declared length through the bounded helper `0x00a2f260`, creates a
24-byte child object, and calls the inner reader at `0x00b17790`. On success
it compares consumed bytes with the declared length at
`0x00b17ea3..0x00b17ead`; a mismatch returns failure. The inner reader
commits its last two 32-bit fields only after their bounded reads at
`0x00b1794f..0x00b1796d`. Earlier fields can be written to this temporary
child before a later field fails, so this observation depends on the caller
discarding the failed child.

At the inspected top-level chunk dispatch, `0x00b188af..0x00b188d1` records
a failed type-13 deserialization; `0x00b185f0..0x00b185f2` exits before
processing another chunk when the previous chunk failed. The loop also caps
at 100 chunks (`0x00b185f8..0x00b185fc`). The type-13 object is destroyed at
`0x00b188fd..0x00b18923`. Thus, on this path, a failed digest or length
check cannot be followed by a successful later chunk that makes the parse
succeed. This is not a claim about every caller or protected parser path.

The local read-only PE helper was adjusted so raw reference and string/byte
queries work without Capstone; direct disassembly used the existing WSL
MinGW `objdump`. The helper reported the exact image hash and references to
`0x014ffed8` at `0x00b176fd`, `0x00b177f1`, and `0x00b17a40`.
The helper source SHA256 is
`e9ffdc88335847e3bfd01b58dc445d4e731db806fd8a9b56d14c274c59c3a35b`.

The source-only top-level bdPacket/DATA guards remain untested inside C1.
The protected key initialization, subsequent type-13 consumers, other chunk
types and hostile-packet behavior remain release gates. This pass did not
establish an additional memory-corruption path or justify a new binary patch.
