# Bounded localized text replacement

This is a local source/build repair for a confirmed static missing-capacity
check at C1 `0x005F4DE0`. It is not an RCE finding or an exploitability claim.
The original routine has not been executed by these fixtures. No malformed
network messages, game launches, installation, commits or pushes are involved.
The independent numeric formatter at `0x005F4BF0` remains unaddressed.

The reviewed reference is `E:/codol-classic/C1.exe`, SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
The actual packed executable used for the second static comparison has SHA256
`a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976`.
The helper's complete 260 bytes and the six caller prefixes (2917 bytes total)
are identical in both files. The helper body SHA256 is
`1b4e734068d5c9406d21c0cf68d5b88e18846df03b22c871a16e184f22f373f8`.

## Native contract and ordinary syntax

All six direct callers push output, arguments, format in that order and clean
the stack after the call. The return is unused. The replacement is x86 cdecl
`void(const char *format, const Args *args, char *output)`; `Args` is 40 bytes:
an integer count followed by nine string pointers. It explicitly realigns its
stack for the native callers' alignment and preserves the cdecl ABI.

The original searches for `&&` (constant at `0x00C4291C`) and consumes one
decimal digit. Its pointer calculation maps `&&1` to `Args+4`, the first value;
`&&0` incorrectly addresses the count word. Thus the safe contract is indices
1 through count, with count0 through9. `&&10` substitutes value1 and retains
literal `0`; `&&x`, trailing `&&`, and other nonnumeric text remain literal.
The ordinary caller keys include the two PLATFORM_DEMO_DVR freecamera/player
prompts, MENU_PARTYPLAYERCOUNT and MPUI_UNLOCKXP. Their one/two-string argument
construction is visible in the pinned callers; localized asset values were
not inferred from key names. No assertion is made that a particular asset
contains `&&0`, or that it is a valid marker.

For literal input, the original already limits its copy to1023 and terminates
at1023. Once a marker exists, it clears1024 then copies literal and substituted
bytes without checking the remaining output capacity. The replacement uses
the same1024-byte caller contract, reserves one NUL, truncates deterministically
and clears invalid-reference output. It validates all references even beyond a
truncation point. Null args mean count0 and permit literal-only text. Missing
referenced arguments, out-of-range indices/counts, unreadable inputs, or inputs
without a NUL within65536 bytes yield bounded empty output. An unused null
argument does not invalidate literal text. Native pointers are checked with
VirtualQuery across readable/writable committed regions before access.

## Six caller capacity checks

`S` below is ESP immediately after that function's frame allocation, before
saved-register pushes and transient arguments. Full frame-to-call bytes pin
the intervening stack adjustments and argument construction. Range ends are
inclusive. Sizes are proven by the frame layout and the native helper's own
1024-byte clear/copy contract, rather than an inferred arbitrary buffer size.

| Call / return | Enclosing entry, allocation | Output range | Nearby boundary / count |
| --- | --- | --- | --- |
|455336 /45533B|454ED0,644h plus saved EBX/ESI/EDI|S+23Ch..S+63Bh (call-site ESP+248h)|cookie S+640h; count1, local text at call-site ESP+148h|
|458531 /458536|458160,578h plus saved EBX/ESI|S+170h..S+56Fh (call-site ESP+178h)|cookie S+574h; count2, both localization-key branches converge|
|4B4610 /4B4615|4B44D0,478h plus saved EBX/ESI|S+70h..S+46Fh (call-site ESP+78h)|cookie S+474h; count2|
|5EE2CE /5EE2D3|5EE1E0,43Ch|EBP-414h..EBP-15h|cookie EBP-8; count2, output LEA at5EE26F|
|5F4B0D /5F4B12|5F4AB0,430h|S+28h..S+427h|cookie S+42Ch; count1 string|
|5F4BC5 /5F4BCA|5F4B40,474h|EBP-44Ch..EBP-4Dh|integer text starts EBP-4Ch; count1|

The direct calls were enumerated in exact `.text` disassembly. The full pinned
prefixes end at the return addresses above and start at each enclosing entry.
They cover both branches of458531's format selection, all count/value stores,
the5EE26F output-address construction, and every frame allocation. Smaller call
contexts are additionally checked. No control flow enters the native bytes.

## Installation and refusal

Before the C1 entry runs, TakeOver checks all260 target bytes, all six full
caller prefixes and the call contexts. It then writes only a6-byte JMP/NOP
over the target's initial complete instructions, with checked protection
changes, cache flush and readback. A failed guard or installation terminates
startup through Fatal. There is no original trampoline or fallback. The guard
also refuses blind repeat installation. This security repair is present in
development, no-console development and shipping candidates.

The wrapper accepts only the six reviewed native return addresses. An unknown
caller receives at most a one-byte NUL if writable; it cannot select an assumed
1024-byte capacity. Known callers require a writable full1024-byte region. This
pins the correction to the current build and avoids assuming capacities for
future callers. Independent5F4BF0 is not redirected or declared repaired.

## Focused proof

`tests/localized_substitution_fixture.c` exercises only the corrected helper,
native memory reader and installer against allocated data-only native bytes.
The successful install test exercises the real page-protection/cache APIs,
temporarily changes the entry page's protection, and restores it; it never
calls those native bytes or the patched entry.
It covers ordinary/reordered/repeated substitutions, one-digit semantics,
count0 literals, empty values, invalid index0/counts/missing values, deterministic
clearing, small capacities with canaries, actual1024-byte literal/substitution
boundaries, all six allowed callers, unknown callers, unreadable arguments,
guard-page input/output, and the input-scan limit. Every one of the3177 target
and frame bytes is individually changed to prove install refusal before any
patch. Success changes only the6 entry bytes; original data is never called.
The Python runner hash-pins both exact input executables.

Full DLL candidates and source/dependency hashes are kept separately in
`output_candidate/localized-substitution-20261029/`. Earlier candidates and
paired native-provider files are preserved. Focused fixtures and successful
builds do not prove real C1 UI formatting acceptance, public exploitability,
or completion of the broader native audit. Parent review precedes staging.
