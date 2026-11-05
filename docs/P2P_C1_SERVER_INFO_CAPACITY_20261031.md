# Exact C1 server-info party-capacity guard, October 31

Exact image: `E:\codol-classic\C1.exe`, SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.

The connectionless party dispatcher receives first-party state
`0x6d7cde8` and second-party state `0x6d7f4d8`. Initialization at
`0x4aad01` points the first state to configuration `0x8106488`; its
public/private capacity fields at offsets `0x50`/`0x54` are therefore
`0x81064d8`/`0x81064dc`. Several routines use their sum as a loop
bound against 19 party records of stride `0x108`.

In the `CG_ParseServerInfo()` path, C1 looks up text keys `2485` and
`2491`, converts both with `atoi` at `0x465f64` and `0x465f94`, and
stores the results to server-info state `[ebx+0x168]` and
`[ebx+0x16c]`. The visible path has no 19-slot cap. If its session
gate passes, `0x466008..0x46600d` writes the second value as public
slots and the first minus second as private slots to the first party
configuration. The exact byte sequence at the second state write
`0x465fa2` is `89 83 6c 01 00 00`.

The local `server_info_capacity_guard.c` replaces that six-byte MOV
with an exact-byte-checked CALL and NOP. It keeps the parsed total
only if it is 0..19 and the parsed public value is 0..total. Otherwise
it clears both server-info slot values before the session update. It
preserves ECX for the following `test ecx`; EDX is dead before its next
use. Startup fails closed if the exact bytes or replacement check fail.

`tests/server_info_capacity_guard_fixture.c` executed the 32-bit
register ABI and call-site patch under AddressSanitizer and UBSan at
`-O0` and `-O2`. Both passed, including 0, 19, 20, negative and
`INT_MAX` boundaries. Fixture SHA256: `-O0`
`8d838fd3a29c29405fe14f1e695b3740f6ddd90b5261aad334f39bd441a59534`,
`-O2` `20e2d752f86d9776d5a4dbf6d674dee19297fd7b1fa5a872399b7739db78b2c5`.
The full local PE32 DLL candidate built with SHA256
`dad79ef108c74c5194ad1ee18164285110e10f50f2142ee7ff1f21749dba78df`.
No stable DLL was replaced or loaded.

The protected server-info lookup, whether a hostile host can choose
these values, second-party configuration, and local session-update
source at `0x49c350` remain separate gates. This guard has not been
exercised in an actual C1 packet, match, or private-relay session.
It does not establish a working exploit or make all party consumers
safe for release.
