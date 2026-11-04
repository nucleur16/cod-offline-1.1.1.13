# Local native startup-log visibility

Source/build fixture only. No C1 invocation, installation, backend change or
gameplay acceptance is part of this change. The previous DLL candidates remain
in `output_candidate/verified-script-ipc-20261029/`; new candidates and build
receipts use `output_candidate/local-log-flush-20261029/`.

The subsequent refusal-diagnostics build uses
`output_candidate/local-log-flush-diagnostics-20261029/`. It retains the original
guards and emits only a fixed stage label, for example
`k33logflush: refused (stage=native-filename)`. Stages distinguish the policy,
pipe, individual native byte guards, current prefix, stream validation, native
flush and receipt operations without exposing paths, pointers, credentials or
caller text. Its 25 development fixtures assert each exercised refusal stage;
the additional partial-write fixture verifies owned incomplete-receipt cleanup.

The import-aware build uses `output_candidate/local-log-flush-import-20261029/`.
The actual packed executable preserves its original `.text`, while the C1
reference is a reconstructed dump. At `0x594020`, both push the same native
critical-section address and have the same call shape/continuation, but the
packed executable calls through `0x00B421EC` and the reference through
`0x09A1D2FC`. This was confirmed both by the parent's bounded live byte read and
the packed executable file (SHA256
`a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976`).
`c1_tables.h` names `0x00B421EC` as `KERNEL32!EnterCriticalSection` and
`FillImports` resolves it through the genuine Windows loader APIs.

The corrected guard pins the instruction/lock argument and continuation across
19 bytes, accepts only those two exact operand values, and requires the live
slot pointer to equal `GetProcAddress(kernel32, EnterCriticalSection)`. It does
not accept arbitrary import slots, even another EnterCriticalSection alias.
Thirty development fixtures and a shipping refusal pass, including both known
encodings, arbitrary slots, wrong/null function pointers and a modified
continuation. A further isolated fixture loads all guard bytes directly from
the actual packed executable; it passes with native dvar/CRT calls still
injected. Live C1 flush success remains a separate acceptance step.

The fixed main-thread engine command is:

```text
k33logflush <actual decimal C1 PID> <32 lowercase hexadecimal nonce>
```

The native `1436` dvar must equal `codol_<nonce>`. The command accepts no filename,
address, pointer or callback. It requires the explicit local multi-client opt-in,
the strict five-line `dwauth.ini` contract with both hosts exactly `127.0.0.3`,
verified-script opt-in, the exact private verified-script pipe name, and a kernel
pipe-server PID equal to the configured SDK parent. Missing/busy pipes refuse
immediately; no request is sent and no game traffic is touched. Shipping always
refuses and excludes the development pipe/native flush implementation.

On success it exclusively creates, with sharing disabled until close:

```text
<exe>/main/codol_<nonce>_logflush_<actualPID>.json
```

The bounded ASCII receipt contains `version`, `pid`, `nonce`, `prefix`,
`flushed:true`, `c1_reference_sha256` and `abi:c1-d8211b2-logflush-v1`. It contains
no log content, credentials or process capabilities. Existing receipts cannot be
overwritten. The Python owner must validate its retained child identity, native
generation and launch-scoped log identity, then consume the receipt once within
its original deadline. The reference hash identifies the reviewed ABI; it is
not remote attestation or the loaded DLL's hash.

## Exact-build evidence

Reference `E:/codol-classic/C1.exe`, SHA256:
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
All addresses below are from that image at base `0x400000`.

| Native location | Observed behavior |
| --- | --- |
| `0x593F2C` | Looks up `1436` through cdecl `0x6232C0`, reads dvar string at `+0xC`, and formats `%s_%s` with `console_mp.log`. |
| `0x58D080` | Rotates the existing active filename to `%s-%03i%s`, first missing index 0..19 or oldest existing timestamp. A new unique prefix has no predecessor to rotate. |
| `0x594010` | Com_LogPrint locks the critical section at `0x2B62AB0`, uses current native file handle `[0x2574660]`, and writes through `0x615120`. |
| `0x613C60` | Main-thread allocation uses native file handles 1..48. |
| `0x6141F0` | Opens the log in `w+t`; native file table has stride `0x11C`, FILE pointer at `0x2AAA320 + index*0x11C`, relative filename at entry `+0x1C`. |
| `0x615181..0x61518E` | Optional per-write flush calls cdecl `0xADB1F7(FILE*)`. Log open initializes the optional sync field to zero. No stock `logfile 2` behavior is assumed. |
| `0x62C0D3..0x62C0F3` | Independently obtains this exact current-log FILE pointer and passes it to native `0xADB1F7`. |
| `0xADB1F7` | Native CRT `fflush`; the non-null path locks that stream, flushes it and returns status. |

The helper byte-checks the native flush entry, current-log caller, dvar entry,
prefix lookup and Com_LogPrint lock call. Under the same native critical section
it checks the handle bound, complete entry readability, exact nonce-prefixed log
filename and non-null readable stream. It calls **C1's CRT** on that one owned
stream. It never calls this DLL's `fflush` on C1 memory and never uses
`fflush(NULL)`. Only native return value zero permits a receipt write.

## Fixture boundary

`tests/local_log_flush_fixture.c` runs in isolated x86 Windows fixture processes.
It loads the actual reference bytes for every guard, uses real Windows named-pipe
server PID checks and receipt/file operations, and injects only the native dvar
lookup and stream flush call. These fixtures therefore prove policy, guards,
call argument/lock discipline and receipt handling; they do not execute the C1
CRT or prove live C1 log visibility.

`tests/test_local_log_flush.py` checks the reference SHA256 and covers positive
receipt publication, exclusive creation, wrong/malformed PID and nonce, prefix,
opt-in, pipe name/server identity, all five byte guards, wrong handle/filename,
null stream, native flush refusal, disabled multi-client mode and wrong/mixed DW
routes. Shipping has a separate refusal run. No game sockets are bound.
