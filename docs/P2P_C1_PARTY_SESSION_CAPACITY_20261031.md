# Exact C1 party-session capacity stores, October 31

Exact `E:\codol-classic\C1.exe` SHA256:
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.

Initialization at `0x68c3b8..0x68c3d1` names three session records
`gameSession`, `partySession` and `searchSession`. The party dispatcher
uses first object `0x6d7cde8` and second object `0x6d7f4d8`; their
session pointers are initialized at `0x4aad01` and `0x4aad47`.

Session creation at `0x68ae70..0x68afcc` stores two unsigned byte
counts to `[esi+0x50]`/`[esi+0x54]` at `0x68af81..0x68af87` without
a visible sum cap. One direct caller at `0x4b38bf` supplies counts
derived from the total/public dvars; `0x4b38b4` can produce a
negative difference before the byte conversion. The shared setter
can therefore store a pair whose sum exceeds the 19-record capacity.
`party_session_capacity_guard.c` checks each value and their sum
before the two stores, writing zero for an invalid pair.

Session refresh at `0x68b3a0..0x68b3fc` writes two dword arguments
to the same capacity fields at `0x68b3ef..0x68b3f5` without a
visible cap. Its sole direct caller at `0x725c88` derives the pair
as `count+1` and `19-(count+1)` from a per-server count. If count
exceeds 18, one field can be negative despite a sum of 19.
`party_session_refresh_guard.c` rejects negative or over-19 pairs
before those stores. The original following EDX flags store remains
intact. Both installers verify the exact six original bytes and
replacement, failing client startup if either cannot install.

Each guard passed 32-bit ASan/UBSan register and patch-byte fixtures
at `-O0` and `-O2`. The refresh fixture initially caught a misplaced
NOP in the six-byte replacement; the corrected version passed.
Creation fixture SHA256: `-O0`
`3061f0e1d631d4891e8706824c75de8dd05aa65b907e5d4605b8b05343778ff2`,
`-O2` `53b854a8b29db1d245016c827fb23ed470cb76c05fd84035e36e2d6c0a62e7fa`.
Refresh fixture SHA256: `-O0`
`4718d383f93869cb47979fcc5893e7ca27abe99111dae001e502fbcc2f5eb597`,
`-O2` `a67d42f576cf70718bbc1842134fa8888ffb8b207fcdcd85c8a6e7f1ab98f08f`.
The full local PE32 DLL candidate SHA256 is
`c62377a3c9adff3661a4522c64576e7abc52405ad7e72288deac8157cd155aef`.
No stable DLL was replaced or loaded, and no actual C1 match or
private-relay packet exercised these guards. Other session setters,
protected readers and remote reachability remain open.
