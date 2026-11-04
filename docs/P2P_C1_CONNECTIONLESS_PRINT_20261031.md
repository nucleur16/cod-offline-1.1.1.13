# Exact C1 connectionless print contour, October 31

Image: `E:\codol-classic\C1.exe`, SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
This is a read-only instruction audit, not a live malformed-packet test.

The `CL_ConnectionlessPacket` dispatcher compares its command with the
`print` string at `0xc48114` from `0x496d47–0x496d62`. The handler passes its
message to `0x59f640`, then copies the returned string at `0x496d71–0x496d8a`
using `0x6283a0` with destination `client+0x38` and size `0x100`.
`0x6283a0` copies at most `size-1` bytes and sets the final byte to NUL.
The handler then formats to a stack buffer at `0x496d8d–0x496da4` with a
constant format pointer and size `0x800`; `0x6286a0` terminates at
`destination+size-1`.

The message string reader `0x59f640` checks the read cursor against the
message bounds. It writes to a global string area only while its output index
is below `0x2000` (`0x59f695–0x59f6a5`) and terminates the final byte at
`0x82d2117`. Thus the observed `print` copies have explicit length caps;
the capacity of the `client+0x38` field was not independently derived here.
The reader may continue scanning a long message after the output cap, bounded
by the message length; this review does not characterize its CPU cost under
flooding.

This does not establish that the whole connectionless dispatcher or
`infoResponse`, `statusResponse`, `connectResponse`, `disconnect` and their
downstream parsers are safe. The `statusResponse` path contains an observed
`0x3ff` copy cap at `0x49c0d8`; its later processing remains unqualified.
No source patch was warranted for the examined `print` buffer writes. No
client executable or local service was launched or changed.
