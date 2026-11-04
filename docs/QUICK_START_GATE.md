# Native Quick start permission (2026-10-24)

Public playlist scope correction (2026-11-02): numbered public playlists also
use the native `playlist` mode string. That string alone does not identify a
managed Quick cycle. The guard now enters its permission policy only for the
existing native managed-Quick discriminator 11. Other discriminators retain
the native timer and StartGame path without requiring Quick Ready callbacks.
An already observed managed cycle still fails closed if its scope changes.
This does not enable Quick Match or change its catalogue visibility.

The added exact-x86 regression uses the observed public-room discriminator 2,
two members and zero Ready flags. It fails against the previous guard and
passes with the scope correction. The actual two-client dedicated handoff is
qualified separately; this offline result alone is not gameplay acceptance.

The shared EN/ZH frontend waits for genuine Ready callbacks from every member
in the current results generation. The native Quick controller separately
entered state 6 and called StartGame after its own 10-second enter-room timer,
even while a peer still viewed results. Captured kind-2 query txn358 waited
111.041 seconds for its answer; the exact native request expires at 60 seconds.
The AS gate alone therefore could not protect that native request.

This module stores permission only when the genuine `StartMatchCountDown` AS
bridge runs for the current managed Quick host with all current native members
Ready. Ready snapshots alone cannot grant permission. The shared frontend's
current-generation callback gate remains required; this is one component of
that paired release, not a replacement for it.

The identity binds the actual Quick object, room ID, sorted unique UID cohort,
room settings, map, mode, alternate mode, final voting entry and previous entry.
Entry zero is valid. The native map string alone is not the final selection.
An intent in phase 1, 2, 3 or 4 survives normal progression to 5. Real repeated
matches showed the genuine AS call arriving in waiting phase 1, before the
native enter-room delay. This call now retains its exact identity through that
delay; merely observing Ready in phase 1 grants nothing. A phase regression,
identity change, missing readiness or duplicate call cannot renew that intent.
Native consumption remains restricted to phase 5. Exact frame disassembly shows
phase 2 is part of ordinary progression: phase 1 writes 2 at 0x71B127, then
phase 2 writes 3 at 0x71B26E. The stock collection dvar `2580` is 90 seconds;
`dw_enterRoomCountingDown` is 10 seconds. Early phase 1/2 intent therefore has a
finite 120-second lifetime, while phase 3/4/5 intent retains 30 seconds. Neither
duplicate calls nor native progression renews that original lifetime. It is invalidated by an observed
identity/Ready/host change or phase regression, and can be consumed only once.
Duplicate and synchronous reentrant bridge calls cannot renew it. Frame
observation before and after the original native frame rearms subsequent
rounds after phase exit; it writes only private policy state. Native mmbook may
advance the final entry after consumption without creating new permission.

The consumer runs at `71B437`, after the original dw_autoStartGame and timer
checks. Denial continues at the native epilogue `71B457`. Acceptance executes
the original state-6 write and continues at `71B43E`, so the retained native
StartGame, query, booking and connection path remains responsible for starting.
The separate manual partial-Ready frame `710383..71054F`, its 60-second policy,
all native Ready fields and dvars are unchanged.

An observed managed cycle is remembered before any permission exists. A change
of scope cannot turn that owned cycle into unrelated-mode passthrough. Missing
or mismatched controller data invalidates permission; only an actually observed
native phase outside 3–5 proves phase exit and permits another generation.

The bridge is exact native thiscall `6AEFB0`, ret4; its vtable/registration proves
the AS method. It emits type17 through room virtual+2C (`70E8B0`); it does not
itself invoke StartGame. The read-only frame observer is `71AD60`. Final voting
setter `70F080` writes info[2C], exporter `696500` labels it playListNextEntry,
and native mmbook `70FC40` consumes it. Fourteen shared exact regions (2999
bytes) are checked on both shipping a5a7 and retained d821 before any patch.
All three trampoline/protection preparations precede changes; producer and
observer install before consumer. Failure rolls back all entries. A rollback
that cannot regain write access terminates before game entry rather than
executing a partial transaction.

Offline verification compiles the actual production module to PE32 x86 and
executes it in Unicorn together with the exact native AS handler, Quick
state-5 block and native prologue/trampolines. Windows APIs, timer result,
native frame body, event delivery and StartGame endpoint are explicit test
boundaries. The old native block starts with an unready guest and no bridge;
the patched block does not, including a 135-second results hold. Tests cover
early intents, same-room repeats, duplicate/reentrant calls, cancellation,
cohort/Ready/host/map/selection changes, finite expiry, timer checks, each guard
mutation and installation failure stages. This is not real C1 acceptance.

If an already-issued intent expires or is invalidated while the UI has disabled
Leave and marked booking started, this module denies the stale start. The
current frontend has no automatic bridge retry. Recovery in that exceptional
contour is **not** claimed; it requires C1 verification and is a release risk.
Normal first/repeat rounds, guest-first acknowledgment and long results holds
must pass on both languages with actual clients before release.

Build fixture (output must be outside the source tree):

```text
i686-w64-mingw32-gcc -O2 -fno-builtin -fno-stack-protector -shared -nostdlib \
  -Wl,--entry,0 -Wl,--image-base,0x20000000 tests/quick_start_x86_fixture.c \
  -o <fresh-E-output>/quick-start-x86.dll
python tests/test_quick_start_x86.py --fixture <fixture> --image <exact-C1> \
  --output <fresh-E-output>/x86-proof.json
```

Exact image SHA256:

- a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976
- d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60
