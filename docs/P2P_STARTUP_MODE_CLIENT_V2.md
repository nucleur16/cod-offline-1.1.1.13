# Fixed match options in the private startup v2 client

This is uninstalled source/build work. It adds no production default or runtime
permission. The frozen native contract is `docs/P2P_STARTUP_MODE_V2.md` in the
paired native worktree. Existing v1 request/envelope/ACK bytes and domain remain
unchanged.

`CODOL_P2P_STARTUP_IPC_V1=2` explicitly selects v2 in a local-test profile. Values
0 and 1 retain their previous meaning; unknown values fail closed. Mode 2 never
retries v1 or legacy Init. The UI reads the actual native-published protocol
number and adds typed presence/difficulty fields only for mode 2.

The actual Zombies creator uses
`int(myDifficultySelector.diffData[selectedIndex].id)`. `RoomObj.currentLevel`
is a public uint slot with a default of 1, so its existence does not establish
freshness or authority. Ordinary MP uses this field as a league. The MP intent
therefore carries absent/-1; Zombies carries the actual finite 0..2 or Endless
3. Co-op/Survival can carry their raw selected UI tier while the authenticated
effective native plan has no difficulty. The owned SDK must compare the actual
fresh signed selection and derive the full immutable packet independently.
Training/playlist-origin exclusions are not lifted by this implementation.

Both UI producers include the typed tier in their v2 context. A level-only edit
updates the private room command and invalidates its attempt token. A pending
Ready timer compares the same context before calling actual native Ready;
stale Ready, malformed tiers and a missing value cancel rather than substituting
Easy. No truthiness conversion discards tier 0.

The wire has a 64-byte CPSQv2 header, a 192-byte CPSEv2 header, a fixed 352-byte
CPMOv1 body and a 76-byte CPSAv2 ACK. Difficulty is signed i8 at request offset
56, presence is u8 at57 and58..63 are zero. The binding includes the exact
request, all three component lengths and hashes under the distinct v2 domain.
The client checks fixed strings, padding, scalar bounds and matching
request/manifest/native epoch/counter/generation/sequence. Authentication and
full rule normalization belong to the retained trusted SDK; native validates
and consumes its actual engine policy.

Only the native game-thread fiber calls the scripts, principals and match
options installers, in that order, checking cancellation between calls. The
v2 ACK reports success only for three literal results of 1. The v2 loader must
resolve `Sh1xHost_UseVerifiedMatchOptions`, `Sh1xHost_InitWithMatchOptions` and
`Sh1xHost_ObserveMatchOptions`; it selects the v2 Init directly and has no
legacy fallback. The native Init renderer is the actual fixed-rule producer.

The bridge copies the exact CPMO bytes and actual startup binding before the
worker can free the envelope. No later observation uses a dangling view.
Native options are observed after the first map Frame and before cached Ready
is enabled. Subsequent between-frame/control processing and post-Frame return
also recheck actual rules; mismatches quarantine principals and clear Ready.
Principal lease updates can advance sequence while the original options remain
immutable. Parent loss, expiry, cancellation, one-host and no-rehost gates remain.

Qualification uses independent Python v2 vectors and the unchanged v1 vectors,
actual C codecs under i386 ASan/UBSan, actual extracted client fiber plus native
options/provider/principal APIs, and source/compiled-export UI behavior tests.
The engine, OS, DTLS and fiber boundaries in portable fixtures are explicit
doubles. Approved stable Java plus the installed FFDec data JAR compiles only
the two owned classes; compiler identity and arguments are recorded separately.
These checks do not establish live AVM2, native gameplay, public networking or
Windows DLL-loader acceptance. No source baseline is refreshed here.
