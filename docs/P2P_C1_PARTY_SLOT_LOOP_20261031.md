# Exact C1 party refresh slot-loop bound, October 31

Exact `E:\codol-classic\C1.exe` SHA256:
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.

In `0x4b37d0`, the party refresh path copies 19 records from
`[ebx+0xb8+i*0x108]` into a 19-record destination at
`0x4b38f2..0x4b391a`. Its following loop starts at `0x4b391c` and
uses the total-slot dvar at `0x172010c` as its bound. Each iteration
reads `[ebx+0x58+i*0x108]` at `0x4b3930..0x4b3936`; the original
back edge `0x4b398c` compares only against the dvar count. If that
count exceeds 19, the outer loop reads past the 19 party records.
The dvar registration at `0x4aadd5` allows a maximum above 19.
Actual remote control of this dvar is not established.

The local `party_slot_loop_guard.c` replaces the exact five-byte
`cmp esi,[ecx+0xc]` / `jl 0x4b3930` sequence at `0x4b398c`:
`3b 71 0c 7c 9f`. It continues the original loop only while the
next index is below both the original signed dvar count and 19.
All registers are preserved. The installer verifies original and
replacement bytes and fails client startup if it cannot install.

The 32-bit ASan/UBSan fixture executed the branch ABI at counts 0,
1, 10, 19, 20, `INT_MAX`, negative and null-pointer cases at `-O0`
and `-O2`; both passed. The Windows-target DLL was built locally.
Fixture SHA256: `-O0`
`11fb74e62c4a2305c77e644d34e44e028ab98ae2eddda4f7d9a6387ebabf46c3`,
`-O2` `2aa145c39a4b371b30cdc4c90f13668e0f9f1c65f481a2ba4f4397a65cb64ca93`.
This is fixture and compile evidence, not actual C1 party-refresh or
hostile-packet acceptance. Other count consumers remain to audit.
