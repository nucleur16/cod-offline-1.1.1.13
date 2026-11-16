# Fresh local log-flush exchanges

Local source/fixture/build extension only. No C1 launch, staging, installation,
runtime service change, commit or push. The prior e081 numeric-fix candidate
and older candidates are preserved. Exact C1 guards, main-thread current-log
lock/stream selection, native CRT ABI, fixed launch prefix, actual PID, private
SDK pipe and isolated backend policy are unchanged.

The startup form remains exactly:

`com_logFlush <actualPID> <launch_nonce32lowerhex>`

It keeps the original seven-field version1 receipt and original path. A new
optional third argument permits a separate fresh exchange against the same
launch and same native current log:

`com_logFlush <actualPID> <launch_nonce32lowerhex> <exchange32lowerhex>`

It writes only:

`main/codol_<launch_nonce>_logflush_<actualPID>_<exchange>.json`

The version2 body contains exactly eight fields:

```
{"version":2,"pid":1234,"nonce":"0123456789abcdef0123456789abcdef","prefix":"codol_0123456789abcdef0123456789abcdef","flushed":true,"c1_reference_sha256":"d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60","abi":"c1-d8211b2-logflush-v2","exchange":"11111111111111111111111111111111"}
```

The ABI/reference constants describe the reviewed C1 contract, not DLL
attestation. The Python owner still verifies its actual native generation.
The exchange changes neither launch prefix nor log filename. It is restricted
to exactly32 lowercase hexadecimal characters; no caller filename, pointer,
address or arbitrary operation is accepted. A malformed exchange refuses
before checking the pipe or invoking the native flush. Shipping rejects both
forms. Unsupported argument counts refuse.

After the existing native stream flush succeeds, receipt publication uses
CREATE_NEW and sharing0 through bounded complete WriteFile, FlushFileBuffers
and CloseHandle. Existing receipts are never overwritten or deleted. Only an
incomplete file created by this invocation is marked for deletion on failure.
Repeating an existing exchange path refuses publication and preserves its
bytes. The native flush may have succeeded before discovering that collision;
the caller must still refuse because there is no new exchange receipt.

Freshness and consumption remain the owner's responsibility: generate a fresh
exchange per request, reject preexisting files, bind the exchange/PID/launch/
prefix/build, retain file identity and request timing, consume at most once,
and never revive a timed-out request from a late receipt. This API does not
renew, extend or create gameplay permission. Readiness checks remain inside
the original short authorization deadline.

The fixture uses actual Windows named-pipe PID/protection policy and receipt
file I/O while mocking only the reviewed native CRT flush and C1 lookups. It
rearms its private pipe between repeated requests, modeling the owner's
accept loop. The41 development cases cover all prior guards/refusals plus
distinct exchanges, version1 coexistence, replay/preexisting no-overwrite,
strict exchange validation, wrong pipe parent, guard mismatch and partial
write cleanup. During every write the fixture attempts to open the same file
for reading and requires ERROR_SHARING_VIOLATION, proving publication remains
closed until completion. The shipping fixture verifies both forms refuse.
No C1 or original vulnerable formatter/CRT code is executed.

Candidates and source/dependency/output receipts use the separate directory
`output_candidate/local-log-flush-exchanges-20261029/`. Previous formatter
repairs remain included. This is build/fixture evidence; actual repeated C1
readiness and both-player CA_ACTIVE qualification require parent review and
subsequent owned local tests.
