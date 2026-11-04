# Kernel33 name-display cutover prototype

The opt-in `K33_KERNEL_NAME_DISPLAY=1 K33_KERNEL_ROOM_CONTEXT=1 K33_NO_COPY=1`
build owns all nine retained NameDisplay patch sites in Kernel33. Both flags
default to zero. It resolves the Windows system d3d9 explicitly instead of the
local render proxy, installs the name hooks before game entry and before voice
or controller threads, and samples identity on the chained native main tick.
It is an uninstalled qualification candidate, not a production release.

The behavior reference is the public r45 app proxy source at
`0c0872ee6c823205273b52dfd96a481225ac889d`, `launcher/client_overlay/NameDisplay.cpp`.
Exact a5a7 executable SHA-256 and all 16 retained native guard regions must match.
All nine writes are a pre-entry transaction: verify, protect all, write all,
flush, restore protection and read back; failures roll back every original byte.
An irrecoverable rollback aborts this process before game entry.

The implementation uses fixed buffers, a bounded strict UTF8/JSON parser, a
single HTTP worker and lazy per-thread render buffers. Only immutable canonical
identity and the already-populated local UID caches select the display alias.
It preserves clan prefixes, the 63-byte UTF8 limit, 30-second request refresh,
60-second alias expiry and stale-response generation rejection. The retired
Theatre receiver supplied generation zero in r45, so this preserves zero rather
than treating room serials as identity changes. TLS verification, fixed HTTPS
origin, disabled redirects, four 2-second timeouts, 8-second response-loop
deadline and 4-KiB response limit match the production path. Developer mappings
and alternate development origins are absent from this migration.

The GFx constructor path accepts only valid extended UTF8 name fields. Chinese
CP936 conversion is restricted to the original private lobby/obituary buffers
and proven MP_CONNECTED HUD name span. Canonical game models, account identity,
peer UID, chat, localization keys and authentication state are not rewritten.
Required JSON keys appearing twice are now rejected; this intentional stricter
response handling avoids ambiguous identity fields. Unknown metadata nesting is
bounded to 16 levels. A failed/malformed response retains native-name fallback.

Qualification evidence, recorded separately from gameplay:

- Portable compiled C policy: 6 passing methods, 10,000 independent Python
  JSON/Unicode oracle cases, truncation/UTF8/surrogate/TTL/generation controls.
- Compiled x86 fixture interpreted in Unicorn: 9 passing methods. Every byte in
  all 16 exact native guard regions is mutated; every protection stage, flush
  and thread-start rollback is faulted. Legacy thunks exercise actual compiled
  C callbacks, original custom helper ABI, GFx thiscall argument positions,
  continuations, preserved registers/flags, English fallback, Chinese encoding,
  one-request backpressure, worker wake/refresh and stale identity rejection.
  Overlapping patch-page protection restoration is modeled. Legacy video-mode
  fallback reads once, honors an existing native dvar, writes atomically via a
  per-PID temporary file and retries failures at most once per second. Open,
  short-write, write, flush and rename failures preserve the old file; unchanged
  mode performs no file I/O per frame.
- Full baseline/default/room/full-cutover DLL builds pass. After relocation
  normalization the default executable code/data/resources remain byte-identical
  to the baseline. Latest full cutover adds 27,136 bytes and Windows WINHTTP only.
- 32 profile expectations pass, including rejected name-without-room flags and
  rejected installation-copy path. Source/AS campaign: 19/19 passing methods.
- Two private source mutants independently bypassing the row UID check and
  native guard check compile successfully and fail the behavioral suite.

The x86 fixture explicitly doubles Windows APIs, HTTP, Unicode conversion and
GFx; it does not establish actual Windows dispatch, rendering, service/network
acceptance, latency, thread teardown, loader behavior or full game lifecycle.
Do not delete the shipped proxy or enable the migration by default until the
exact combined candidate passes both-language roster/HUD/obituary/lobby labels,
startup/update/rollback, repeated room/game transitions, borderless/window
behavior and controller/voice/Discord interaction on approved stable paths.
Retained proxy borderless behavior must be compared against Kernel33's existing
borderless implementation. No candidate DLL in this work was installed or
loaded by Windows.
