# P2P public RCE review and host-filter repair

This is a bounded audit of public vulnerability disclosures and the selected
CoDOL implementation. It is **not clearance for arbitrary player-hosted matches**.
No exploitable C1 RCE was demonstrated or ruled out. No production component was
restarted, replaced, or retired. No exploit payload was sent to a game client.

## October 31 local security follow-up

An additional short-datagram native receive defect was confirmed in
`src/codol/net/codol_network.c`. After a two-byte minimum check, a NAT-shaped
datagram `{13, 2}` reached `packet[2]` before testing for the required
29-byte length. The failed-DATA diagnostic also indexed bytes 2–5 while the
old minimum remained two. A 32-bit AddressSanitizer fixture using an exact
two-byte stack buffer reproduced the NAT read at source line 506 before the
repair. The receive path now rejects lengths below 16, the shortest accepted
handshake/record, and checks the NAT length before inspecting its fields.
The expanded handshake fixture passed under 32-bit AddressSanitizer after the
repair; the Windows-target native object compiled, SHA256
`d791af01f6cb6a6f999947c2e9f536ddf9805acb4ffb6a9259de981ac9b2dabe`.
This is a proven source out-of-bounds read, not demonstrated RCE or a live
Windows game/server test. The native game build was not linked or launched.

Exact C1 disassembly also confirms a DemonWare receive weakness. During the
QoS pump, `0x4dd6b0 -> 0x4dcfd0 -> 0xa4d7e0 -> 0xb0fb50` receives at most
`0x508` bytes into a stack buffer. The fresh-connection route passes the
actual received length to `0xb18510`, whose parser reads a 16-bit declared
chunk-area length at `0xb18573`. At `0xb18583-0xb1858f` it uses that length
without first requiring it to fit the received datagram; subsequent reads use
the declared length. This supports an out-of-received-datagram read/crash
contour, consistent with the earlier `0xae00f7` copy-read crash. No
out-of-bounds destination write or RCE was demonstrated. The source-IP host
gate does not check bytes from an allowed host, so this remains an open
client-side release gate. The static result has not been tested with a
malformed packet in C1. Local `bd_packet_guard.c` now installs two
exact-signature C1 entry guards before game startup: the packet guard rejects
declared chunk areas larger than the physical datagram, and the type-2 DATA
guard checks both area and external-tail payload lengths before C1 copies
them. The type-2 parser at `0xb16460` receives the physical tail length but
did not use it; both copy source spans are now checked in the wrapper. A
32-bit AddressSanitizer fixture using the actual guard source passed valid,
truncated, oversized, null-tail and cursor-boundary cases, plus a wrong
instruction signature and an installed jump/trampoline with x86 `thiscall`
forwarding on private Linux pages. A Windows-target fixture object compiled
with warnings treated as errors, SHA256
`b828ed4e635bfd320cd8a2337e3f81c7877dbcac8abc7a7573e225b04dfbb9b2`.
This verifies these two source guards and fixture ABI; it is **not** an
actual C1 parser run. Other DemonWare chunk types and inner readers remain
unqualified, and the rebuilt candidate has not been staged or loaded in C1.
The installer now checks both exact entry signatures before changing either
one; the fixture verifies a mismatch at either entry leaves the packet entry
unmodified. A second static pass through chunk types 1-14 found no further
clear out-of-datagram copy in the examined handlers. Types 4 and 5 use the
bounded reader for variable fields, while type 13 enters partly opaque nested
crypto code. This narrows the known BD contour but cannot establish full
parser safety without actual C1 malformed-input testing.
The November 1 exact-image recheck confirmed the type-2 header field order
and variable-length encoding against `0xb16460`, `0xb18cb0`, and `0xa2f400`.
The current source guard's 32-bit ASan private-page fixture and PE/i386
fixture object compile passed again. See
`docs/P2P_C1_BD_GUARD_RECHECK_20261101.md` for addresses and fresh hashes.
The guard still has not been loaded into C1, and no hostile packet was sent.
The checkout's normal MSYS2 build setup is absent here. A full candidate was
later linked with WSL's existing MinGW cross compiler and local dependency
receipts, as recorded below. No newly located Windows build executable was
launched.

The native sender's `c1_store_inert` guard had an escape: after 17 consecutive
sealed records with a byte that could make C1 parse a DemonWare chunk, it sent
the final unsafe record. It could also send a nonzero but sub-16-byte result
from the seal function. Both cases now drop the packet after consuming the
reserved counter; a later valid record can still be sent. The actual
`src/codol/net/codol_network.c` send path passed an AddressSanitizer fixture
covering ordinary sends, three unsafe retries, retry exhaustion, short seals,
counter persistence and recovery. WSL's 32-bit MinGW cross compiler produced
a Windows-target object, SHA256
`9e61a95b15fec6323bb63b21cd0b6c6d816d652278b3b787aaf7d2a5177f23ee`.
No Windows game/server executable was linked or launched. This sender repair
does not secure C1 against arbitrary packets from a malicious allowed host.

The child-host loopback RCON status socket was unconnected, so a status datagram
from another endpoint could satisfy the readiness text check. It now connects
to the intended `127.0.0.1` server port before sending and receiving, and
rejects a truncated command buffer. The in-process host now applies the same
map/session/gametype/category status predicate already used by the child path
before publishing readiness. The existing exact-status predicate fixture
passed in WSL. These are source-level control-integrity repairs, not
demonstrated RCEs; the revised client candidate has not been exercised in
actual C1.

The Opus receive path was checked against the public caller-buffer pattern in
[FreeRDP GHSA-72j9-356v-88xq](https://github.com/FreeRDP/FreeRDP/security/advisories/GHSA-72j9-356v-88xq)
and [PJSIP GHSA-j59p-4xrr-fp8g](https://github.com/pjsip/pjproject/security/advisories/GHSA-j59p-4xrr-fp8g).
Here `VoicePull` passes `VOICE_FRAME=960` mono samples and uses a 960-float
`g_tmp` output buffer; the reviewed voice relay caps each encoded frame at
255 bytes and checks each length against the received payload. The analogous
caller output-buffer mismatch was not found in this path. This does not
validate every codec/library version or every native voice ingress path.

The native server also accepted a decrypted four-byte `0xffffffff`
connectionless packet into `SV_ConnectionlessPacket`. That dispatcher read
`msg->data[4]` in three binary-query branches and compared 20 bytes starting
at `msg->data+4` in the Source query branch, without checking the declared
message length. The backing `plain[65507]` array makes this an
out-of-*message* read of prior/uninitialized stack contents, not a demonstrated
out-of-allocation read or RCE. Local source now rejects connectionless packets
shorter than five bytes and requires 24 bytes before the 20-byte comparison.
A focused AddressSanitizer fixture linked the actual `SV_ConnectionlessPacket`
from `src/sv_main.c`: a four-byte marker and zero-length packet returned
before parsing; a 23-byte packet bypassed the 20-byte query comparison; a
24-byte packet reached it. Other handler dependencies were stubbed for this
fixture. This tests the length branches, not full server networking.
Review of that comparison uncovered a shared `Q_strncmp` off-by-one: it loaded
the `(n+1)`th byte before returning equal for the first `n` bytes. Its sibling
`Q_stricmpn` did the same. Both now stop after comparing exactly `n` bytes,
including an early return for nonpositive `n`; ordinary comparison ordering
is preserved. This removes a possible one-byte out-of-allocation read at
callers with exact-sized buffers, though no remotely exploitable caller or RCE
was demonstrated. An AddressSanitizer test of the actual `src/qshared.c`
functions passed for 20-byte nonterminated buffers, last-byte differences and
zero-length calls. The test placed the buffers directly before inaccessible
guard pages. At `-O0`, the original source crashed on the exact 20-byte match
in each function, while the patched source passed at `-O0` and `-O1`. At
release-like `-O1`, the original compiler eliminated the positive-count extra
read in this fixture, but the original `Q_strncmp(NULL,NULL,-1)` still crashed.
These results prove the source defect and the repair, not a reachable RCE in
the exact Windows game/server build. The fixture ran inside WSL.
The exact `src/sv_main.c` translation unit passed Linux `gcc -fsyntax-only`
and produced a Windows-target object with WSL's i686 MinGW cross compiler,
`CODOL_SERVER` enabled; object SHA256
`47353369012e9cc96a04e218e79391d67f328036fd315bbafeb3d2e317dcd48b`.
The corrected `src/qshared.c` also produced a Windows-target object, SHA256
`79947a73c5d1a029980797d1c6145e030c1c1d280c8f9505d1600f1ee6eec73e`.
No Windows game/server binary was linked, launched, staged, or exercised with
hostile packets. The remaining compiler warnings were pre-existing and outside
this change.

The configured-master TCP response path in `SV_HeartBeatMessageLoop` also read
a signed nested length and previously rejected only oversized positive values.
It could pass a negative or truncated length to `MSG_InitReadOnly` after
`MSG_ReadData` copied too little. The local source now requires at least four
bytes, retains the old 8192-byte exclusive ceiling, and checks that the claimed
length remains inside the received response before copying. This is a distinct
configured-master trust boundary, not evidence that a P2P participant can reach
it. The updated `sv_main.c` Windows-target object is the SHA256 above; no
runtime response injection was performed.

The direct ICE prototype's `IcePeer.recv` previously counted rejected encrypted
game frames but continued indefinitely. A participant able to send data on its
authorized ICE path could keep the receiver busy with invalid frames. This is an
availability weakness, not demonstrated RCE or a bypass of game-frame AEAD.
The source now closes that peer after 64 consecutive invalid frames; an ordinary
authenticated frame resets the streak while the total diagnostic count remains.
Ordinary authenticated frames still pass and invalid frames never reach the native
bridge. Four focused tests passed with the existing approved Python runtime, including a real loopback
ICE pair and a 64-frame malformed receive sequence. This source fix has not been
repacked into the private C1 build or exercised in actual C1 gameplay. It affects
the standalone direct ICE path; the overnight full matches used the private TURN
bridge, whose separate counters and expiry were already tested. No clients,
servers, installed files, production services or firewall rules were changed.

The native server still has a fixed ECDH scalar (`codol_network.c`). Source review
confirms it is used for each CoDOL handshake. Its confidentiality impact depends
on the separate admission security key, but it removes forward secrecy if that
key is exposed later. Replacing it requires exact C1 compatibility testing;
changing it in this review would strand current clients. This remains an open
release gate rather than a claimed repair.

The task-owned Host `C1.exe` and read-only `E:\codol-classic\C1.exe` have the
same 33,608,704-byte image and SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
A PE-header check found image base `0x400000`, `.text` at RVA `0x1000` and the
previously documented packet path at VA `0x497360` inside it. Exact-image
disassembly corrected the earlier audit's label: `0x497360` dispatches
connectionless packets to `0x497080`; it is not the reliable-command handler.
The actual 256-slot reliable-command sequence/index checks are at
`0x487730–0x4877d5`. The old audit's address and direction-of-bound claim were
inaccurate. This correction does not establish safety of downstream commands,
gamestate/configstrings or snapshot fields.
Further exact-image decoding corrected the prior audit's byte-reader label:
`0x49d934` is an inlined read in the snapshot region, while the standalone
byte reader is at `0x5a00f0`; `0x59f3a0` checks a four-byte read. Those
individual reads check the cursor against the declared message size. The
previous inference that this clears the whole snapshot cursor path was too
broad. Further exact-image review found four snapshot write-bound defects,
patched in local source but not yet exercised in C1:

- A full snapshot increments the entity count at `0x49ca69` after a
  `0x114`-byte write into the static array at `0x6d36abc`. Its capacity is
  1024 entities; the 1025th write starts at `0x6d7babc`, a separately used
  global. The later `CL_GetSnapshot` cap cannot undo the parser write.
  `snapshot_guard.c` checks count `< 0x400` and entity number `0..0x7fe`
  at exact entry `0x49ca00` before forwarding, otherwise sets the message
  overflow flag that the caller uses to stop its loop.
- The playerstate field decoder at `0x5a19c0` accepts a run index beyond its
  245-field descriptor table and tests the index against the decoded count
  only after a field writer call. A short malformed run can select index 294;
  the resulting descriptor points the writer far beyond the `0x3400`-byte
  snapshot allocation. The source guard at `0x5a1b19` rejects indices above
  the decoded count or at/above 245 before skip-copy and descriptor lookup.
  The equal-to-count slot is valid: C1 reads that terminal descriptor before
  its loop compares `EDI < count` at `0x5a1ba1`. A local C1 Outpost session
  showed one rejection per full snapshot with the earlier `EDI >= count`
  guard, followed by repeated invalid-frame deltas and no `CA_ACTIVE`.
  The corrected boundary passed the 32-bit address-sanitized hook fixture;
  gameplay acceptance still needs a live C1 run.
- The entity field decoder at `0x5a0c00` checks its decoded field count
  against the selected table size, but its first run index can exceed that
  count. The skip-copy loop and descriptor access at `0x5a0d5a–0x5a0db1`
  occur before the later index comparison. A guard at `0x5a0d52` checks
  the index against the decoded count and the maximum 66 fields before
  either access. A real local C1 Outpost full snapshot used `ESI=48` with
  decoded count `48`; the earlier `ESI >= count` guard rejected each such
  frame and C1 then logged repeated invalid-frame deltas. The equal-count
  terminal descriptor precedes C1's `ESI == count` test at `0x5a0dbc`.
  The corrected guard permits equality while still rejecting indices above
  count or at/above the 66-entry table limit. The updated 32-bit ASan
  fixture passes; live C1 gameplay remains to be retested.
- The shared delta decoder at `0x5a0a40`, used by two more snapshot
  record parsers and other field readers, validates the inclusive last field
  index but lets a wire
  run overshoot it. Skip-copy and descriptor access follow before the later
  comparison. The guard at `0x5a0ae4` checks the prospective index against
  both the decoded last index and the table maximum. Its reject path also
  removes the pending run-reader argument before taking the original clean
  exit, matching the exact stack layout.

The adjacent snapshot record decoders at `0x49cec0` and `0x49d2f0` use ring
destinations with configured moduli and call the shared decoder above for
their delta fields. `0x49d700` writes 0x2c-byte ring records and its
`0x5a1170` field switch bounds the four-bit selector to eight cases before
dispatch. These observations narrow those paths but do not clear every
consumer or the protected game-message dispatcher.

The exact snapshot header at `0x49d95f..0x49d981` stores the decoded flags,
resets the static full-snapshot count when bit 8 is set, and branches on the
delta reference. In the entity append routine `0x49ca00`, the same bit 8
selects the static `0x6d36abc + count*0x114` destination; the other branch
selects a ring destination using `[edi+0x9cc8]` and `[edi+0x9ce0]`.
This supports applying the 1024-count guard only to the full branch. The
areamask size and copy path were not identified in this pass.

All guards preflight exact bytes at all four sites before patching. Their actual
x86 jumps and trampolines passed a 32-bit ASan private-page fixture for
boundary, bad-signature and overflow cases. A Windows-target fixture object
compiled with `-Werror`, SHA256
`17d6cd65631762ee33cc0e45b5e8c048820f9b6fbb9988f236947bdc30fe7c8f`.
Fixture entry point from this checkout under WSL: compile
`tests/snapshot_guard_fixture.c` with 32-bit GCC, `-fno-pie -no-pie`,
`-fsanitize=address -fno-omit-frame-pointer`, then run it with
`ASAN_OPTIONS=detect_leaks=0`. Compile the same source with
`i686-w64-mingw32-gcc -O2 -Wall -Wextra -Werror -c` for the Windows-target
object. Linux PIE was disabled so the naked entry hooks preserve C1's live
registers; the fixture checks the relocated instructions' outputs.
This is a static memory-corruption finding and a source-level mitigation, not
proof of host-to-client packet reachability, RCE, or game compatibility. No
updated DLL was loaded into C1, and no hostile packet was sent to C1.
Gamestate/configstring, other snapshot fields and areamask handling remain
unverified.
An October 31 exact-image reference pass found the later `CL_GetSnapshot`
copy at `0x48709a–0x4870c9`: it clamps the reported entity count to 1024 at
`0x48705e–0x48707d` before copying from the static entity array. That later
copy cap confirms why the pre-write parser guard is still necessary. Direct
references to the array base also occur in the entity parser at
`0x49c9c0–0x49ce8f`. Two raw `.text` reference candidates near `0x72f621`
and `0x72f65d` did not decode as ordinary code in this pass, so they are not
classified as safe callers or as proven writes.

The current local-testing source also linked into a full PE32 `Kernel33.dll`
candidate using `tools/build_local_audit_candidate.sh` and existing 32-bit
SDL2, Opus and zlib receipt libraries. Its SHA256 is
`0b3397ae0345d1e95996c03edefd460746f8d5678c5693e9ca498cbadf3f8adb`.
The embedded `unpacked.z` was byte-compared with the exact C1 mapped region
(match), then found intact in the candidate resource. Its import table has no
SDL2 or Opus runtime DLL. This proves a full local compile and link, not a
client load or game-networking result. The task-owned stable Host and Guest
copies still have their previous `Kernel33.dll` SHA256
`d9aaa130510369ea54ce0bfb9e7b1f28ad7aa4a277e7a7d73db33ac86740a87c`.
The October 29 human handoff forbids another autonomous C1 launch without a
new request; the candidate remains in ignored `candidate-local/` and no
stable client copy was changed.

Additional source inspection found that the native immutable-source packet
parser checks aggregate length, each name/source length, cumulative source
size, canonical paths and duplicate names before publishing its clone. Its
`fs:` reader's one-byte terminator is backed by a caller allocating `size+1`.
This is a source-bound review, not hostile-client or release qualification.
On the exact C1 image, the connectionless line reader at `0x59f6d0` writes
only while its destination index is below `0x400` and forces a terminator at
offset `0x3ff`; its read path checks the declared message size before loading
a byte. This is one bounded primitive, not proof that every connectionless
handler or downstream infostring parser is safe.
The bulk message reader at `0x59f770` adds cursor and requested count with
32-bit arithmetic, so a caller-controlled extreme count would warrant review.
Raw direct-call xrefs in the on-disk `.text` were enumerated; inspected network
call sites pass small constants (`0x4a6149`, `0x4b1ef8`, `0x730367`), or a
locally calculated count capped at `0x4d8` (`0x5cf904`). No attacker-controlled
extreme-count caller, out-of-bounds write, or RCE has been established. Other
callers and indirect dispatch still need control-flow qualification.
The November 1 follow-up inspected further direct callers in the same exact
image: `0x4a6fc6` passes a fixed `0x3d`, `0x4b311f` passes eight,
`0x726886` passes `0x3d`, `0x72fc97` passes `0x1c`, and `0x72e1ee` passes a
fixed `0x7ffc` into `*[0x6c267dc]+0x4c`. The last call sits after a
`0x20000`-sized message/buffer setup, so it is not an example of a wire-driven
extreme count overflowing the reader's 32-bit cursor sum. Its destination
allocation and whole caller's reachability remain unproved; these static
observations do not clear gamestate or other bulk-read callers.
The `0x5cf904` count path was checked again against the exact image: the
byte-read index is rejected by an unsigned comparison when above 26 (including
the reader's `-1` error value). Each accepted index uses a stride of `0x4d8`,
and the copied count is the smaller of `0x4d8` and `0x8000 - index*0x4d8`.
At index 26, the last 528 bytes end exactly at the 32 KiB buffer boundary.
This closes the extreme-count concern for that direct caller only; it does not
prove the destination allocation, other callers or whole message safe.
The C1 gamestate/configstring, remaining snapshot fields, BD/party and zone
parsers remain high-priority untrusted-host security gates.
On the host-facing native path, source review followed authenticated DATA
through `SV_PacketEvent` into `SV_ExecuteClientMessage`: CoDOL checks the
remaining decrypted length before its bounded copy, validates the three-bit
command selector, and bounds usermove count to its local arrays. The separate
client-command reader stages at most its destination capacity and commits only
after a NUL-terminated, fully bounded decode. These checks narrow the reviewed
server-side packet contour; they do not validate every command's game logic or
the separate C1 client parsers.

## Reviewed inputs

- Client patch: `CODOL-EMU/codomp_clientShipRetail-patch` main
  `2e08b66ea8efdf1ff95114fdffc58a0dc211a0a3`. Its main history contains the P2P
  supervisor, in-process experiments and newer localhost lobby integration.
  `dev@dc259f53` was located first, then the audit advanced to main.
- Native server: `nucleur16/codol_1.1.1.13_alpha_server` main
  `70ac47806f23a8b05c90bdb3e5104e2cd7ab7362`.
- New backend integration baseline: private `zzVertigo/codol-classic`
  `8ec07788589e0f3be8ddb73de6410550d6793d6f`.
- Additional GitHub inventories: CODOL-EMU lobby, rawfiles, OLfinal client/server,
  private Classic application and original dedicated repository. OLfinal targets
  a different build and is not evidence of 1.1.1.13 safety.
- Read-only C1 reference on this machine: timestamp `0x5314def0`, SHA-256
  `d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
  At `0x496482` it has the voice patch's expected instructions. This dump's hash
  is recorded independently; a timestamp or matching hook does not establish
  equality with every distributed executable. The embedded `unpacked.z` is only
  the high-address loader range, not a complete replacement for the C1 text.
  This reference has a `.tvm0` section; the earlier audit's VMProtect/`.vmp0`
  attribution is not confirmed for it. Missing plaintext string references do
  not by themselves establish virtualization, reachability or missing bounds.

## Public disclosures and applicability

| Public issue | Evidence in this implementation | Assessment |
| --- | --- | --- |
| [CVE-2018-20817](https://nvd.nist.gov/vuln/detail/CVE-2018-20817), unchecked Steam authentication blob | No `SV_SteamAuthClient` or `authBlob` implementation in the reviewed native server source/assembly. CoDOL bypasses the legacy reliable/SApi envelope in `src/sv_client.c` and rejects its sequence in `src/sv_main.c`. | The reviewed native route does not expose this Steam-auth path. Absence of a C1 string does not prove absence of an equivalent binary parser. |
| [CVE-2019-20893](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2019-20893), [public MW2 JoinParty disclosure](https://github.com/BlastsMods/JoinPartyRCE) | The *exact CoDOL C1 image* independently has a connectionless `joinParty` handler at `0x4b1520`. Token 7 controls an unbounded stack write loop. Token 3 can wrap a later signed capacity addition. The local candidate bounds both counts at `0x4b17cd`, combined records at `0x4ac8c0`, and filler records at `0x4ac945`. An exact-image trace follows the `0xffffffff` marker check at `0x497360` through the tokenizer, party dispatcher, and `joinParty` handler. The isolated relay carries native-host packets to the Guest client endpoint on loopback; `HostGateOk` permits loopback. Actual C1 dispatch and the handler's party/state gates remain untested. | Confirmed analogous C1 memory-corruption defect and local source patch. The CVE identifies a different MW2 build; no exact CVE inheritance or working CoDOL RCE is claimed. Actual C1 and hostile-packet tests remain open. |
| [CVE-2018-10718](https://nvd.nist.gov/vuln/detail/CVE-2018-10718), [separate MW2 Huffman overflow](https://github.com/momo5502/cod-exploits) | The reviewed CoDOL native receive path has a size check before copying and does not use the IW4 Huffman expansion path. Protected C1 parsers remain partly unreviewed. | No corresponding exact CoDOL defect has been established from this separate disclosure. |
| [IW4x public security fixes](https://github.com/iw4x/iw4x-client/blob/main/src/Components/Modules/Security.cpp), Huffman expansion, deferred packet copy, relay and UI directive boundaries | CoDOL native receive path copies after a size check instead of using Huffman. C1's gamestate, snapshot and BD paths are distinct native code. | Honest-server receive bounds are present. Client-side equivalents still need exact-build analysis and hostile-input regression coverage. |
| [IW4x disclosure and later correction](https://forum.alterware.dev/t/important-security-notice-remote-execution-vulnerabilities-found-in-iw4x/86), oversized localized OOB text | The June 2024 correction identifies unchecked copying in localization; the earlier `va` explanation is not reliable. C1's connectionless dispatcher is native and includes protected call targets. | No verified CoDOL instance. A source-IP allowlist does not validate an allowed host's text or other payloads. |
| IW4x `openLink` / HTTP helper attack chains, documented in the same maintainer disclosure | Kernel33's known local commands, supervisor configuration and launcher bridge do not add that IW4x script API. Native downloadable zones/IWDs remain a separate parser surface. | Do not import IW4x-specific conclusions into C1. Untrusted content loading remains unqualified. |
| [IW4x r5243 security release, October 17, 2026](https://github.com/IW4x/iw4x-client/releases/tag/r5243) | Maintainers report fixes for malicious-server or crafted-file crashes/memory corruption, file access, player impersonation and command-bearing links in their separate MW2 build. This cross-check does not identify corresponding exact C1 call sites. | Keep C1 download/zone, party identity and server-message review open. Do not infer an RCE or transplant build-specific patches from the release summary. |

The old `SECURITY_AUDIT_CLIENT_20261026.md` is evidence of an earlier pass, not
a security certificate. Its unproven native rows 14–18 remain open. Likewise the
old server audit's broad shared-IP admission finding was narrowed: current
`CODOL_AdmissionMember` requires an exact UID/member or UID/IP row. The UID
comes from the client-controlled connect request. When two members share the
host key and source IP, this check alone cannot distinguish them; the later
occupied-slot repair and remaining first-connect gate are described below.

## Confirmed host-filter defects repaired here

These are security-boundary defects and an availability weakness, **not confirmed
RCEs**:

1. Shipping accepted `+set k33_hostGate 1`, forwarding denied hosts in log-only
   mode despite the stated shipping trust requirement. Only explicit local-test
   builds can now use this mode.
2. Missing or mismatched import slots caused `InstallHostGate` to return while
   C1 startup continued. It now returns a checked success result; the caller
   stops startup on failure. The resolver import is validated too, and all
   three installed pointers are read back before success.
3. A continuously readable queue of denied datagrams could keep one receive
   call draining indefinitely. Each call now yields after 64 rejected packets.

The filter still trusts configured lobby addresses, loopback, SSDP/default
gateways and any explicitly vouched address. It operates on IPv4 addresses,
not authenticated packet contents. It is not a firewall against source spoofing,
a compromised router/lobby, a malicious approved host, or native parsing bugs.
No remote host-ticket path is enabled by this change.

## Verification

`tests/hostgate_windows_fixture.c` includes the actual filter/installer and runs
in a disposable 32-bit Windows process with a private surrogate import table.
It never loads a game. Ten isolated scenarios pass in each build profile:
unmapped/missing imports; substituted receive/send/resolver imports; each of the
three failed writes; successful installation despite the diagnostic switch;
and packet allow/drop, peek consumption and bounded-drain behavior. Queue
readability uses an actual loopback UDP socket; packet source metadata is a
test double. These are **20 profile/scenario runs**, not real C1 acceptance.

Compile each profile with MinGW GCC13:

```
i686-w64-mingw32-gcc -std=c99 -O2 -Wall -Wextra -Werror -Wno-cast-function-type -DCODOL_LOCAL_TESTING=0 tests/hostgate_windows_fixture.c -o hostgate-0.exe -lws2_32 -liphlpapi
python tests/test_hostgate_windows.py hostgate-0.exe
```

Repeat with `CODOL_LOCAL_TESTING=1`. The normal Windows test runner includes
both profiles. The native reviewer separately compiled and ran eight exact-main
source fixtures: handshake guard, vote guard, info strings, data fast path,
peer pool, session reservations, connect parsing and gamestate validation.
All passed; some deliberately use network/crypto doubles. Server gamestate
validation checks its own output, which a modified host can bypass.

No full client DLL or native game runtime is qualified by these fixtures.
Real C1 startup, normal lobby login and trusted-server joining remain required
before distributing these client bytes. No binary deployment is part of this PR.

## Additional exact-C1 bounds repairs, October 29

Continued static review found unchecked localized-text expansion in the exact
C1 string helper `0x5F4DE0` and independent numeric formatter `0x5F4BF0`.
Their ordinary caller output contract is 1024 bytes; expansion did not check
remaining capacity. This establishes a code defect, not remote exploitability
or an RCE demonstration. No malformed network payload or original vulnerable
routine was executed for these checks.

Both now have bounded replacements with exact target/caller guards, validated
argument indices/counts, reserved termination and no original fallback. The
numeric replacement preserves the measured ECX/stack calling convention and
native two-slot temporary-string lifetime through a guarded, fixed-format
bounded sink. Installation failure stops startup. See
[string ABI/capacity evidence](LOCALIZED_SUBSTITUTION_20261029.md) and
[numeric ABI/capacity evidence](LOCALIZED_NUMERIC_20261029.md).

The parent reran 31,114 string checks and 6,289 numeric checks against each
hash-pinned reference/packed executable; native bytes remain fixture data and
the return sink is mocked. All three DLL profiles rebuilt. Only the private
development candidate was staged in the two task-owned copies: SHA256
`e0813d06ccd37ed7bee635c06d32dc20c783dcc37628bd60e428601a3e431022`.
Both actual C1s accepted its required startup guards and passed the native
log-flush/UDP ownership probe. Particular menu formatting and subsequent game
acceptance remain separate checks. No installed-client or production update,
push, public exploitability conclusion, or broader parser clearance follows.

## Gate before public P2P or dedicated-server retirement

The local DW audit on November 1 found a separate relay authorization bypass:
the room type scan recognized only one manifold level, although nested native
messages can contain a type-3 StartGame. The local DW worktree now classifies
all nested leaves with its strict decoder and refuses malformed game-room
batches before relay. The same scan guarded social channels and is corrected
there too. The measured nested Ready bit is now decoded at every level so a
later Ready=false cannot leave backend readiness stale. Five focused regressions and the existing room, Quick,
and private-handoff fixtures passed. The stable Host/Guest clients have not
loaded this change. This is a cross-component relay defect, not a demonstrated
C1 code-execution exploit; see the local DW overnight checkpoint.
The ordinary Quick reservation/host-response/delivery gates also used a
one-level scan; the local source now validates all nested leaves and refuses
nested host StartGame responses. Its 17-test fixture suite passed, including
the new handler-level regression. This remains an offline local patch.
The short private relay permit separately allowed an observed generation
mismatch to recover if room state reverted before expiry. The local service
now makes the observed mismatch terminal and clears cached credentials;
20 private-transport fixtures and the 70-test adjacent service/handoff/active
suite passed (one optional skip). This does not revoke a TURN credential
already issued to a participant or prove C1 parser safety.

- Resolve or independently validate C1 gamestate/configstring, snapshot,
  connectionless/localization, BD/party/relay and downloadable-content paths.
- Authenticate and expire host authorizations by session, joiner, endpoint and
  exact build. A signed host ticket is an authorization, not a proof of safety.
- Replace/test the native fixed ECDH scalar in coordination with actual C1;
  it is not a newly demonstrated RCE and cannot safely be changed by guesswork.
- Qualify remote clients, NAT/relay, departure/recovery and rollback. Keep host
  match reports out of authoritative careers until the trust model is settled.
- Later script work must use signed releases, fresh authoritative match-start
  version checks and authenticated encryption, with no loose-file fallback.
  Encryption/hashes cannot make a player-controlled process tamperproof.

Backend/script prototypes may run in isolation after this audit checkpoint, but
must not enable unqualified remote P2P or retire production capacity. Coupled
client/backend/native rollouts require their ordinary release qualification and
the announced maintenance/continuous-empty checks.


## Later private runtime acceptance, October 29

The latest paired private development Kernel33 includes those bounds repairs
and the native log-flush exchanges. SHA256:
`d9aaa130510369ea54ce0bfb9e7b1f28ad7aa4a277e7a7d73db33ac86740a87c`.
Actual local C1 startup guards, ordinary login/room menus, two-client first-person
gameplay through private TURN, natural ten-minute match finish and both clients'
statistics/room returns have now been observed. Round26 made57 authenticated
relay replacements while preserving its native source socket. These normal
flows do not demonstrate execution of every formatting branch, prove safety of
remaining parsers, or establish public exploitability/non-exploitability.

Actual round27 refused a deliberately damaged local encrypted publisher before
loading the native host DLL; validR3 restoration allowed the same Host process
to install verified sources and reach native readiness. Guest room join timed
out during that negative and is preserved as a failure. Fixture rejection and
this actual prehost refusal do not make a modified player's memory trustworthy.

Receipts and exact logs remain in the private runtime evidence directory:
`E:\CoDOL-Task-Clients\local-services-5a61b7d5\evidence`.
The public/new-queue/native-parser release gates above remain open. No P2P push,
publication, installed-client change, production deployment or retirement occurred.

## October 31 native session-pool and reconnect follow-up

The local native audit found two network-triggered session-disruption paths,
not RCE. A full 64-peer pool previously let an unauthenticated INIT reclaim an
idle established association. A staged reconnect previously replaced the old
player at COOKIE_ECHO even though possession of the lobby key is only proved
by encrypted DATA. The local source now refuses full-pool INITs and promotes
a staged association only after its DATA authenticates; it persists the new
send-counter reservation on promotion. The focused 32-bit Linux peer-pool,
handshake-guard and counter-persistence fixtures passed. Exact source and
fixture hashes, mechanism, and capacity tradeoff are in
`re/p2p-native-audit-20261028/docs/P2P_PEER_POOL_EVICTION_AUDIT_20261031.md`.
The 64-slot exhaustion lifecycle and actual C1 reconnect remain release gates.
No stable client/server binary was replaced or launched for these fixes.

The same follow-up found a bounded-line over-read in the legacy RCON
command-tail extraction after an authenticated, unterminated quoted password.
The local source rejects that form; a 32-bit ASan parser fixture and server
source syntax check passed. A valid RCON password is required for the path,
and no additional RCE was demonstrated. See
`re/p2p-native-audit-20261028/docs/P2P_RCON_UNTERMINATED_QUOTE_AUDIT_20261031.md`.

The local admitted-P2P native source also refused a fail-open configuration:
with `codol_admissionRequired` enabled and no encryption key path, packets
previously fell through to the ordinary plaintext UDP parser. They are now
consumed. The focused 32-bit fixture covers both required-admission refusal
and the separate diagnostic mode; this is not evidence that production was
misconfigured or that an RCE was reachable. See
`re/p2p-native-audit-20261028/docs/P2P_ADMISSION_PLAINTEXT_FALLBACK_20261031.md`.

The next exact C1 bulk-reader xref pass found a count-controlled 0x40-byte
record loop at `0x72feca`. A matching input path clears a fixed 0x4fc-byte
region with a 0x3c-byte header, leaving 19 records; the reader had no
pre-write count cap and continued advancing the destination after input
overflow. A second indirect reader at `0x730cf0` has the same unchecked count
and also copies into a fixed 0x4c0-byte postread region. Required exact-site
guards now bound both observed input paths in local client source. Their
32-bit AddressSanitizer private-page fixture passed.
The postreader also has a separate direct caller and uses each record's first
field as an unchecked index into a 19-entry player table. A third exact-site
guard validates the entire count and index list before the copy. The updated
fixture passed and the candidate SHA256 at that stage was
`9221444ccd860c4d97efbad535d66016ec35432720595ef8134ba55a3dec7569`.
An exact-image xref to `0x72fe30` runs under demo keyframe seek: the callers
at `0x72e6c0` and `0x72e700` of its parent routine use "DemoForward" and
"DemoBack" errors, and the parser itself reports bad keyframe information
while reading scores. The indirect reader and separate postreader still need
complete input-provenance analysis. This bulk-record fix is not evidence of a
live host-driven crash or RCE.
Stable Host/Guest DLLs remain unchanged.
See `docs/P2P_C1_BULK_COUNT_20261031.md` for the exact bound and receipts.

## October 31 admitted-send plaintext fallback

The native `CODOL_NetworkSend` return value of `-1` tells `NET_SendPacket` to
send the original buffer through plaintext UDP. Before the local repair, a
missing key configuration or established peer returned `-1` even when lobby
admission was required. The source now drops non-OOB gameplay in that state
while preserving explicit four-byte-marker connectionless replies and the
admission-disabled diagnostic mode. The focused fixture failed its new
missing-peer assertion before the repair, then passed under 32-bit ASan;
the Windows-target native object compiled. This is a source-level plaintext
fallback defect, not a demonstrated live disclosure or RCE. Exact receipts
and remaining gates are in the native checkout's
`docs/P2P_ADMISSION_PLAINTEXT_SEND_FALLBACK_20261031.md`.

## October 31 exact C1 connectionless print contour

The exact-image `print` branch at `0x496d47` reads a bounded message string,
requests a `0x100`-byte terminated copy, and formats with an `0x800`-byte cap.
The reader stores at most `0x2000` bytes and terminates the last output byte.
This narrows that buffer-write contour; the destination field's allocated size,
other connectionless handlers and full dispatcher safety remain unproven.
See `docs/P2P_C1_CONNECTIONLESS_PRINT_20261031.md` for instruction receipts.

## October 31 exact C1 statusResponse append contour

The exact-image dispatcher routes `statusResponse` to `0x49bf80`. Its
first-line reader and stack field have `0x400`-byte caps; the selected
server-record text appends use a `0x2000`-byte capacity, and the formatter
forces a terminator. The apparent table stride is `0x202c`. This narrows
the observed append writes but does not qualify transaction matching or
other downstream processing. The native fixed ECDH scalar remains an open
confidentiality gate: a later-exposed seat key can reconstruct that seat's
past traffic, while access to other seats' keys is unproven. See
`docs/P2P_C1_STATUS_RESPONSE_20261031.md`. No process was launched or
binary replaced.

## October 31 exact C1 infoResponse contour

The dispatcher routes `infoResponse` to `0x49ad40`. Visible browser-table
searches stop at 128 records, and visible text fields have explicit copy
lengths and terminators. Its first-line packet reader enters protected
`.tvm0` code at `0x8460f7f`; the call site supplies `0x400`, but the
reader's actual bounds have not been validated. No RCE was demonstrated
or binary patch made for this contour. See
`docs/P2P_C1_INFO_RESPONSE_20261031.md`.

## October 31 local ICE offer-shape repair

The isolated signaling authority read both offer identities before validating
offer shape, allowing a malformed future caller input to raise an uncaught
`TypeError` or `KeyError`. The local prototype now checks both dictionaries
and exact field sets before identity access. The malformed-offer regression,
existing signed-grant case and encrypted loopback ICE case passed (three
focused tests). This is an internal robustness repair; the current authority
receives owned ICE offers, so no remote crash or RCE reachability is claimed.
See the backend prototype's `docs/P2P_SIGNALING_OFFER_SHAPE_20261031.md`.
No Windows game EXE was launched or P2P code pushed.

## October 31 exact C1 connectResponse address check

In connection state 4, the exact-image `connectResponse` branch compares
the pending remote with the packet source using `0x5a3580`, which compares
base-address bytes but omits `+0x14` for address type 4 and the `+0x08`
word for address type 5. The browser-response comparator `0x5a3700`
checks those respective fields. After accepting
the reply, C1 copies its source address into the channel. This is a
port-flexible reply binding; whether a remote attacker can exploit it in
the current private relay is unproven, and legitimate NAT behavior has not
been measured. See
`docs/P2P_C1_CONNECT_RESPONSE_20261031.md` for instructions and limits.

The local candidate subsequently replaced that exact base-only comparison
call with a wrapper requiring both C1 address comparators to match before
the sender can become the channel endpoint. The shared 32-bit
AddressSanitizer fixture and Windows-target DLL build passed. This has not
been loaded into C1; legitimate direct/NAT port changes remain unqualified.

## October 31 exact C1 disconnect check

The connectionless `disconnect` branch delegates to `0x496300`, which
requires connection state at least 3 and compares the sender against the
active channel with `0x5a3700`, the exact comparator rather than the
base-address one. It also ignores very early disconnects in state 8+.
The reason token's downstream UI/localization processing remains open;
no RCE or defect was demonstrated in the address check. See
`docs/P2P_C1_DISCONNECT_20261031.md`.

## October 31 exact C1 challengeResponse sender review

The state-3 `challengeResponse` branch parses a numeric challenge, advances
to state 4, and copies the packet sender into the pending connection address
without a visible comparison in that branch. The earlier `0x493a10` sender
check protects `hostquit` and `roundsdone`, not this reply. Generic command
dispatchers and receive filters remain to be fully qualified. Current
enforcing `hostgate.c` blocks unvouched public IPs, while allowing configured
IPs and loopback at any port. This is a potential connection-integrity gate
for direct P2P, not a demonstrated remote RCE or exploit against the private
relay. The exact state-3 request path was subsequently traced to the same
pending address field. A local exact-site hook now requires both C1 base and
extra address comparisons before accepting the reply. Its 32-bit
AddressSanitizer fixture and full DLL candidate build passed. The current
candidate with both challenge and connect sender guards has SHA256
`021a8479cddaff430f61a2188f74918d1c7e9d5e29ea7c7864ca04851eb70877`.
This is not actual C1 or direct/NAT acceptance, and endpoint comparison is
not cryptographic sender authentication. See
`docs/P2P_C1_CHALLENGE_RESPONSE_20261031.md`. No stable binary or client
process was changed.

## October 31 in-process host zone-log repair

The local in-process host advanced a 1024-byte zone-log buffer position by
the unchecked Windows `_snprintf` return. A truncation can return `-1` and
leave the final string unterminated. The source now stops on truncation and
terminates the buffer. The full local DLL candidate rebuilt with SHA256
`bf5d4ecb6dfae095b4a23433dc7ce8c4be10ce98f9c0b7f5f68fda2a249cdc6f`.
Zone-name provenance, actual C1 behavior and remote exploitability remain
unqualified; this is not a demonstrated RCE. See
`docs/P2P_C1_ZONE_LOG_TRUNCATION_20261031.md`. Stable Host/Guest DLLs were
not replaced.

## October 31 exact C1 joinParty count overflow

The exact C1 command table pairs `joinParty` with handler `0x4b1520`. After
address and party-state checks, it reads token 7 as a signed loop count.
The original branch at `0x4b17cd` rejects counts below 1 but allows any
positive count. At `0x4b1800`, each iteration writes a dword to
`[esp+0x30+index*4]` before downstream validation. The next observed local
starts at `[esp+0x84]`; a large count corrupts adjacent stack data. A
later party scan has 19 entries. The local exact-byte-checked hook bounds
the subparty and player counts to 1..19. A second hook bounds existing plus
incoming party records and later filler records to 19 before the builder's
0x5c-stride stack writes.
Both 32-bit ASan fixtures pass at `-O0` and `-O2`, and the
Windows-target DLL candidate builds. See
`docs/P2P_C1_JOIN_PARTY_COUNT_20261031.md` for exact receipts and limits.
The stable Host/Guest DLLs were not replaced or loaded. Current private
relay reachability to this C1 handler and a working RCE are unproven.

## October 31 exact C1 adjacent memberJoin contour

The same digit-prefixed party command table maps `memberJoin` to
`0x4b1d00`. Its visible path has bounded name-field copies and a
19-entry duplicate scan, but two packet-line reads enter protected
`.tvm0` code at `0x8460f7f`. An earlier slot search uses the sum of two
configuration fields as its bound against the 19-entry party array. The
local exact-byte-checked guard now rejects negative or greater-than-19
combined capacity before the sole call into that search. Both 32-bit ASan
fixture variants passed and that-stage Windows-target DLL candidate built (SHA256
`87cc1a136cfcfa9e9687461460292a76263efb36f8acf925b319c8a4cdcec581`).
The first party configuration's fields are written from server-info/session
state, but their upstream bounds, the second configuration, and other
capacity-driven party loops remain unverified. The protected reader, actual
C1 behavior and private relay reachability also remain unproven. This is a conditional bounds
hazard and local hardening, not a demonstrated remote exploit or RCE. See
`docs/P2P_C1_MEMBER_JOIN_20261031.md`; this remains an open parser gate.

The first party configuration also has a server-info setter fed by two
`atoi` conversions without a visible 19-slot cap. An exact-byte-checked
local guard now rejects invalid total/public pairs at the setter before
they can feed the party capacity fields. Both 32-bit ASan and UBSan
fixture variants pass, and that-stage local DLL candidate builds (SHA256
`dad79ef108c74c5194ad1ee18164285110e10f50f2142ee7ff1f21749dba78df`).
See `docs/P2P_C1_SERVER_INFO_CAPACITY_20261031.md`. No actual C1 packet
has exercised it; protected lookup, hostile-host control and second-party
configuration remain open. The separate session-update path follows.

The separate `CL_Live_SessionUpdate` route also had no visible 19-slot
bound before writing the first party configuration. A local entry guard
now skips the update for an invalid public/total pair, preserving the
last accepted values. The corrected 32-bit ASan/UBSan fixtures pass at
`-O0` and `-O2`; the first run caught and eliminated a hook register
preservation error. That-stage PE32 DLL candidate was SHA256
`dffe63854a40b6ef669a1e84824f0c770478d7cba7ef2772682bed5c69511a26`.
See `docs/P2P_C1_SESSION_CAPACITY_20261031.md`. This is compile and
fixture evidence only; actual C1, second-party capacity and hostile
host reachability are still open.

The adjacent party refresh path loops over a 19-record array with a
total-slot dvar count that can be configured above 19. Its local
exact-byte-checked branch guard now preserves ordinary counts and
limits iteration to 19. Shared session creation and refresh setters
also accepted slot pairs whose fields or sum exceeded 19; local guards
now store zero for invalid pairs. The 32-bit ASan/UBSan hook fixtures
pass at `-O0` and `-O2`, and the current PE32 DLL candidate built with
SHA256 `c62377a3c9adff3661a4522c64576e7abc52405ad7e72288deac8157cd155aef`.
See `docs/P2P_C1_PARTY_SLOT_LOOP_20261031.md` and
`docs/P2P_C1_PARTY_SESSION_CAPACITY_20261031.md`. No actual C1 packet,
party session or private-relay path has exercised this build. Other
setters, protected code and remote reachability remain open.

## October 31 private relay RCON rate-limit bypass

The private relay sends a registered guest's raw datagrams from its
per-guest loopback UDP socket to the embedded native server. The native
OOB dispatcher accepts `rcon` there. Password verification remains
mandatory, but the previous failed-password lockout exempted loopback,
so a guest could guess without that lockout. The isolated native source
now counts loopback failures keyed by source port, with other addresses
still keyed by IP; an exact-key 256-source table avoids the old direct
hash-collision eviction. The 32-bit ASan guard fixtures passed at `-O0`
and `-O2`; the Windows-target `sv_main.c` object compiled. See the native
audit checkout's `docs/P2P_RELAY_RCON_LOCKOUT_20261031.md`. No native
server was linked/launched, and no actual guest RCON or RCE was shown.
The isolated backend prototype now drops Guest RCON before native UDP send
in both its short and renewable bridge paths, using the native first-token
rules. Its loopback UDP fixture and 22 renewable-path model tests passed;
see `re/p2p-backend-prototype-20261028/docs/P2P_GUEST_RCON_RELAY_FILTER_20261031.md`.
No actual C1 Guest packet exercised the filter, and alternate routes and
other connectionless handlers remain open release gates.

## October 31 loopback qport cross-slot demux

The embedded server authenticated an outer Guest association but selected
the inner sequenced client's slot by loopback base IP and 16-bit qport. It
also changed that slot's reply port before validating the inner sequence.
The local native audit checkout now requires the exact stable loopback UDP
source port before `SV_ReadPackets` selects a slot. A 32-bit ASan helper
fixture passed at `-O0` and `-O2`, and the Windows-target `sv_client.o`
compiled. See `re/p2p-native-audit-20261028/docs/P2P_LOOPBACK_QPORT_DEMUX_20261031.md`.
Nonloopback players sharing an IP still need a cryptographic association-to-
slot binding audit; no actual C1 cross-slot packet or gameplay test occurred.

The same native client-command review found that `teamXYZ` inherited the
`team` flood exemption even though dispatch uses an exact verb. The local
source now checks the native token delimiter before exempting it; the
Windows-target object compiled. See
`re/p2p-native-audit-20261028/docs/P2P_CLIENT_TEAM_FLOOD_20261031.md`.
No hostile C1 command stream has exercised this change.

The native source review then traced outer bdDTLS peer selection to a full
IP:port, while the inner sequenced demux still used base IP plus qport for
admitted nonlocal players. A same-public-IP player with a separate admitted
association could therefore select another slot if its qport was known or
guessed. The local native audit checkout now requires the connected UDP port
for every admission-required client; diagnostic nonlocal mode retains its
historical behavior. The 32-bit ASan/UBSan endpoint fixture passed at `-O0`
and `-O2`, and Windows-target `sv_client.o` compiled. See
`re/p2p-native-audit-20261028/docs/P2P_ADMITTED_ENDPOINT_DEMUX_20261031.md`.
This is a source-level cross-slot disruption finding, not a demonstrated RCE.
Actual C1 reconnect after NAT port change, same-public-IP players, and
host/guest gameplay are untested release gates. No full native server was
linked or launched.

The follow-on connect audit found that party members may receive the host's
same room key. For a host and guest sharing a public IP, that key and IP cannot
prove the claimed UID; a guest can claim the host UID directly before the host
connects. The former same-UID reconnect path accepted another
UDP source port when qport matched and could reuse an occupied victim slot.
The isolated native policy now requires the existing port and qport for any
verified same-UID reconnect; the policy helper passed 32-bit ASan/UBSan tests
at `-O0` and `-O2`, and `sv_client.o` compiled. See
`re/p2p-native-audit-20261028/docs/P2P_SHARED_ROOM_KEY_IDENTITY_20261031.md`.
First-connect impersonation with a shared host key and same source IP remains
unresolved: it needs a per-member credential or trusted relay source binding
before any release or progress authority is safe. No hostile C1 proof or RCE
claim follows from this source finding.

The adjacent `CODOL_AdmissionMember` fallback was also still accepting an
ordinary roster row for any claimed UID at the same source IP, even when the
authenticated key belonged to a different host and no explicit member row
linked them. The isolated native source now requires an explicit host/member
row; own-key clients identify directly and do not use this fallback. Both
32-bit ASan/UBSan admission fixtures passed sequentially, including an
unrelated same-IP UID rejection, and Windows-target `codol_admission.o`
compiled. See `re/p2p-native-audit-20261028/docs/P2P_SAME_IP_ROSTER_FALLBACK_20261031.md`.
The shared-host-key first-connect gate still stands. No C1 or release proof.

The native sequenced dispatcher also now rejects fewer than six bytes before
reading qport or looking up a client. Previously a missing short read could
become qport `0xffff` and mutate a selected slot before the later netchan
length check. The Windows-target `sv_main.o` compiled; no C1 hostile-packet
test occurred. See
`re/p2p-native-audit-20261028/docs/P2P_SHORT_SEQUENCE_DEMUX_20261031.md`.

## October 31 local forced-relay first-connect identity prototype

The hosted-room search result reserializes the host descriptor, including
its 16-byte key, before Ready. Removing the key only from StartGame would not
solve the first-connect identity gap. The isolated local Host/Guest trial now
has a source prototype: its signed Guest UID is bound to the forced relay's
stable loopback source port before any Guest packet is forwarded, and native
admission rejects another UID claimed from that port. The relay model also
marks a timed-out native receive pump as incomplete cleanup. The native
ASan/UBSan admission tests pass at O0/O2, Windows-target admission and connect
objects compile, all 24 WSL relay model tests pass, and the PE32 client DLL
candidate builds. Exact source and artifact hashes are in
`re/p2p-native-audit-20261028/docs/P2P_LOCAL_RELAY_FIRST_CONNECT_20261031.md`.
No new code was installed or exercised by C1. A same-machine process could
bypass the loopback relay; direct public/NAT play has no per-player credential.
The first-connect identity and protected parser release gates remain open,
and no RCE has been demonstrated. No push or deployment.

## October 31 public-disclosure cross-check: vote and stats

[CVE-2006-5058](https://www.cve.org/CVERecord?id=CVE-2006-5058)
describes long `callvote map` input causing RCE in Call of Duty 1,
United Offensive and Call of Duty 2. Those affected versions are not this
CoDOL C1/sh1x build. This server does have a `callvote map` path, so the
pattern was checked directly: `src/g_sv_cmds.c::Cmd_CallVote_f` copies all
three arguments into bounded arrays, rejects command separators with
`CODOL_VoteArgIsSafe`, restricts map/gametype names to 1-63 plain bytes with
`CODOL_VoteNameIsSafe`, and uses bounded `Com_sprintf` for `level.voteString`.
The existing `tests/test_codol_vote_guard.c` covers long names and separators.
This source comparison does not transfer the older CVE to CoDOL or prove the
protected C1 parser safe.

[CVE-2008-2106](https://www.cve.org/CVERecord?id=CVE-2008-2106)
describes a crash from a type-7 stats packet in CoD4 1.5 and earlier. The
retained `SV_ReceiveStats_f` path is under the CoD4x reliable-message reader,
but the CoDOL server build excludes both its snapshot-frame caller
(`src/sv_snapshot.c`) and reliable-message setup on connect
(`src/sv_client.c`) with `#ifndef CODOL_SERVER`. The CoDOL P2P transport does
not use that reader in this source path. No exact-C1 type-7 reproduction or
claim of RCE follows from this comparison.

## Exact C1 first-connect sender and connectionless `print` follow-up

Read-only disassembly of `E:\codol-classic\C1.exe` SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`
finds `connect %s \"%s\"` at `0x00c47cf8`. Its sender at `0x0049600b`
uses a 16-hex-digit local UID string prepared by `0x00494d50` from the
`0x00722760` identity getter; it separately adds the `xuid` userinfo field at
`0x00495d83`. The server's structural equality check does not authenticate
that sender's account. The October 29 two-C1 loopback receipt also shows both
clients used the same retrieved DTLS room key. A per-player credential remains
necessary for direct P2P; the local relay source binding only covers its
forced-relay path. See the native first-connect document for the exact scope.

For the native connectionless `print` branch at `0x00496d53`, the handler
reads a string through `0x0059f640`, which caps writes to its 0x2000-byte
global buffer and terminates its last byte at `0x0059f6b3`. The branch then
copies into a 0x100-byte destination via `0x006283a0` and formats it with
the literal `%s` into a 0x800-byte stack buffer at `0x00496d8d` before
printing. This shows explicit copy and format length caps in this **one**
branch, but the capacity of the client object's `+0x38` destination was not
independently derived. The dedicated `P2P_C1_CONNECTIONLESS_PRINT_20261031.md`
has the wider receipt. This review does not validate all `msg_t` origins,
other connectionless verbs, protected gamestate parsing, or BD party readers.
No candidate client DLL was installed or run for this follow-up.

The local native relay binding reader also had a fail-closed ordering defect:
it skipped port/expiry/UID validation when the packet came from another IP.
An added test failed on the old source and passes after validating the complete
row before endpoint comparison. Both 32-bit sanitizer fixtures pass and the
Windows-target admission object compiles. Exact hashes and limits are in the
native first-connect document. This does not resolve direct P2P identity or
demonstrate an RCE.

The initial native `connect` validator also used a case-sensitive check for
the `xuid` info key, while general `Info_ValueForKey` lookups ignore case.
A second, differently cased field could pass validation and later be read as
the first `xuid`. A focused 32-bit sanitizer regression failed before the
local parser repair and passes at O0/O2 afterward; a Windows-target object
compiled. See the native checkout's
`docs/P2P_CONNECT_XUID_CASE_ALIAS_20261101.md` for exact hashes and scope.
This removes parser ambiguity, not the shared-key first-connect identity gap.

## Exact C1 DemonWare type-13 nested reader follow-up

The exact C1 image SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`
dispatches chunk type 13 from `0x00b187d7`. Its vtable at `0x00c23de4`
selects `0x00b17d70` for deserialization. That method first calls
`0x00b18cb0`, which requires four bytes available before reading its type
byte, then uses the bounded reader
`0x00a2f260` for a one-byte flag and, when present, a two-byte length. Its
inner object reader at `0x00b17790` requires more than 20 bytes remaining
before it reads a 20-byte digest, compares that digest at `0x00a2bb70`, and
uses `0x00a2f260` for the subsequent 32-bit fields. The helper checks each
read against the supplied logical limit. The source-only local top-level guard
requires that logical chunk area to fit the actual datagram before this
dispatch. On this traced path, no additional out-of-datagram copy or
destination overflow was found.

This is a static trace of one nested reader, not a proof that the digest
authenticates a player or that all type-13 state changes are safe. The digest
implementation at `0x00b1a000` and later packet consumers still require
behavioral review. No malformed type-13 packet was sent to C1, and the
candidate guard has not been loaded into C1. Protected gamestate/party
parsers and direct/NAT first-connect identity remain release gates.

The subsequent exact-image trace follows the type-13 digest into its
count-only serializer and hash dispatcher. The serializer accounts for five
four-byte fields after the 20-byte digest, so the visible `size - 20` hash
length does not underflow on the inspected path. The dispatcher checks its
algorithm index before indexing its table. The 20-byte global key material is
initialized through protected code, so its secrecy and association binding
remain unknown. The outer reader rejects a declared-length mismatch after
the inner read, and the inspected top-level loop exits on a failed chunk
before parsing another; it also caps the loop at 100 chunks. The failed
type-13 object is destroyed. See
`docs/P2P_C1_TYPE13_DIGEST_TRACE_20261101.md` for exact branch addresses.
This is static narrowing only; no new C1 patch or gameplay test resulted.

## Voice packet and codec disclosure follow-up

The native encrypted DATA receive path passes a successful association open to
`MSG_InitReadOnly` and `SV_PacketEvent`. For connectionless `v` packets,
`MSG_ReadStringLine` consumes bytes through `MSG_ReadByte`, which stops at the
message size, before `CODOL_VoicePacket` uses `msg->readcount`. The relay
accepts only a connected active client at that exact endpoint, requires a
five-byte voice header, and bounds its list to six frames of at most 255 bytes.
Its largest accepted list is `5 + 6*(1+255) = 1541` bytes; the outbound
length is 1544 bytes in a 1552-byte stack array. The client `VoiceOnPacket`
requires the current server address, checks the physical message size, permits
at most eight frames, and checks each one before `VoiceInsert` copies at most
255 bytes into a 255-byte slot. `VoicePull` calls `opus_decode_float` with
`frame_size=960`, mono output, and a 960-float temporary buffer. These source
checks did not reveal a caller-buffer overflow on this path.

The local client candidate SHA256
`be97375e08a906428f89283348c53de2b477f02e9e7b5ce6e9f2c3fd7ccb415b`
contains the static `libopus 1.5.2` version string. Its pinned build input
`libopus.a` is 604070 bytes, SHA256
`b7e4f2fdfff5652061b7b6f9f8224ad552a123c1cf227136a95cb118166e2825`.
The published [FreeRDP Opus overflow](https://github.com/FreeRDP/FreeRDP/security/advisories/GHSA-72j9-356v-88xq),
[PJSIP Opus overflow](https://github.com/pjsip/pjproject/security/advisories/GHSA-j59p-4xrr-fp8g),
and [GStreamer channel-count overflow](https://gstreamer.freedesktop.org/security/sa-2026-0053.html)
are bugs in those callers, not evidence of an equivalent CoDOL defect.
[Xiph's large `frame_size` encoder report](https://github.com/xiph/opus/issues/491)
does not match this client's fixed 960-sample encoder call. This is a
reachability check, not a security certification of the bundled Opus library.
No malformed voice packet was sent to C1 and the candidate has not been loaded
into C1; no patch was made on this path.

The authenticated native client-command path had a separate work-amplification
defect: its three-bit opcode loop had no per-packet operation cap, built-in
handlers ignored the legacy flood decision, and exempt gameplay verbs could
reach VM notifications without a rate ceiling. The isolated native source now
caps packets at 256 opcodes, applies the flood decision to built-ins, and
limits exempt non-loopback commands to 128 per second. Focused 32-bit sanitizer
budget tests and adjacent command/pure fixtures passed; the Windows-target
server object compiled. See the native
`docs/P2P_CLIENT_COMMAND_WORK_BUDGET_20261031.md` for exact hashes and limits.
This bounds an admitted player's denial-of-service path; it is not a
demonstrated RCE or live C1 qualification. No stable executable was changed.

The next authenticated packet review found a reliable-ACK integrity defect:
the native server validated a peer supplied ACK with signed subtraction, then
replaced invalid ACKs with its newest server sequence, discarding pending
reliable delivery. The isolated CoDOL source now validates unsigned sequence
distance before changing client state and limits bad-ACK diagnostics. Its movement bitstream passed a focused
200,000-input 32-bit sanitizer corpus without a discovered out-of-bounds write.
See native `docs/P2P_USERMOVE_ACK_AUDIT_20261031.md` for exact object and fixture
hashes. No RCE was demonstrated; valid 255-command packet work and real C1
hostile-packet behavior still need qualification.

## Additional published Mbed TLS disclosure reachability

The native source bundles Mbed TLS `2.16.6`. The maintainer's
[CVE-2026-50580 advisory](https://mbed-tls.readthedocs.io/en/latest/security-advisories/mbedtls-security-advisory-2026-07-tls12-ecdhe-psk-client-buffer-overflow/)
includes this version, but requires a TLS client configured with an oversized
PSK identity and an ECDHE-PSK negotiation. The only non-vendor SSL client call
site found in the isolated native checkout is `src/httpftp.c`; it configures
CA and hostname verification but has no `mbedtls_ssl_conf_psk` call. The P2P
handshake uses the separate `codol_ecdh.c` path rather than Mbed TLS SSL. This
specific disclosed overflow is therefore not mapped to a reachable CoDOL
P2P path by the current source. The maintainer's
[CVE-2026-34877 advisory](https://github.com/Mbed-TLS/mbedtls-docs/blob/main/security-advisories/mbedtls-security-advisory-2026-03-serialized-data.md)
starts at Mbed TLS 2.19.0 and concerns loading tampered serialized TLS state;
this bundled 2.16.6 predates that range, and no non-vendor session/context
load call was found. This is advisory-to-call-site triage, not proof that the
old bundled TLS library is generally free of vulnerabilities.

An admitted native peer could also force repeated lobby roster/key-file reads
by sending DATA records that fail to open. The isolated native source now
limits that failed-DATA fallback to one lookup per association per second; a
focused 32-bit sanitizer fixture covers a malformed burst, alternating valid
records, and later successful key rotation. See native
`docs/P2P_FAILED_DATA_KEY_LOOKUP_BUDGET_20261101.md`. This is a host-work
denial-of-service repair, not a demonstrated RCE or live C1 qualification.


November 1 static security continuation: the exact C1 joinParty count, builder, and filler guard branch targets were rechecked against the pinned image. Fresh 32-bit ASan/UBSan fixtures passed at O0 and O2. The native P2P handshake had an additional confirmed availability flaw: a COOKIE_ECHO could reserve an established peer without authenticating the room key, escape pending limits, and persist indefinitely. The isolated native source now requires authenticated DATA for long-lived peer status, gameplay sends, admission, and persistence; unverified peers stay under the pending limits and expire ten seconds after first INIT. Focused 32-bit sanitizer tests at O1/O2 and a Windows-target object compile passed. See native docs/P2P_UNVERIFIED_ASSOCIATION_POOL_20261101.md for exact hashes and limits. No actual C1 hostile-packet run or stable executable change occurred. No push or deployment.

The November 1 public-disclosure cross-check also reviewed [CVE-2018-20817](https://nvd.nist.gov/vuln/detail/CVE-2018-20817), which names SV_SteamAuthClient/authBlob in several other Call of Duty titles. A source search of the isolated native and client P2P checkouts found no SV_SteamAuthClient or authBlob handler. SteamID/account helpers in the native base are not evidence of that missing authBlob copy. This advisory does not establish a CoDOL P2P match; uninspected protected C1 code remains an independent gate.


November 1 correction: the first-connect claim is also cross-IP. The native candidate-key and key-identification paths fall back to keys from other source addresses, and SV_DirectConnect did not compare the resulting seat IP with the connecting endpoint. The isolated native source now requires the final seat IP to match the actual source, except for an exact validated local relay UID binding. Explicit member rows retain their own source check. Focused 32-bit sanitizer admission tests and Windows-target admission/connect objects pass; see native docs/P2P_CROSS_IP_FIRST_CONNECT_20261101.md for hashes. This is a temporary direct/NAT restriction: NAT-pool address changes can fail, same-IP shared-key impersonation remains, and a per-player credential is still a release gate. No actual C1/public test or push occurred.

The maintainer's [CVE-2026-35336 ECDH buffer-overflow advisory](https://mbed-tls.readthedocs.io/en/latest/security-advisories/mbedtls-security-advisory-2026-07-possible-buffer-overflow-ecdh-calc-secret/) includes bundled Mbed TLS 2.16.6 and can reach RCE when an application calls mbedtls_ecdh_calc_secret with an undersized output buffer. No non-vendor caller of that API was found in the isolated native checkout. Its P2P ECDH path calls mbedtls_ecdh_compute_shared on fixed secp224r1 and writes to a 28-byte output, the curve's required size. This particular disclosed overflow is not mapped to the P2P call path by current source; it is not proof that the full bundled crypto library is safe.

November 1 follow-up: an unverified native COOKIE_ECHO peer could send unlimited invalid DATA records into MAC checks before its ten-second expiry. The local native source now applies the existing per-endpoint/global unauthenticated packet budget to pre-proof DATA; verified gameplay keeps its current path. A 50-record burst fixture, authenticated DATA fast-path fixture, O1/O2 32-bit ASan/UBSan builds and Windows-target object compile passed. Updated exact hashes are in native docs/P2P_UNVERIFIED_ASSOCIATION_POOL_20261101.md. Actual hostile C1 traffic remains untested; no executable, service or production state was changed.

November 1 local route and capacity follow-up: exact C1 disassembly traces connectionless packets into the `joinParty` handler. The isolated short bridge forwarded a harmless OOB `2joinParty` marker from a host test socket to its Guest peer, and the client HostGate accepts loopback; the actual C1 party gates and overflow remain untested. A separate native source regression showed that one room-key-holding Guest could consume every authenticated association slot from different ports on one IPv4 address. The isolated native source now caps new associations at 32 per IP; four 32-bit sanitizer peer-pool/handshake fixture variants passed and a PE object compiled. The cap does not solve multi-IP exhaustion or player identity. See client docs/P2P_C1_JOIN_PARTY_COUNT_20261031.md and native docs/P2P_AUTHENTICATED_PEER_IP_CAP_20261101.md. No new Windows executable, live service, push, or deployment.

The official [IW4x r5243](https://github.com/IW4x/iw4x-client/releases/tag/r5243) commits were checked individually against the isolated CoDOL connect, voice, pipe, path, and format-string routes. No additional equivalent exploitable source call site was established in this pass. The connect `xuid` case-alias repair remains local and tested; same-IP shared-key identity and C1 protected download/zone, gamestate, party and remaining DW parser paths remain open. See native `docs/P2P_IW4X_R5243_CROSSCHECK_20261101.md` for the exact source mapping and links. No actual C1 malicious-packet test was run.

Further relay/native review found that the Guest RCON relay filter could be
bypassed by sealing an OOB `rcon` payload inside valid bdDTLS DATA. The native
receive path opened and dispatched that payload after the relay saw only the
outer encrypted record. The RCON password and failed-password limiter still
applied; no command execution or RCE was demonstrated. Isolated native
`SV_ConnectionlessPacket` now rejects RCON for an authenticated game
association before both handler branches. Separate 32-bit sanitizer fixtures
proved the DATA-to-dispatch boundary and rejection in the actual dispatcher;
a PE/i386 server object compiled. See native
`docs/P2P_ENCRYPTED_RCON_RELAY_BYPASS_20261101.md` for exact hashes and test
limits. The client/server candidate was not launched, pushed, or deployed.

November 1 P2P host policy follow-up: client configuration could disable DW
admission, and an absent `CODOL_VERIFIED_SCRIPTS_REQUIRED` flag let P2P hosting
fall back to loose local GSCs. These are fail-open authorization and script
integrity paths, not a demonstrated RCE. The isolated client now requires
`admission_required=1`, an explicitly selected verified script service, a local
testing build, and an in-process host before startup. Fetch and install also
refuse empty/unselected inventories. The lobby Host and pending-restart paths
now select the in-process host. The full PE32 client DLL linked, SHA256
`cca78352548d7ce210d1ea755f25468b0d73097ba4db377a0cb822b2580fc90d`;
local and shipping policy fixture translation units compiled to PE/i386 objects
but were not executed under Windows. See `docs/P2P_VERIFIED_SCRIPTS_LOCAL.md`.
The candidate has not been staged or run in C1. It remains local, and the
same-IP shared-key first-connect identity and protected C1 parser gates remain
unresolved.

The local `extra` command suffix is now limited to the trial's two lifecycle
switches, preventing that configuration field from overriding admission after
the startup check. This does not authenticate a hostile host machine.
