# Exact C1 local session capacity, October 31

Exact `E:\codol-classic\C1.exe` SHA256:
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.

`CL_Live_SessionUpdate` at `0x49c350` reads the public-slot dvar pointer
from `0x27f4f00` and total-slot dvar pointer from `0x172010c`. When the
capacity differs from the current party configuration, it calls
`0x4aff20` with total minus public and writes public/private fields at
`0x81064d8`/`0x81064dc` (`0x49c3bc..0x49c3c2`). No visible check in
this routine requires 0 <= public <= total <= 19 before the writes.
Those fields are the first party configuration's capacity bound for
several 19-entry array loops. The routine is installed as a local
session-update callback at `0x49c4e5..0x49c4f5`.

The local `session_capacity_guard.c` replaces the exact six bytes
`8b 0d 00 4f 7f 02` at the no-prolog function entry. It checks both
dvar pointers and requires 0 <= public <= total <= 19 before the
original body. Valid input recreates the overwritten ECX value and
continues at `0x49c356`; invalid input returns to the original caller
without changing the last accepted party configuration. Installation
checks the original bytes and fails client startup if the replacement
cannot be verified.

The first 32-bit sanitizer fixture run failed: compiler-generated
address preparation changed EBX and ESI, which the hook must preserve.
The guard now uses direct absolute memory operands. AddressSanitizer
and UBSan fixtures pass at `-O0` and `-O2`, including valid boundaries,
oversized, negative, `INT_MAX`, and null dvar-pointer cases. Windows
DLL disassembly also shows direct loads from `0x27f4f00` and
`0x172010c`, with no EBX/ESI writes in the guard.

Final fixture SHA256: `-O0`
`5febea66e3e6177829593ed917cc947f836bba3c0e7dc52e05e8aa7ae4d553af`,
`-O2` `820e8c689184308c4230a023efedc7753c2cb4ec3deaa8d54bb535959c8a7efb`.
Guard source SHA256:
`18c26a38c3a99a79067949e639b46ebdd1eadec58bd509f588a9f74973b8bac3`.
The full local PE32 DLL candidate built with SHA256
`dffe63854a40b6ef669a1e84824f0c770478d7cba7ef2772682bed5c69511a26`.
No stable DLL was replaced or loaded.

This guards one local session-update route into the first party
configuration. The second configuration, other writes/consumers,
protected parsing, actual C1 behavior, and private-relay reachability
remain unverified. This is not proof of a remotely reachable exploit,
working RCE, or release readiness.
