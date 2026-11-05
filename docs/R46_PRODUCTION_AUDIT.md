# r46 native release gate audit

The exact public r45 Kernel33 source is `96d7020d8133d854d4be04dd76eda47ef4474a49`.
Its DLL is SHA-256 `f4b6a547d8bb2dd8242e575929a32588373c36908e0339fddd3a3c91a006413a`.
The source-root DLL is an older artifact and must not be used as release evidence.

The shared UI baseline retained the `quick_start_gate.c` digest from before
`c54fa1b` (the reviewed public playlist autostart repair). The source matches
that commit byte-for-byte. This change advances only that file's baseline pin;
it does not rebuild a movie or alter the native hook. The existing mutation
rejection tests continue to reject changes to each pinned native component.

The offline exact x86 suite now covers every public discriminator value except
the managed-Quick value, every supported cohort size with duplicate member
rejection at each position, 128 repeated round cycles, and expiry immediately
before/at/after 30 seconds across tick wrap. The suite also retains its guard
mutation and installation rollback fault tests. All 28 test methods pass against
the exact supported shipping image in Unicorn. OS, frame body, event delivery
and allocation boundaries remain doubles. These results do not establish
gameplay reliability or real client round-trip acceptance.

The source/AS suite passes 19 methods after the one-pin baseline repair. The
portable Quick, locale and date arithmetic policies pass; a 36-combination
compile matrix accepts only the three valid testing/diagnostic profiles.

The public r45 DLL contains no Kernel33 log path, command-file transport,
synthetic pad command or sound-diagnostic command markers. SDL2, Opus and zlib
are statically linked; their external DLLs must not be added to a client release
because a build used them. The actual reconstructed game imports still require
Bink, Miles and Direct3D. Miles loads its codec/filter plugins dynamically.

The r45 Direct3D proxy still owns name display hooks, native room context and
legacy borderless/replay code. Kernel33 already owns its own borderless mode.
Removing the proxy before migrating and qualifying the remaining behavior would
lose active functions. New patchwork belongs in Kernel33; complete removal is a
separate required cutover gate, not certified by these offline tests.
