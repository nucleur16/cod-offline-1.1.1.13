# In-process host zone-log truncation, October 31

The local in-process host's `PiLogC1Zones` appended C1 zone names to a
1024-byte stack buffer. Its loop allowed another append while `used < 960`
but did `used += _snprintf(...)` without checking the return. On Windows,
`_snprintf` returns `-1` on truncation and need not terminate the buffer.
A sufficiently long later zone name could therefore leave the final log
argument unterminated. Subsequent loop iterations would use a stale buffer
position and could overwrite earlier text. This is a source-level bounds and
logging defect; the actual zone-name provenance and remote reachability were
not established.

The local source now stops on a negative or capacity-exceeding return and
terminates the end of the buffer before logging. `used` remains nonnegative
and below 960 on every continuing iteration, so the next write starts
inside the 1024-byte array with at least 64 bytes remaining. No C1 image,
installed client, stable Host/Guest DLL or service was changed.

The full Windows-target local DLL candidate rebuilt under the existing WSL
MinGW toolchain. SHA256:
`bf5d4ecb6dfae095b4a23433dc7ce8c4be10ce98f9c0b7f5f68fda2a249cdc6f`.
This is compilation evidence only. The logging path was not exercised in an
actual C1 client after the change, and no RCE was demonstrated. Public P2P
and release gates remain open.
