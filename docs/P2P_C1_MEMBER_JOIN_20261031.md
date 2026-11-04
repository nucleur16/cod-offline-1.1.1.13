# Exact C1 memberJoin packet contour, October 31

Exact image: `E:\codol-classic\C1.exe`, SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
The disassembly and exact call-site check were read-only. A separate local
DLL candidate was built; no client was launched or stable binary replaced.

The connectionless party table at `0xc3d720` pairs `memberJoin` at
`0xc4bf10` with `0x4b1d00`. The same `0x4aaad0` digit-prefix party-mode
gate and `0x4b3140` command matcher used by `joinParty` precede it.
This is a separate handler; the three local `joinParty` guards do not
establish its safety.

Visible packet processing:

- `0x4b1d00` allocates a `0x4b4`-byte local frame and initializes a local
  record at `[esp+0x40]`. Party-state checks at `0x4b1d2e..0x4b1e27`
  precede the main parsing route.
- At `0x4b1f25` and `0x4b1f4c`, the handler asks protected `.tvm0` code
  at `0x8460f7f` to read a line with call-site argument `0x400` into a
  stack region. The actual reader implementation and its bounds remain
  unverified. The first returned text is copied with a `0x3f` argument
  and an explicit terminator at `0x4b1f39..0x4b1f60`; the second has a
  four-byte copy and terminator at `0x4b1f68..0x4b1f78`.
- Numeric and address fields are then parsed. At `0x4b2016..0x4b2037`,
  a visible party scan iterates 19 records before comparing against the
  configured player capacity. The handler calls `0x4ad370` to add or
  update the member. That function has a 19-entry duplicate scan at
  `0x4ad3ef..0x4ad41e`, but its earlier slot search at
  `0x4ad377..0x4ad3c2` uses the configured capacity. Its write helper
  `0x4ad150` copies visible name fields with `0x3f` and four-byte caps.

The earlier slot search at `0x4ad377..0x4ad3c2` uses the sum of the
configuration dwords at offsets `0x50` and `0x54` as its limit, while the
party record array has 19 entries of stride `0x108`. If that sum exceeds 19,
the search can index beyond the party array. The upstream bounds and packet
control of those configuration values have not been established, so this is a
conditional out-of-bounds hazard rather than a demonstrated remote exploit.

The dispatcher has two party objects: `0x6d7cde8` and `0x6d7f4d8` at
`0x49667b..0x4966cd`. Initialization at `0x4aad01` points the first
object's configuration to `0x8106488`, making its capacity fields
`0x81064d8` and `0x81064dc`. Direct stores at `0x466008..0x46600d`
derive these from a session structure and log via a string identifying
`CG_ParseServerInfo()`; stores at `0x49c3bc..0x49c3c2` derive them from
session dvar values in `CL_Live_SessionUpdate`. This traces the first
configuration to server-info/session state, but the upstream allowed
ranges and the second object's configuration remain unproved. It does
not establish that a remote sender can choose a value above 19.

More specifically, `0x465f57..0x465fa2` looks up two server-info text
keys (`2485` and `2491` in this image), converts each with `atoi` at
`0xada2a7`, and writes the results to `[ebx+0x168]` and `[ebx+0x16c]`
without a visible 19-slot comparison. At `0x465fbd..0x46600d`, if the
session-state gate is true, it writes the second value as public slots
and the difference of first minus second as private slots to the first
party configuration. The string at `0xc44718` identifies this as the
`CG_ParseServerInfo()` session modification path. Whether the two text
values are constrained before this function and whether a hostile P2P
host can supply them remain to be established. The memberJoin guard
does not make these other configuration consumers safe by itself.

Other visible party routines also use this capacity sum as an array loop
bound, including `0x4ab811..0x4ab84f`, `0x4ac2dc..0x4ac310`, and
`0x4ac3e3..0x4ac445`. The `memberJoin` call-site guard does not cover
those routines. Their call paths and capacity invariants remain a
separate review gate.

The sole direct call from this handler to the slot search is at `0x4b21b3`.
Its exact C1 bytes are `e8 b8 b1 ff ff`. The local
`member_join_guard.c` candidate replaces that call with a fail-closed
capacity check: both fields must be nonnegative, each must be at most 19,
and their sum must be at most 19 before the original call runs. Rejection
returns zero to the existing failure branch. The installer verifies the
original five bytes and the written call bytes.

`tests/member_join_guard_fixture.c` exercised the 32-bit call ABI, patch
bytes, valid boundaries, oversized/negative capacities, null pointers and
unexpected original bytes under AddressSanitizer at `-O0` and `-O2`.
The first run caught signed overflow for two `INT_MAX` fields; the guard
was corrected to check each field before adding. Both final variants pass.
The memberJoin-stage Windows-target PE32 DLL candidate built with SHA256
`87cc1a136cfcfa9e9687461460292a76263efb36f8acf925b319c8a4cdcec581`.
The later server-info-stage candidate supersedes it; neither has run in C1.

Exact fixture SHA256: `-O0`
`5252168d62cd33a35fec22ff0c8a15bc0bc26e46e9bc3db412f22a42c9308c38`,
`-O2` `a0e075cc1c4de60ad2e2d84b26f2f9aa3228945607411bdcd4ab8f811b04db30`.
Guard source SHA256:
`a7e922881ca86d70e5a131afd043f370f8ceaf7a2e1cd316ac33be60f434fd59`.

No working exploit was established in the visible `memberJoin` instructions.
The protected line reader, configured-capacity provenance, later helper
behavior and actual packet route remain open. The candidate guard is
compiled and fixture-tested, not actual C1 acceptance or release proof.
