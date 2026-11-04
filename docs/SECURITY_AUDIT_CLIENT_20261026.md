# CoDOL Client Security Audit — inherited IW4 RCE review

- Date: 2026-10-26
- Scope: **client side** — the C1 game image `codoMP_client_shipRetail.exe`
  (build 1.1.1.13, IW4-derived, VMProtect-wrapped) and the client patch DLL
  **Kernel33.dll** (repo `codomp_clientShipRetail-patch`, branch cut from
  `origin/dev`).
- Reason: P2P hosting is being built (`feat/p2p-hosting`). Once any player can
  host, a malicious host/peer or a malicious lobby must not be able to execute
  code on other players' clients. This audit looks for remote-code-execution
  classes inherited from IW4 and fixed upstream by IW4x / Plutonium /
  alterIWnet, plus any introduced by Kernel33 itself.
- Method: static analysis. Kernel33 reviewed from source (full read). The game
  image reviewed by disassembly with GNU `objdump` (i386 / pei-i386). The main
  `.text` of `codoMP_client_shipRetail.exe` is present **unencrypted on disk at
  its live VAs** (confirmed: the bytes at 0x496482 / 0x496491 match Kernel33's
  own byte-guards exactly), so direct VA disassembly is valid for the
  non-virtualized functions. References: IW4x (`D:\CoD\iw4x_development`),
  OpenBLOPS, CoD4x server source for function shapes.
- Author: michaelpmaurerii@gmail.com (owner, auditing own software).

## Tooling note / honesty of claims

Each finding is tagged:
- **VERIFIED(src)** — read in Kernel33 source, high confidence.
- **VERIFIED(dis)** — confirmed in the game-image disassembly at the stated VA.
- **REASONED** — inferred from disassembly shape / IW4 derivation but not
  exhaustively proven instruction-by-instruction.
- **OPAQUE** — the function appears VMProtect-virtualized (no plaintext string
  xref / not a plain prologue), so static confirmation was not possible with
  the available tooling (no Ghidra/IDA licence, no radare2/rizin installed).

No live client was touched. No probe-server run was needed for this pass (no
code change was shipped — see "Fixes / staging").

---

## Findings table

| # | Entry point | Where | Severity | Reachable by | Status |
|---|-------------|-------|----------|--------------|--------|
| 1 | Voice connectionless packet `"v"` | Kernel33 `voice.c:1291 VoiceOnPacket` (replaces native 0x00496482) | n/a (hardened) | malicious host/server | **SAFE — VERIFIED(src)** |
| 2 | Voice jitter store | `voice.c:131 VoiceInsert` / `:471 VoiceStore` | n/a | host | **SAFE — VERIFIED(src)** |
| 3 | Discord getinfo reply parse | `discord.c:470 DiscordQuery` | n/a | **loopback only** | **SAFE — VERIFIED(src)** |
| 4 | Discord IPC frame read | `discord.c:296 DiscordReadFrame` | n/a | local Discord pipe | **SAFE — VERIFIED(src)** |
| 5 | Infostring value extraction | `discord.c:148 DiscordInfoValue` | n/a | host (via #3) | **SAFE — VERIFIED(src)** |
| 6 | Managed-room info hook | `managed_quick.c:17 ManagedRoomInfoHook` | n/a | lobby/host room list | **SAFE — VERIFIED(src)** |
| 7 | Launcher self-upgrade bridge | `launcher_update_bridge.c:50 TryLauncherUpgrade` | n/a | local staged file | **SAFE — VERIFIED(src)** |
| 8 | Local multi-client mutex (QA) | `local_multi.c` | n/a | compiled out in prod | **SAFE — VERIFIED(src)** |
| 9 | Parity playerstate snapshot read | `parity.c:465 ParitySaveOld` | n/a | local predicted PS | **SAFE — VERIFIED(src)** |
| 10 | Cbuf / command helpers | `c1game.c:72 C1CbufAddText` | n/a | Kernel33-internal only | **SAFE — VERIFIED(src)** |
| 11 | Usermap file redirect hooks | `usermaps.c` CreateFileA/FindFirstFileA | n/a | server map name | **SAFE — VERIFIED(src)** |
| 12 | Reliable server-command window | game image, `CL_ParseServerMessage` 0x00497360 | — | malicious host | **BOUND PRESENT — VERIFIED(dis)** |
| 13 | MSG_ReadByte / message reads | game image 0x0049D934 | — | host | **BOUND PRESENT — VERIFIED(dis)** |
| 14 | `CL_ParseGamestate` configstring index/length | game image (string @0xC49358) | High *if absent* | malicious host | **OPAQUE — VMProtect-virtualized, UNVERIFIED** |
| 15 | `CL_ParseSnapshot` entity-num / areamask | game image ~0x0049D800 | High *if absent* | malicious host | **REASONED — reads bounded, per-field bound not fully proven** |
| 16 | Other connectionless packets (infoResponse / statusResponse / connectResponse / print / disconnect) | game image, dispatcher around 0x00496xxx | Med–High | malicious host/peer | **REASONED — native, not re-implemented; not exhaustively RE'd** |
| 17 | BD / DemonWare (Tencent) lobby packets (party/room/relay) | native Tencent BD SDK + Python lobby | High | malicious lobby/peer | **NOT AUDITED — largest residual surface** |
| 18 | Fastfile / zone / IWD parse of downloaded content | native C1 zone loader | High | malicious host (download) | **NOT AUDITED — native parser** |

No confirmed, fixable RCE was found **in Kernel33's own code**. The unverified
risk is concentrated in native C1 subsystems that Kernel33 does not currently
re-implement or wrap (rows 14–18).

---

## Kernel33 source review (what was checked and why it is safe)

### 1–2. Voice packets (`voice.c`) — the one connectionless packet Kernel33 owns
Kernel33 hooks the native `"v"` connectionless handler at `0x00496482`
(`VOICE_RECV_VA`, byte-guarded `68 7C 7E C4 00 / 6A 10 / E8…`) and replaces it
with `VoiceOnPacket` (`voice.c:1291`). This is the single remote-data parser
Kernel33 fully re-implements, and it is written defensively:

- `Readable(msg,0x18)` before touching the msg struct; `data`/`size` validated
  (`size < 4+2+6 || size > 0x20000 || !Readable(data,size)` → return).
- Sender must equal the server address (`from[0..2]` vs `clc.serverAddress`).
  In P2P the host *is* the server, so this does not keep a malicious host out —
  but every subsequent read is bounded, so that only enables spoofing of who is
  "talking", not memory corruption.
- `talker` bounded `< VOICE_TALKERS(64)` (unsigned byte, so ≥0); `count`
  constrained `1..8`; per-frame loop checks `off >= avail` and
  `off + 1 + len > avail` before each `VoiceInsert`.
- `VoiceInsert` (`:131`) and `VoiceStore` re-validate
  `talker`/`len (1..VOICE_MAXFRAME=255)` **before** indexing `g_talkers[]` and
  before `memcpy(s->data,data,len)` into `BYTE data[255]`. Opus decode uses
  `s->len` into a `VOICE_FRAME` float buffer with `got < VOICE_FRAME` clamp.

Result: no OOB. This is a model for how the other connectionless packets should
eventually be handled.

### 3–5. Discord presence (`discord.c`)
- `DiscordQuery` only ever sends `getinfo` to `INADDR_LOOPBACK` (`:486`), so a
  remote host cannot feed it. The reply goes into `reply[4096]`, NUL-terminated
  at `reply[n]` with `n < 4095`, prefix-checked `infoResponse`, and parsed with
  `DiscordInfoValue`.
- `DiscordInfoValue` (`:148`) clamps the copy to `n-1` before `memcpy` — no
  overflow regardless of input.
- `DiscordReadFrame` (`:296`) rejects `len > DISCORD_MAX_FRAME (16 KiB)` and the
  caller's `body` is `DISCORD_MAX_FRAME+1`, so `body[len]=0` is in bounds. This
  path is the local Discord named pipe (trusted-ish, local only).

### 6. Managed Quick room (`managed_quick.c`)
`ManagedRoomInfoHook` copies room info through a fixed `local[0x48]` only after
`Readable(info,0x48)`; the embedded `mode` pointer is followed only behind
`Readable(mode,9)`. The vtable/controller reads are `Readable`-guarded. Bounded.

### 7. Launcher upgrade bridge (`launcher_update_bridge.c`)
This is a one-time self-replace of the *launcher*, not a network download: the
replacement's SHA-256 is **compiled in** (`CODOL_LAUNCHER_BRIDGE_HOST_SHA256`)
and the staged file must hash to exactly that value before it is executed; an
unpinned dev build is inert (`strlen(expected)!=64`). The parent process image
is validated against `TCLS\LauncherHost.exe` / `CODOLLauncher.exe`. There is no
attacker-controlled URL, no elevation. Integrity is enforced by the pinned hash.
*Observation (not a vuln):* it trusts whatever file is already staged at
`TCLS\LauncherHost.pending.exe` on disk — but it will only *run* it if it hashes
to the pinned release, so a planted file cannot be used to run arbitrary code.

### 8. Local multi-client (`local_multi.c`)
Entirely behind `#if CODOL_LOCAL_TESTING==1` (not defined in `build.sh` /
`build-dev.sh`), and additionally gated by a command-line flag plus a loopback
routing guard reading a ≤256-byte `dwauth.ini`. Not present in production.

### 9. Parity logger (`parity.c`)
Reads the **local predicted** playerState (`cg+0`) at fixed offsets into
fixed-size `g_prev` arrays (`memcpy` sizes are `sizeof(dest)`). No network buffer
is parsed here; all `_snprintf` calls use `sizeof(buf)`. Safe.

### 10. Command / Cbuf helpers (`c1game.c`)
`C1CbufAddText` and `C1AddCommand` are only ever called with
Kernel33-controlled, constant or locally-sourced strings (console input,
`k33_cmd_<pid>.txt` which is test-only behind `+set k33_cmdFile 1`, and
`set <dvar> <int>`). **No server- or peer-controlled string reaches Cbuf**, so
there is no remote command-injection path through Kernel33. `C1Printf` uses a
fixed `buf[1024]` with `_vsnprintf(...,sizeof-1)` and an explicit NUL — and is
always called with a Kernel33-owned format string (no format-string injection).

### 11. Usermaps (`usermaps.c`)
Kernel33 hooks the kernel32 import slots for `CreateFileA` / `FindFirstFileA`
etc. to redirect zone and `main_shared` lookups. The redirect only substitutes
files from a **pre-built index enumerated from the local `usermaps` directory at
startup** (`UserMapIndex`); it matches requested names against that index. A
server-supplied map name cannot cause a path-traversal open through this layer,
because nothing concatenates the server name into a path here — the native C1
map loader builds the path, and the hook only swaps in an already-indexed local
file. All `_snprintf` in this file use `sizeof(dst)` with explicit NUL. (The
`.ff` content parsing itself is native — see row 18.)

---

## Game-image (native) review

### 12. Reliable server commands — **bound present (VERIFIED(dis))**
**October 31 correction against the exact C1 image** (SHA256
`d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`):
`0x00497360` is a packet path that dispatches `0xffffffff` connectionless
packets to `0x00497080`; its comparison at `0x00497527–0x00497539` is not the
reliable-command ring guard. The reliable-command handler starts at
**0x00487730**. It compares incoming sequence `edi` with
`[client+0x4014c]-0x100` at `0x00487743–0x00487751`, taking the drop/error
path when the sequence is too old, and rejects sequence greater than
`[client+0x4014c]` at `0x004877a3–0x004877b7`. The ring slot is selected by
`sequence & 0xff` at `0x004877ba–0x004877d5`. This proves the 256-slot index
and sequence checks at that entry; it does not prove every downstream command
parser is memory safe. The previous address and direction-of-bound description
were inaccurate.

### 13. MSG primitives — **bound present (VERIFIED(dis))**
**October 31 exact-image correction:** `0x0049D934` is an inlined byte read
inside the snapshot region, not the `MSG_ReadByte` function. It checks
`readcount` against the declared message size and rejects a negative cursor
before loading one byte. The standalone byte reader at **0x005A00F0** performs
the equivalent checks for the main and split buffers and returns `-1` at the
end of the message. `MSG_ReadLong` at **0x0059F3A0** similarly checks four
bytes before reading. These are bounded primitives for a valid `msg_t`; they
do not prove every parser uses them correctly or that individual decoded
indices, lengths, and destinations are bounded.

### 14. `CL_ParseGamestate` configstring index/length — **OPAQUE / UNVERIFIED**
The strings `CL_ParseGamestate: begin parse of gamestate` (0xC49358),
`bad command byte %d` (0xC49388) and `bad clientNum %i` (0xC493B0) are present
and correctly located, **but have no plaintext `push imm32` / `mov reg,imm32`
xref anywhere in the on-disk code section.** The exact image contains a
`.tvm0` section and protected call targets, but absent plaintext xrefs do not
prove which routine is protected. Its configstring index bound
(`index < MAX_CONFIGSTRINGS`) and configstring
length bound (into `gameState.stringData`) **could not be confirmed statically**
with the available tooling. This is historically one of the two worst IW4 client
RCEs (OOB write of `gameState.stringOffsets[index]` / stringData overflow). It
must be treated as **unverified** and is the top follow-up item (see below).

### 15. `CL_ParseSnapshot` entity-number / areamask — **PARTLY OBSERVED / UNVERIFIED**
**October 31 update:** The exact-image follow-up in
`P2P_PUBLIC_RCE_AUDIT_20261028.md` confirmed a 1025th full-snapshot entity
write beyond the static array and three field-run bounds defects. Local
`snapshot_guard.c` now checks these four sites; its private-page x86 fixture
passes, but no updated DLL or hostile-packet C1 run has been verified.
The historical assessment below remains the October 26 baseline. Other
snapshot fields and areamask handling are still open.

The snapshot region (~0x0049D800, referencing the
`non-SNAPFLAG_BASELINE … Discarding packet` string 0xC48F70 at 0x0049D99D and
`CG_ReadNextSnapshot: way out of range` 0xC450BC at cgame 0x0046D9AE) contains
at least one bounded inlined byte read (row 13). The individual field bounds —
delta-entity / packet-entity number `< MAX_GENTITIES` (0x800) and areamask
length `<= sizeof(areamask)` — and the other cursor paths were **not** proven
instruction-by-instruction in this pass. The per-entity delta code path is
large and partly threads into high protected call targets. Treat snapshot
parsing as unverified beyond the one observed read.

### 16. Other connectionless packets — **REASONED / native**
`CL_ConnectionlessPacket` (the function containing the `"v"` handler at
0x00496482) still natively dispatches `infoResponse`, `statusResponse`,
`connectResponse`, `print`, `disconnect`, etc. Only `"v"` is re-implemented by
Kernel33. These native handlers were not exhaustively reversed. In IW4 these are
comparatively small and mostly route into infostring parsing; the infostring
parser's buffer discipline is the thing to confirm next for these.

### 17. BD / DemonWare lobby packets — **NOT AUDITED (largest residual)**
The matchmaking / party / room / relay layer in CODOL is the Tencent "BD" SDK
(strings like `BD_MAX_BUFFERED_CONNECTIONS`, `BD_MAX_SERVER_HOST_SIZE`,
`bdHTTPWrapper`, SCTP-style `sending init: too many retries`) plus the Python
lobby/auth. The alterIWnet-era RCEs lived precisely in the party-state / lobby
packet handlers (`partystate`, `memberjoin`, relay, XNADDR). This surface is
native + Python and was **not** covered by this client-binary pass — it is the
single biggest unknown for P2P and should be scoped as its own audit (and partly
overlaps the server worker's remit for the Python side).

### 18. Fastfile / zone / IWD parsing — **NOT AUDITED (native)**
Downloaded/usermap `.ff` and `.iwd` content is parsed by the native C1 zone
loader. Kernel33 only *routes* which file is opened (row 11) and validates stock
zone sizes/magics (`PureAcceptRebuiltZones`). The fastfile decompressor/loader
itself (a classic attacker-supplied-file RCE surface) is native and out of scope
for this static pass.

---

## Fixes / staging

**No code change was shipped in this pass.** Kernel33's own
remote/file-facing code (rows 1–11) was found to be consistently bounded, so
there was no confirmed Kernel33-side bug to fix. The genuinely dangerous
inherited surfaces (rows 14–18) are in native C1 / BD code and are either
VMProtect-opaque or not yet reversed; shipping a *speculative* memory hot-patch
into the live net path (e.g. a guessed configstring-index clamp) would risk
crashing the protected client and the running match, which the task forbids. A
hot-patch there must be derived from a confirmed native layout and
live-verified against a probe server before it is staged — that work is listed
as follow-up rather than guessed at here.

Consequently `D:\CoD\CODOL Builds\k33_secaudit_staging` holds no new DLL from
this pass. The user's install (`…\Kernel33.dll`) and the running client were not
touched.

### Recommended follow-up hot-patches (design, for a later verified pass)
1. **Configstring index/length clamp (row 14, top priority).** Resolve whether
   `CL_ParseGamestate` / the `svc_configstring` writer is virtualized. If a
   reachable non-virtualized clamp site exists, add a Kernel33 hook (existing
   trampoline style, cf. `managed_quick.c`) that enforces
   `index < MAX_CONFIGSTRINGS` and `offset+len <= stringData size` before the
   copy. If it is fully virtualized, the clamp has to move to the call boundary
   (wrap the gamestate-parse entry and pre-validate the message) — more
   involved, but the correct place.
2. **Snapshot field bounds (row 15).** Confirm and, if missing, clamp
   delta/packet entity numbers `< 0x800` and areamask length at the native read
   sites.
3. **Infostring parser (row 16).** Confirm the native `Info_ValueForKey` /
   connectionless `print` buffers are bounded; otherwise wrap them the way
   `voice.c` wraps `"v"`.
4. **BD lobby packets (row 17).** Separate, scoped audit — highest P2P priority
   after configstrings.

---

## Residual risk for P2P hosting

Turning any player into a host means the host-controlled channels — gamestate
(configstrings/baselines), snapshots, reliable commands, downloads, and the BD
lobby/party packets — all become attacker-controlled. From this pass:

- **Confirmed hardened:** the voice connectionless packet (Kernel33-owned,
  fully bounded); Kernel33's own Discord/managed-room/launcher/usermap/console
  code (no OOB, no remote→Cbuf injection, no attacker-controlled download URL).
- **Confirmed-present native guards:** the reliable-command 256-window and the
  message-level MSG read bounds — so trivial packet truncation / command-flood
  overflow is already blocked by the shipped binary.
- **Unresolved (the real P2P exposure):** configstring index/length
  (VMProtect-opaque — **assume vulnerable until proven otherwise**), snapshot
  per-field bounds, the native infostring/connectionless handlers, the BD
  lobby/party packet layer, and fastfile/IWD parsing of downloaded content.

**Recommendation:** do not ship open P2P hosting to untrusted hosts until at
least row 14 (configstrings) and row 17 (BD lobby packets) are resolved — those
are the two that match the publicly-known IW4-family client RCEs most directly.
Until then, constrain hosting to trusted/authenticated hosts via the lobby.
