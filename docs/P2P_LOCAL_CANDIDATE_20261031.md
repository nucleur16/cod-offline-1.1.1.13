# Local P2P security candidate, October 31

The compiled DLL is `candidate-local/Kernel33.dll` in this audit checkout.
It is ignored by Git and has **not** replaced either stable Host/Guest DLL.
No Windows game or server executable was launched for this build.

| Artifact | SHA256 |
| --- | --- |
| Exact `E:\codol-classic\C1.exe` and task-owned Host C1 | `d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60` |
| Candidate `Kernel33.dll` (current local artifact) | `be97375e08a906428f89283348c53de2b477f02e9e7b5ce6e9f2c3fd7ccb415b` |
| Existing stable Host/Guest `Kernel33.dll` | `d9aaa130510369ea54ce0bfb9e7b1f28ad7aa4a277e7a7d73db33ac86740a87c` |
| Embedded `unpacked.z` inflated body | `d4529b14cbcfd9a4108ee5f88421c72fe1370e9602dc3c9b272031c4934c531d` |
| 32-bit SDL2 static library | `7ddeae4fec4bbe29d9c10e9e7c81ede4d82fc85067875c392d91ad560b1d6fb0` |
| 32-bit Opus static library | `b7e4f2fdfff5652061b7b6f9f8224ad552a123c1cf227136a95cb118166e2825` |
| 32-bit zlib static library | `9d338fbe07f4a5a9b7d7e3d762c96e8f8540866201aafc5d7b21bf6135e05fbb` |

Build entry: `tools/build_local_audit_candidate.sh` under WSL with the
read-only dependency receipt at
`codol-zombies-fixes-20261029/death-machine-local-work/combined-client-current-host-r1/deps`.
The current candidate was rebuilt after `p2p.c` configured the task-owned
native relay identity path (source SHA256
`bb7462fcb0903f9801b01842700bd56c9618cf9485e8c1b388383c49e0b13221`).
The previous `c62377a3c9adff3661a4522c64576e7abc52405ad7e72288deac8157cd155aef`
hash identifies the earlier candidate without that binding. The current DLL
has not replaced either stable Host/Guest DLL or run in C1.
The PE is 32-bit x86 and contains the full `unpacked.z` byte sequence as a
resource. Its import list has no SDL2 or Opus runtime DLL.

The snapshot and BD parser guard fixtures passed in 32-bit AddressSanitizer
builds. The snapshot fixture additionally compiled as a Windows-target
object with warnings as errors (SHA256
`17d6cd65631762ee33cc0e45b5e8c048820f9b6fbb9988f236947bdc30fe7c8f`).
These tests prove compilation and the isolated hook mechanics. They do not
prove actual C1 packet handling or game compatibility.

The candidate was rebuilt locally after exact-image 19-record bulk-count
guards were added to both observed input readers and to their postreader.
The postreader is also called from a separate site and now validates each
record's player-table index. Their private-page 32-bit
AddressSanitizer fixture passed;
`docs/P2P_C1_BULK_COUNT_20261031.md` records the disassembly, bounds proof,
fixture hash and unresolved caller reachability. The candidate was not staged
into either stable task-owned client.

The candidate was rebuilt again after the in-process host zone-log truncation
guard in `p2p_inproc.c`. That stage had hash
`bf5d4ecb6dfae095b4a23433dc7ce8c4be10ce98f9c0b7f5f68fda2a249cdc6f`;
the earlier `9221444c...` hash identifies the preceding build. The logging
guard is compiled but has not been exercised by an actual C1 process.

The previous candidate SHA256
`021a8479cddaff430f61a2188f74918d1c7e9d5e29ea7c7864ca04851eb70877`
includes exact C1 state-3
`challengeResponse` and state-4 `connectResponse` sender guards. The prior
challenge-only candidate SHA256 was
`f527b53a319e40980bb64e9b2b05b1fa5ea53168290ea5d1ee59d47238e01a52`.
The updated 32-bit AddressSanitizer fixture passed with SHA256
`99ac247c3a8eabffe321c56eb42abe5eb02a69fccca4674fe66a124b4cce8e07`.
The hook has not been loaded into the stable Host/Guest setup; private relay
and direct/NAT C1 joins with it remain untested.

The candidate additionally includes three exact C1 `joinParty`
count and party-builder record guards. They bound both attacker-controlled
counts, combined party records, and filler records before fixed stack writes. The
32-bit ASan fixtures passed at `-O0` and `-O2`;
`docs/P2P_C1_JOIN_PARTY_COUNT_20261031.md` records the branch, stack
write, fixture hashes and reachability limits. No actual C1 packet has
exercised this candidate.

An earlier candidate added the exact C1 `memberJoin` slot-capacity
guard at the handler's sole direct call to its capacity-driven slot search.
The 32-bit ASan fixtures passed at `-O0` and `-O2` after a signed-overflow
case exposed and corrected an initial guard error. The prior candidate
SHA256 was `3d5db47f759106a7e0f2f760de21db7bade64f1f318d040e8f935477d34ffac8`.
See `docs/P2P_C1_MEMBER_JOIN_20261031.md`. No actual C1 packet has exercised
this guard; packet control of the configured capacity remains unproven.

The candidate was then rebuilt with a `CG_ParseServerInfo()` slot-capacity
guard. The prior memberJoin-stage candidate SHA256 was
`87cc1a136cfcfa9e9687461460292a76263efb36f8acf925b319c8a4cdcec581`.
The server-info guard rejects invalid total/public pairs before the first
party configuration can receive them. Its 32-bit ASan and UBSan fixtures
passed at `-O0` and `-O2`. See
`docs/P2P_C1_SERVER_INFO_CAPACITY_20261031.md` for the exact setter and
the still-open upstream, second-party and actual-C1 gates.

The preceding candidate also guarded the separate
`CL_Live_SessionUpdate` route into the first party configuration. The
preceding server-info-stage candidate SHA256 was
`dad79ef108c74c5194ad1ee18164285110e10f50f2142ee7ff1f21749dba78df`.
The first 32-bit sanitizer fixture caught a preserved-register error in
the new hook; the corrected direct-address version passes ASan and UBSan
at `-O0` and `-O2`, and Windows DLL disassembly confirms the expected
register behavior. See `docs/P2P_C1_SESSION_CAPACITY_20261031.md`.
No actual C1 packet or session has exercised this build.

The current candidate adds a 19-record bound to the separate party
refresh dvar loop and checks the shared session creation and refresh
capacity stores. The preceding candidate SHA256 was
`dffe63854a40b6ef669a1e84824f0c770478d7cba7ef2772682bed5c69511a26`.
The 32-bit ASan/UBSan fixtures passed at `-O0` and `-O2`; the refresh
fixture caught a patch-byte error before the corrected full DLL build.
See `docs/P2P_C1_PARTY_SLOT_LOOP_20261031.md` and
`docs/P2P_C1_PARTY_SESSION_CAPACITY_20261031.md`. No actual C1
session has exercised the new build.

The local DW/script service and Windows kernel pipe suite passed on October 31:
45 tests, 2 optional tests skipped. The first run had four false failures
because this Windows virtual-environment Python launcher delegated to a second
process: the PID returned by `Popen` differed from the PID the kernel reported
at the named pipe. The test child now reports its actual PID before connecting;
the production pipe's one-process PID check was unchanged. The 45-test rerun
passed in 6.775 seconds. This verifies the local IPC contract with a Python
child, not C1's native pipe consumer.

The subsequent native P2P short-datagram fix is in the separate native audit
source checkout. It is not part of this client DLL. Its exact two-byte NAT
regression failed under AddressSanitizer before the repair and the expanded
handshake fixture passed afterward; the Windows-target native object SHA256 is
`d791af01f6cb6a6f999947c2e9f536ddf9805acb4ffb6a9259de981ac9b2dabe`.
No native game/server build was linked or launched.

The October 29 human handoff prohibits further autonomous C1 launches after
the normal manual setup was prepared. An actual C1 load/test awaits a new
request. Then use only the existing task-owned stable executable paths,
verify current ownership, preserve the old DLL hashes/backups, and keep the
backend and P2P tests local. No push, deployment or production promotion.
