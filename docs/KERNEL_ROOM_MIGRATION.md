# Kernel33 room-context migration candidate

`src/kernel/kernel_room_policy.h` ports room-context publication, double-read sampling,
bounded native command-buffer enqueue and same-lock recovery into fixed-buffer
C. `src/kernel/kernel_room_context.c` binds it to the exact supported client functions and
chains the existing main-thread event-loop hooks. No network worker, command
file, log path or per-frame allocation is added.

The migration is **disabled by default**. An opt-in build requires
`K33_KERNEL_ROOM_CONTEXT=1 K33_NO_COPY=1`; the ordinary installation copy is
rejected. At startup the binder also requires the Windows system D3D9 module,
an exact supported executable hash, exact opcode guards and an expected event
loop predecessor. This prevents simultaneous legacy-proxy and Kernel33 room
publishers. Installation failures restore the prior call before entry; an
unrecoverable rollback aborts startup rather than entering with a partial hook.

Native calls use a per-call, thread-local x86 SEH registration. A recognized
continuable access/in-page fault now terminates the client with exit code
`0x4b524301`, with fast-fail7 fallback if self termination fails. It does not
rewrite CONTEXT, return false, log, allocate or guess which native locks to
release. Exact Cbuf_AddText and Dvar_FindVar can acquire locks before faulting;
continuing after abandoning them is unsafe. This repairs the inherited r45
fault policy, not its underlying native fault: the client deliberately crashes
instead of continuing corrupted global state. Ordinary null/false returns
continue normally; unwind/noncontinuable/unrelated exceptions continue search.
Actual Windows dispatcher and fail-fast behavior remain required acceptance.

Evidence so far:

- Differential comparison with the retained C++ publisher/sample/queue policies:
  32,768 transitions, 98,592 assertions pass. Lost acknowledgments, queue
  backpressure, recovery, changed rooms, read failures, capacity boundaries,
  tick wrap and serial exhaustion are covered. A private mutation removing
  backpressure fails that proof.
- Compiled x86 call-boundary fixture: ten test methods pass, including actual
  native Cbuf fault at0x58baf2 after lock acquisition, nested recovery-lock
  depth2, native Dvar_FindVar fault at0x623305, normal successful/null return
  lock balance, all five calling conventions, altered native stacks, in-page
  failure, unrelated exception search and fail-fast fallback. Faults never
  continue normally or issue invented unlocks. Windows dispatch is doubled.
- Compiled x86 binder fixture: all eight methods pass. It executes the exact
  retained Cbuf_AddText instructions, tests every startup guard-byte mutation,
  trusted System32/SysWOW64 paths versus local proxy rejection, predecessor
  validation, install rollback/abort fault stages, acknowledgment/backpressure
  and recursive-lock recovery. Windows APIs, hash input, room predicate and
  dvar lookup are explicit doubles.
- Baseline, migration-off and migration-on complete DLLs build. Normalize PE
  relocations to the same preferred base before comparison: migration-off
  `.text`, `.rdata`, `.data` and `.rsrc` are byte-identical to the prior source.
  Migration-on adds 9,216 bytes in the observed build and adds no non-system
  DLL dependency. Neither candidate was installed or launched.

The shared baseline pins the reviewed source with candidate qualification state
preserved. Its mutation checks include the migration binder, policy, guards and
profile controls. This pins provenance; it does not authorize migration-on in
a public client release.

The opt-in name-display companion now implements all nine UTF8/alias/HUD hooks
and explicit Windows system-D3D loading; see `KERNEL_NAME_MIGRATION.md`. Full
proxy-free qualification remains unfinished. Borderless persistence parity,
retired replay/console removal, Windows native fault/lock behavior and actual
client room/join/results/repeat lifecycle must pass together before cutover.
Do not publish the opt-in DLL or delete a working proxy from offline results.

Offline source tests require explicitly supplied retained legacy headers and
owned private image. No private binary is added to the repository. Build the
fixture DLLs without an entry point, then execute the x86 Python fixtures in
Unicorn. Do not start a new Windows fixture executable.
