# Kernel33 room-context migration candidate

`kernel_room_policy.h` ports room-context publication, double-read sampling,
bounded native command-buffer enqueue and same-lock recovery into fixed-buffer
C. `kernel_room_context.c` binds it to the exact supported client functions and
chains the existing main-thread event-loop hooks. No network worker, command
file, log path or per-frame allocation is added.

The migration is **disabled by default**. An opt-in build requires
`K33_KERNEL_ROOM_CONTEXT=1 K33_NO_COPY=1`; the ordinary installation copy is
rejected. At startup the binder also requires the Windows system D3D9 module,
an exact supported executable hash, exact opcode guards and an expected event
loop predecessor. This prevents simultaneous legacy-proxy and Kernel33 room
publishers. Installation failures restore the prior call before entry; an
unrecoverable rollback aborts startup rather than entering with a partial hook.

Native calls use a per-call, thread-local x86 SEH registration. Access and
in-page faults restore the wrapper's stack/frame and preserved registers;
unwind/noncontinuable/unrelated exceptions continue searching. This replaces
the retained C++ reader's `__try` boundaries without installing a process-wide
exception hook. Actual Windows dispatch acceptance remains required.

Evidence so far:

- Differential comparison with the retained C++ publisher/sample/queue policies:
  32,768 transitions, 98,592 assertions pass. Lost acknowledgments, queue
  backpressure, recovery, changed rooms, read failures, capacity boundaries,
  tick wrap and serial exhaustion are covered. A private mutation removing
  backpressure fails that proof.
- Compiled x86 call-boundary fixture: all five test methods pass, including
  actual Unicorn memory faults with altered native stacks under every supported
  calling convention. Windows exception dispatch/CONTEXT application is an
  explicit boundary double.
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

Complete proxy migration remains unfinished. Name-display UTF-8/alias hooks,
borderless persistence parity, retired replay/console removal, system D3D
loading, Windows native fault/lock behavior and actual client room/join/results/
repeat lifecycle must pass together before cutover. Do not publish the opt-in
DLL or delete a working proxy based on these offline results.

Offline source tests require explicitly supplied retained legacy headers and
owned private image. No private binary is added to the repository. Build the
fixture DLLs without an entry point, then execute the x86 Python fixtures in
Unicorn. Do not start a new Windows fixture executable.
