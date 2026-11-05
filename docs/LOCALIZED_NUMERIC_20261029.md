# Bounded numeric localization

This continues the separate numeric path left open by the3693 string-helper
candidate. That candidate and its staged files are preserved. This change is
local source/fixtures/rebuild only, pending parent review; no game launch,
staging, installation, commits, pushes, production changes or RCE claim.
The original numeric routine is never executed by the fixtures.

The exact reference is C1.exe SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
The preserved packed executable has SHA256
`a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976`.
The482-byte numeric target5F4BF0..5F4DD1 is identical in both, SHA256
`80f97bfa35c9af4da42e34ee6d41bb770d8b38203d0f72c6aeb3567b4ba626b7`.

## ABI, reachability and caller bounds

5F4BF0 takes ECX=`const int32_t *values`, stack arg1=`const char *format`,
stack arg2=`int count`. It returns a string pointer in EAX and uses plain RET;
its caller removes the two stack arguments. This is not MSVC thiscall (which
would pop them), nor standard fastcall (which would also consume EDX).
All direct caller push/cleanup sequences, count stores and arrays are pinned.

Exact executable-section disassembly and an independent raw relative E8/E9
scan both find the following six direct calls and no direct jumps. No raw
absolute reference to the function address was found. This does not claim to
exclude every possible computed/obfuscated route; the replacement refuses
unreviewed return addresses without calling the original or native TLS code.

| Call / return | Enclosing function | Integer array and count | Ordinary localization key |
| --- | --- | --- | --- |
|4AA1DA /4AA1DF|4AA110|EBP-Ch,2 ints through EBP-5|MENU_MAPVETOSTATUS or MENU_MAPVETOSTATUSPLURAL|
|4AA441 /4AA446|4AA320|EBP-18h,4 ints through EBP-9; cookie at EBP-8|MENU_TESTINGBANDWIDTH|
|4AA49E /4AA4A3|4AA320|EBP-10h,2 ints through EBP-9; cookie at EBP-8|MENU_JOININGLOBBY|
|4B454D /4B4552|4B44D0|two locals at pre-argument ESP+8/+Ch; count2|MENU_LOBBYPLAYERCOUNT|
|5EDE33 /5EDE38|5EDD50|two locals at pre-argument ESP+10h/+14h; count2|EXE_GETTINGINFOFORSERVERS|
|70100F /701014|700E90|two locals at pre-argument ESP+10h/+14h; count2|PLATFORM_INVITETOOSOON|

4AA320's full prefix covers both its callsites. The five enclosing prefixes
total1344 bytes and pin all six array/count constructions. Counts are constant
2 or4 in these ordinary paths. Integer values are signed32-bit and are
converted with the original fixed `%d` constant at C40A74. The original has a
40-byte local argument layout (count+nine pointers); its unchecked count loop
and unchecked output expansion warrant a complete bounded replacement, rather
than returning blank for ordinary UI prompts.

Original output is EBP-408h..EBP-9 inclusive (1024), cookie at EBP-4 with a
four-byte gap. The literal path copies1023 and terminates at EBP-9. Its marker
path clears1024 then copies literal and replacement bytes without remaining-
capacity checks. Formatted integer storage begins EBP-808h and advances64 per
value. The repair validates count0..9 and readable count*4 input, uses twelve-
byte bounded signed decimal buffers (including INT_MIN), and the already
tested shared1024-byte bounded formatter. Marker semantics remain &&1..&&9,
one digit only, including &&10=value1 followed by literal0. Invalid references
produce empty output, with no original fallback.

## Native return lifetime and bounded sink

The original final call is `6286D0("%s", output)`, not a pointer to its stack.
The complete126-byte va routine is identical in both executables, SHA256
`9d65dbd5e178de52b888195bea931cbad0e7d571624be6332166c336b29c8eec`.
Its TLS chain is FS:[2Ch] -> first static-TLS block -> pointer at+Ch -> pointer
at+4. This final state has two1024-byte slots, then a32-bit index at+800h.
The routine selects index*400h, advances index modulo2, passes1024 to its
bounded native vsnprintf wrapper, explicitly terminates slot+3FFh, and returns
the selected pointer. A second call preserves the previous slot; a third may
reuse it. Keeping the original va sink preserves interactions with other
native va callers, including5EDE33 immediately calling5F4B40 afterward.

The entire201-byte native vsnprintf core/wrapper ADA5AD..ADA675 is also pinned.
6286D0 supplies the400h capacity; the wrapper forwards it into the CRT's local
output-stream count before formatting. The replacement passes only the fixed
DLL literal `%s` and its own already-terminated string of at most1023 bytes;
localized text is never used as a printf format. It therefore stays below va's
native truncation/error threshold. The old numeric routine and its native
unbounded integer sprintf are not called.

Before entering va, the worker checks every TLS-chain dereference, full804h
writability and index0 or1. Missing/uninitialized/read-only state returns an
immutable empty string without entering va. Afterward it checks the returned
pointer, modulo2 index advance, and bounded termination. Unknown callers also
return immutable empty text. Invalid input for a known caller with valid TLS
still consumes one empty va slot, preserving the ordinary return lifetime.

## Exact guard and thunk

The entry thunk saves EBP, captures ECX, the two stack args and actual return
address, then forwards them to a stack-realigned cdecl worker. It restores EBP
and returns EAX with plain RET, leaving the caller's eight-byte cleanup intact.
The worker preserves x86 nonvolatile registers. A nine-byte JMP/NOP patch
covers complete original instructions: push EBP, mov EBP,ESP, sub ESP,83Ch.
A six-byte entry patch would split that SUB instruction and is not used.

Guard coverage is482 target+126 va+201 CRT+1344 caller bytes, plus fixed `%d`
and `%s` constants (2159 bytes total). All are identical across the two files
except the proven timer IAT operand in4AA320 at4AA369..36C. The reconstructed
C1 import table labels9A1D78C WINMM!timeGetTime; the packed instruction uses
B424C0, independently named WINMM!timeGetTime in c1_tables.h:302. Both exact
operands are accepted only when the dereferenced live slot equals the genuine
winmm timeGetTime export. This was verified from the import tables; an earlier
GetTickCount inference was corrected before implementation. No arbitrary slot,
GetTickCount slot, or merely shaped indirect call is accepted.

Installation runs before C1 entry, following the string-helper installation.
Any guard/protection/cache/readback failure terminates startup. There is no
numeric-original trampoline or fallback. The string-helper caller guard covers
4B454D's call encoding, which stays unchanged because only the numeric entry
is redirected. All three build profiles include both defensive repairs.

## Fixture boundary

The focused native fixture calls only corrected worker/thunk code, with a
mocked va using the measured two-slot layout. It verifies six caller identities,
decimal limits, ordinary2/4-value formatting, all nine indices, count0 literals,
invalid count/index/missing input, literal percent text,1024-byte truncation,
TLS-ring lifetime, inaccessible/partial integer arrays and input, read-only or
missing TLS, invalid TLS indices and unexpected mocked sink results. Its custom
ABI invocation independently checks ECX/stack arguments, EAX result, actual
return-address capture, caller cleanup, EBX/ESI/EDI/EBP preservation and four
incoming stack alignments. Canaries surround both TLS slots and index.

Each of the2159 guard bytes is individually changed and must refuse install;
wrong/null timer exports and an unrelated IAT operand also refuse. Success
changes only the nine entry bytes; original and sink bytes are data only and
never called. The installer briefly changes the test page's protection and
restores it. Both input executables are hash-pinned by the Python runner.
Full DLL outputs and source/dependency receipts use the new directory
`output_candidate/localized-numeric-20261029/`. Builds/fixtures do not prove
real C1 numeric-menu acceptance, remote exploitability or completion of the
broader audit. Parent review precedes any staging.
