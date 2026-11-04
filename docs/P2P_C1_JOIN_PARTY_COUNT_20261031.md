# Exact C1 joinParty count review, October 31

## Image and command route

- Exact `E:\codol-classic\C1.exe` SHA256:
  `d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
- The C1 connectionless command table pairs the `joinParty` string at
  `0xc4bf1c` with handler `0x4b1520` at table address `0xc3d730`.
  Connectionless dispatch reaches that table via `0x4aaad0` then
  `0x4b3140` after direct-command cases. The command is digit-prefixed:
  `0x4aaae0..0x4aab1e` gates the leading character against party mode
  (with a special branch for prefix `2`),
  and `0x4b3160..0x4b31a1` compares the remainder with `joinParty`.
  The handler requires source address type 4 at
  `0x4b153a`; party ID and state checks precede the count loop.
- This establishes an exact-image C1 handler, not that a guest datagram in
  the current private relay reaches it. The relay routes registered guest
  bytes to the embedded native server at `127.0.0.1:17760`; a later route
  from that socket to this C1 connectionless dispatcher is unproven.

### Fresh receive-path trace, November 1

- The packet dispatcher at `0x497360` checks length at least four and the
  four-byte `0xffffffff` connectionless marker at `0x49736d..0x497378`.
  It calls the command tokenizer at `0x497080` from `0x4973aa`.
- The tokenizer calls the command handler at `0x4963e0` from `0x4971ac`
  or `0x497289`. That handler calls the digit-prefixed party dispatcher
  `0x4aaad0` at `0x496680` and `0x4966cd`; the latter calls the party
  command table dispatcher `0x4b3140` at `0x4aac3c`. Its table contains
  `joinParty` -> `0x4b1520`. This is an exact-image path from a
  connectionless packet to the vulnerable count loop, conditional on the
  prefix, party state/ID, address type, and other handler gates.
- The network receive wrappers at `0x595d6d`, `0x595de6`, and `0x595f36`
  call `0x497360`. This does not establish that the private P2P relay's
  Guest-to-native route forwards back into a C1 receive wrapper. It also
  does not demonstrate a hostile packet passing the party checks or
  executing the local hook in a running C1 process. Keep this guard and
  those live reachability checks as release gates.
- In the isolated local bridge source, `_native_to_peer(path, False)` reads
  bytes from the stable native socket only when their source is
  `127.0.0.1:17760`; `_peer_to_native(path, False)` forwards its paired
  bytes to the configured Guest client endpoint. This establishes a
  *bridge-level* native-host-to-Guest-client route. The reverse route sends
  Guest bytes to native port 17760, so it alone does not feed the Host C1
  parser. Whether the actual C1 Guest socket takes a crafted host packet
  through this dispatcher and reaches the count write remains unobserved.
- The local client `hostgate.c::HostGateOk` accepts every `127/8` source
  address. The bridge's Guest endpoint is restricted to `127.0.0.1` and
  `_peer_to_native(path, False)` sends host-side bytes to it, so the gate
  does not discard those bytes before C1 parses them. This is a concrete
  local host-to-Guest trust path, conditional on the bridge pairing and C1
  socket use. The `joinParty` handler still requires a matching party ID,
  mode, source type, and state; no crafted packet was sent to C1.
- A direct PE-byte assertion against the pinned image rechecked the receive
  wrapper calls, tokenizer/command/party call targets, the four-byte OOB
  marker comparison, and the handler's source-type gate. It passed. This
  is static verification only; it did not run the game or exercise the hook.
- A focused loopback bridge test sent an inert `0xffffffff` + `2joinParty`
  marker from a test host UDP socket and observed the exact bytes on the
  Guest peer side. It used `GameBridge`'s real forwarding code with a
  stand-in transport module for its unavailable `aioice` import; the
  independent Guest RCON rejection test also passed. Four tests passed in
  total. Backend test source SHA256:
  `84f5dd96dcbf4a8fc3ad928e080ee4e7d9e43b0ef3e1c74ef6de3d2b4f1ad875`.
  This proves the short bridge's packet forwarding, not C1 dispatch, the
  renewable bridge, or exploitability.

## Defect and local repair

- Token 7 is converted with `atoi` at `0x4b1703` and stored in ESI. The
  original bytes at `0x4b17cd` are `83 fe 01 7d 07` (`cmp esi,1` / jump if
  at least 1); they provide no upper bound.
- The loop at `0x4b17e0..0x4b1823` reads tokens starting at index 9 and
  writes each converted dword to `[esp+ebx*4+0x30]` at `0x4b1800` before
  subsequent checks. Its loop bound is the parsed ESI count. Missing
  tokens can yield an empty string and zero; they do not prevent the write.
- The handler's local frame is `0x9c` bytes, with an observed next local
  starting at `[esp+0x84]` and a stack cookie at `[esp+0x98]` after the
  pushed registers are accounted for. A large count therefore writes past
  the local array into adjacent stack data. The downstream party scan at
  `0x4ac761` iterates `0x13` (19) entries, supporting a conservative
  19-count cap.
- Token 3 is separately converted into EBX at `0x4b1652` and passed as
  player count to `0x4ac730`. That builder adds the player count to an
  existing-player count at `0x4ac77f` and uses a signed capacity comparison
  at `0x4ac787`. A very large positive count can wrap the addition when
  existing players are present. The caller hook also requires player
  count 1..19 before the builder is called.
- The builder clears a `0x6d4`-byte, 19-record region at `0x4ac549`. At
  `0x4ac8c0`, EDX holds its existing group count and EBX the incoming
  subparty count. Its later 0x5c-stride writes can exceed the region when
  those counts sum above 19. A second exact-byte-checked hook at that
  site rejects negative counts and sums above 19 before the write.
- The same builder appends filler records at `0x4ac955` according to a
  computed remaining-player count in EAX. A third exact-byte-checked hook
  at `0x4ac945` requires EDX existing records plus a positive EAX filler
  count to fit in the same 19-record region. It preserves the original
  skip path for nonpositive counts and emulates the overwritten
  `mov ecx,edx` before the original indexed-write path.
- `join_party_guard.c` replaces two exact five-byte branches and one
  six-byte branch after
  checking their bytes. Counts below 1 retain C1's original error path;
  oversized counts take existing balanced cleanup before the first indexed
  write. The builder guard preserves its original pair/single branch for
  valid counts and takes its existing error return otherwise. The candidate
  refuses startup if any guard cannot install. No stable client DLL
  was replaced.

## Verification and limits

- `tests/join_party_guard_fixture.c` executes all three actual 32-bit hook ABIs
  at boundary counts, including -1, 0, 1, 19, 20 and INT_MAX. It verifies
  each rel32 patch and refusal of unexpected original bytes. Linux 32-bit
  ASan builds passed at `-O0` (SHA256
  `7e7cecd87d79c1c16aede4e377de5fce749bf6d97a9119b15dc292c2e081a8d5`)
  and `-O2` (SHA256
  `2b1633b8dd0253db14276f618e586e2d5deb387001d74556f4c6124385ed039d`).
- The Windows-target local DLL candidate compiled. SHA256:
  `3d5db47f759106a7e0f2f760de21db7bade64f1f318d040e8f935477d34ffac8`.
  The existing stable Host/Guest DLLs remain SHA256
  `d9aaa130510369ea54ce0bfb9e7b1f28ad7aa4a277e7a7d73db33ac86740a87c`.
- No actual C1 packet, gameplay, hostile-packet, long-session or public/NAT
  run exercised this hook. The protected readers, later party processing,
  and complete dispatcher route remain open. This is a confirmed
  exact-image stack-write defect with a built local candidate, not a
  demonstrated CoDOL RCE or release approval.

The public [CVE-2019-20893](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2019-20893)
describes a `joinParty` buffer overflow in *Modern Warfare 2*. Its
disclosure motivated this check. The CVE does not identify this CoDOL C1
build; the evidence for C1 above comes from its own bytes and call route.

Fresh November 1 guard fixture rerun against unchanged source: Linux 32-bit
ASan/UBSan O0 SHA256 1af8add5730eed0d5a87ccd601a86491a2d51031fab0d97cdcf5bc3b1805f0e8;
O2 SHA256 5d0b26ebd16440c74b5dfccbf198df898453d60c4c995130fb03bf9e300f34c8.
The exact C1 branch targets at 0x4b1781, 0x4b17d2, 0x4b17d9,
0x4ac8c5, 0x4ac911, 0x4acb25, 0x4ac94b, and 0x4ac965 were
read back from the pinned image. No actual C1 packet was sent.
