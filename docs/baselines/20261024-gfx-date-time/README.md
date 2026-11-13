# C1 Date.setTime timezone cache repair

The original C1 Date constructor records the standard timezone bias at object
offset `0x20`, its epoch milliseconds at `0x28`, and enables dynamic daylight
saving adjustment with byte `0x24=1`. Original `Date.setTime` at `0x88A640`
preserves/clips the supplied epoch but clears that byte without refreshing the
cached bias. In summer this makes a timestamped PM one hour earlier in Pacific
time while the sender's new Date() remains correct. Lord Howe has a thirty
minute variant. The shared English/Chinese AS path is the same.

`gfx_date_time.c` calls the original setTime, then updates only the local offset
cache using the target UTC date and the OS dynamic timezone rules. The explicit
disabled-DST preference uses the standard bias. It changes neither the epoch,
the original result/NaN/TimeClip behavior, nor the flag cleared by setTime.
Unsupported Windows calendar dates or API failures retain the original result.
No clock, timezone, server time, AS file, or language resource is changed.

The patch checks fourteen exact native code/table regions and requires the
member initializer not to have run. It changes four immediate bytes in the
existing nineteen-byte initializer at `0xB36E50`; that initializer installs the
replacement member pointer at `0xCE7D38`. Original setTime instructions remain
intact. Install after FillImports, before original game entry/CRT initialization.
The measured receiver is thiscall ECX; output pointer and double occupy twelve
stack bytes. The fastcall wrapper's unused EDX argument preserves that ABI.
All object and output pointers remain synchronous and are never retained.

Supported owned private PE SHA256 values:

- Shipping: `a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976`
- Retained: `d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`

Their Date implementation differs at three verified GetTimeZoneInformation IAT
operands: shipping `0xB4222C`, retained `0x9A1D414`. Shipping's loader import
table independently binds the former to KERNEL32.dll/GetTimeZoneInformation.
All other selected native/CRT/table bytes agree. No private binary is included.

## Reproduce

From the client source root, with a C99 compiler and Python:

```sh
python3 -B docs/baselines/20261024-gfx-date-time/run_regressions.py --output-dir /scratch/date-policy
```

This compiles the actual portable policy and runs eighteen checks. It needs no
game files and runs wherever a C compiler does. `--cc` selects the compiler.

For original x86 execution, install `pefile` and `unicorn` in the test Python
environment and build the Win32 bridge with MinGW:

```sh
i686-w64-mingw32-gcc -O2 -Wall -shared -static -static-libgcc -I. docs/baselines/20261024-gfx-date-time/policy_bridge.c -Wl,--export-all-symbols -o /scratch/date_bridge.dll
python3 -B docs/baselines/20261024-gfx-date-time/run_native.py --image /owned/C1.exe --bridge /scratch/date_bridge.dll --output /scratch/date-native.json
```

The image argument is mandatory and its full SHA256 is checked before mapping.
The bridge is never loaded by Windows or injected into a process. Unicorn
executes the actual compiled wrapper, original Date/CRT/getters/initializer.
Fifty-two checks pass on each image: return pointer and stack ABI, six offset
cases, original epoch/NaN/TimeClip equivalence, four failed API paths, disabled
DST, the actual initializer, fourteen mutated guard regions, late install, and
missing API rejection, actual getTime/eight UTC getters and repeated same-object
season changes. Negative controls show why original setTime, resetting
the dynamic flag, and adding a fixed hour are incorrect.

Windows conversion is a named double in that x86 fixture. Independently compile
and run the native Win32 rule fixture on Windows:

```sh
i686-w64-mingw32-gcc -O2 -Wall -static -I. docs/baselines/20261024-gfx-date-time/windows_rules.c -o /scratch/date_rules.exe
```

Execute the resulting fixture only on Windows. It calls the actual production
offset routine and actual Windows conversion APIs using explicit enumerated
timezone snapshots. It never sets the system timezone or clock. Its 107 checks
include 94 independent IANA-derived reference cases, UTC, Pacific seasonal and
2006/2007 historical rule changes, spring/fall UTC boundaries, Lord Howe thirty
minute transitions, Sydney southern-hemisphere rules, India/Nepal offsets,
local year boundaries, pre-epoch dates, disabled DST and invalid policy inputs.
The fixed reference inputs are in `timezone-cases.json`.

## Acceptance boundary

Offline tests pass; these tests do not represent live PM acceptance. Clock output
and WinAPI calls are explicit doubles in the x86 fixture, with actual OS rule
conversion checked separately. The virtualized native PM clock was not executed
to completion. The same-epoch original Date execution proves the cache defect
without making a claim about that VM. English/Chinese reciprocal PM timestamps
after one rebuilt-client restart remain the live acceptance gate.

Microsoft API references:
[SystemTimeToTzSpecificLocalTimeEx](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-systemtimetotzspecificlocaltimeex),
[EnumDynamicTimeZoneInformation](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-enumdynamictimezoneinformation).
Enumeration is a test-only advapi32 API; production resolves the two kernel32
dynamic-timezone APIs and fails closed if unavailable.
