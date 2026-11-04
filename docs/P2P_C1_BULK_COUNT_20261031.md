# Exact C1 bulk record count guard, October 31

Target image: `E:\codol-classic\C1.exe`, SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
This is the same PE32 image as the task-owned Host C1.

The exact-image parser at `0x72fe30` accepts marker `0x81`, reads a 32-bit
count into `[*0x6c267dc + 0x7234d4]`, then loops at `0x72feca–0x72fee9`,
passing 0x40 bytes and an advancing destination to the bulk reader at
`0x59f770` for every positive count. A corresponding input path at
`0x73106a–0x7310fd` clears 0x4fc bytes beginning at that destination. The
first record begins 0x3c bytes in, leaving `(0x4fc - 0x3c) / 0x40 = 19`
records. Count 20 starts a write at the end of this fixed region. The bulk
reader sets the message overflow flag when input is exhausted, but the caller
does not stop its count-controlled destination loop on that flag. This is a
confirmed missing destination bound in the exact parser. A hostile-host path
to this parser, an actual C1 crash, or RCE is not yet established.

Direct calls to `0x72fe30` occur at `0x72e1d3` and `0x731271`.
The first lies in `0x72deb0`, reached through `0x72e6c0` and `0x72e700`.
The exact-image strings used on those paths say "DemoForward" and "DemoBack"
when a keyframe cannot be found; the parser's error says "bad keyframe
information while reading scores." The second call lies inside the
indirect stream reader at `0x730cf0`, reached by `0x731320`. This places the
observed bulk-record parsing in the demo/keyframe handling family. Whether
a shared or downloadable demo file can be attacker supplied has not been
established; these xrefs do not show live host packet reachability. The guard
is installed only after
checking the exact seven bytes at `0x72feca`:
`39 1f 7e 1d 8d 4f 3c`. It rejects counts above 19 before the first record
write, sets the message overflow flag and exits through the original epilogue.
For counts at or below 19 it reproduces the displaced compare, empty branch
and destination LEA. A mismatched image fails the required startup gate.

A second input path at `0x730cf0` loads the same global destination base at
`0x730d62`. Its indirect read callback reads the count at `0x731086`, then
passes successive 0x40-byte destinations to the callback at `0x7310d8`.
This path clears the same 0x4fc-byte region beforehand. Its postreader
`0x731390` also copies the count and records into a fixed 0x4c0-byte region.
The exact seven bytes at `0x7310c6` are `c7 45 f0 00 00 00 00`.
The second required hook checks the count before either record loop or copy;
accepted counts replay the displaced local initialization and original
count-vs-zero flags, while a count above 19 is reset to zero and skips the
loop. The indirect callback's precise input source is still unresolved, so
live P2P packet reachability is not established. This malformed-count fallback is a
bounded empty record set, not a demonstrated protocol-level rejection.

The postreader at `0x731390` also has a separate direct caller at `0x599b06`.
At `0x73144b–0x731455`, it copies each 0x40-byte record into the fixed
0x4c0-byte destination, then at `0x73145c–0x731466` uses the copied first
dword as an unchecked 0xb50-stride player-table index. An exact C1 iterator
at `0x70cf14–0x70cf53` traverses 19 player-table entries (`0..18`). A third
required guard at `0x731428` prevalidates count `0..19` and all record indices
`0..18` before the postreader loop. Malformed input clears the destination
count and skips that loop. It checks the exact six-byte signature
`83 3a 00 89 55 f0`. The postreader's independent caller at `0x599b06`
runs only when the bulk state flag at `+0x7239d0` is set, but the producer
of that state needs further tracing. This establishes an unchecked table-index
defect in the exact image, not a live P2P exploit path.

The x86 hook ran on a private executable page in a 32-bit Linux
AddressSanitizer fixture: the two reader hooks preserve the 0, 19 and negative-count
branches; count 20 sets overflow and exits in the direct reader and becomes
an empty set in the indirect reader. A mismatched signature at either site
refuses installation. The postreader fixture accepts 19 valid IDs, rejects
count 20, a record ID of 19, and a negative count, and clears the destination
count on rejection. A mismatched third signature refuses installation; failed
first-, second- or third-site partial writes roll back all affected jumps. The full local PE32
`Kernel33.dll` candidate linked with all three guards. A Windows-target object
disassembly confirmed the postread hook starts by reading the count from EDX
without a compiler-inserted thunk. These are hook and build checks, not a C1
load or hostile network test. Concurrent mutation of the shared source during
postread prevalidation and copy is not ruled out by this fixture.
The Linux fixture was compiled with `-fno-pie -no-pie` to match MinGW's
position-independent-code behavior for this naked x86 hook; Linux GCC's
default PIE inserted a thunk that overwrote EDX before the hook body.
Recheck command from this audit checkout:
`gcc -m32 -fno-pie -no-pie -O0 -g -fsanitize=address -fno-omit-frame-pointer tests/bulk_count_guard_fixture.c -o candidate-local/bulk_count_guard_fixture && ASAN_OPTIONS=detect_leaks=0 ./candidate-local/bulk_count_guard_fixture`.
Stable task-owned Host/Guest DLLs were not replaced or launched.

SHA256: `bulk_count_guard.c`
`43587ee4a04919514bb8de99c132ac9755b195a53aaa3039b17a8fb3f83fc669`,
fixture source `18907f58d49da8f8fbe2d46447fd986bda9dc74e770888ca686bfc145776f2fe`,
fixture binary `d10d1954ed5d0d037c7c9bf4c3bc5486c737edf60f267288a988f478193977c7`,
Windows-target object `70ba782815bdd882549600f97125611df28a9c70db3f6a10ed5c45a3fe4fd3a7`,
local candidate `Kernel33.dll`
`9221444ccd860c4d97efbad535d66016ec35432720595ef8134ba55a3dec7569`.
Stable Host/Guest DLL SHA256 remains
`d9aaa130510369ea54ce0bfb9e7b1f28ad7aa4a277e7a7d73db33ac86740a87c`.
