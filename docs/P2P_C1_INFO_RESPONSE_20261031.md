# Exact C1 infoResponse contour, October 31

Image: `E:\codol-classic\C1.exe`, SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
Read-only disassembly; no C1 or server process was launched.

The connectionless dispatcher compares `infoResponse` at `0xc480cc`
(`0x496adc–0x496af0`) and calls `0x49ad40` at `0x496b24`.
The handler requests a `0x400`-byte first-line read into `ebp-0x80c`
at `0x49ada7–0x49adb2`. The reader target `0x8460f7f` is in C1's
protected `.tvm0` section; its actual bounds and termination behavior
are not established by this call-site argument.

The visible server-browser record search at `0x49a950` stops at `0x80`
entries of stride `0x130`. When a new record is inserted, `0x49af81`
rejects an index of `0x80`; `0x49afac–0x49b041` writes fields within
the selected stride. The visible field-population helper `0x49ab60`
copies text into record fields with requested lengths `0x3f`, `0x17`
and `0xf`, followed by forced terminators at offsets `+0xc7`, `+0x11f`
and `+0x12f`. This narrows these visible writes. It does not prove the
protected input reader, info-string lookup at `0x8346291`, numeric
conversions, or every downstream browser consumer safe.

The native local P2P/relay prototype has separate admission, TURN and
authenticated-game-frame bounds; those do not remove this C1 parser
surface if an attacker can make C1 accept a connectionless response.
No RCE was demonstrated in the visible `infoResponse` writes, and no
binary patch was made. The protected first-line read and end-to-end
packet reachability remain release gates.
