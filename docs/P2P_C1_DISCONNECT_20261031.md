# Exact C1 connectionless disconnect contour, October 31

Image: `E:\codol-classic\C1.exe`, SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
Read-only disassembly; no hostile datagram or client process was run.

The connectionless dispatcher matches `disconnect` at `0x496ba6–0x496bc1`
and calls `0x496300`, passing the source address and either token 1 or an
empty reason. `0x496300` returns when connection state is below 3. For
later states it compares the packet source with the active channel remote
using `0x5a37c0`, which calls the exact-address comparator `0x5a3700`
(`0x496318–0x496390`). Unlike the `connectResponse` branch, it does not
use the base-address comparator. For address type 5, `0x5a3700` checks
the word at `+0x08` and the address bytes at `+0x0a..+0x13`.

In state 8 or above, the handler also ignores disconnects for the first
`0xbb8` milliseconds after its stored state time (`0x496392–0x4963b9`).
An accepted reason token reaches `0x4960e0`, which selects UI/error paths
using comparisons and localization helpers. The complete behavior of those
helpers and all tokenization edge cases is not established here.

This narrows arbitrary-source disconnect spoofing: the visible state-change
path requires the active endpoint, including its compared port for type 5.
It does not authenticate a malicious active host, prove source-IP
unspoofability, or clear all C1 connectionless parsing. No RCE or source
defect was demonstrated in the observed address check; no patch was made.
