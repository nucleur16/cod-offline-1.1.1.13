# sh1x server security audit, 2026-10-26

**Scope:** the CoDOL match server (`bin/sh1x_dedrun.exe`). This covers the C sources in `src/`, the retained IW3 assembly in `src/asmsource/`, `src/online/<area>/`, and the GSC that responds to client input. The C1 client and Kernel33 were audited separately.

**Goal:** find any RCE, memory-corruption, authentication or denial-of-service weakness inherited from IW3/IW4 or added in the CoDOL port, and fix every confirmed one.

**Branches:**

| Repository | Branch | Commits |
|---|---|---|
| Server, off origin/main | `sec/server-audit-20261026` | 08feb5d0, 5e4426bf |
| Server, off `deploy/late-corpse-powerdoor` (base of the live exe) | `sec/server-audit-20261026-deploy` | 378e5027, c2485794, plus a follow-up commit |
| Rawfiles | `sec/server-audit-20261026` | 2cdc49c |
| Rawfiles | `sec/server-audit-20261026-deploy` | 3f91e86 + the same fix |

**Live-tree GSC patch:** `docs/SECURITY_AUDIT_SERVER_20261026_rawfiles.patch`. It applies cleanly to the live rawfiles at 3f91e86.

## Entry points

- **UDP from non-loopback sources** goes through the bdDTLS layer (`src/online/net/network.c`). Without an association, a sender can reach only:
  - the QoS reply;
  - the NAT intro echo;
  - INIT;
  - COOKIE_ECHO.

  Everything else needs an established association: OOB commands, the netchan, usercmds, reliable commands, stats and voice.
- **Loopback OOB** is plaintext; this is the lobby's rcon channel.
- **TCP on the game port** serves Source RCON (`src/hl2rcon.c`). It sits outside the bdDTLS gate.
- **Connected clients** reach the following through the netchan:
  - `SV_ExecuteClientMessage`
  - `SV_ClientCommand` / `ClientCommand`
  - `SV_UserMove`
  - menu responses and the notify commands into GSC

## Findings and status

Severity is the worst case. "Fixed" means fixed on both branches.

| # | Area (file) | Severity | Reachable by | Status |
|---|---|---|---|---|
| 1 | Vote command builds a console command from client arguments (`src/g_sv_cmds.c` `Cmd_CallVote_f`; run by `CheckVote`) | Critical: console command execution | Any connected team player | Fixed: strict argument validation (`vote_guard.c`) |
| 2 | Vote counting: IW3's "already voted" flag is C1's vehicle flag, so it was never set (`src/g_cmds.c` `Cmd_Vote_f`) | High: lets one client pass a vote alone | Any connected client | Fixed: per-client record per vote |
| 3 | `va()` ended the level (ERR_DROP) on oversize output; client text reaches it, e.g. the unknown-command echo (`src/qshared.c`, `src/g_cmds.c`) | High: DoS, whole match | Any connected client | Fixed: truncate and warn under CODOL; echo capped at 64 bytes |
| 4 | One spoofed INIT replaced an established association and dropped its player (`network.c`) | High: DoS | Unauthenticated, with a spoofed source | Fixed: a new INIT is staged until its COOKIE_ECHO completes |
| 5 | Pending-handshake slots could all be held by one host; a retransmitted INIT kept a slot alive (`network.c`) | High: DoS, blocks new players | Unauthenticated | Fixed: at most 4 pending per IP and 48 in total; a retransmit does not refresh the slot |
| 6 | Unauthenticated replies and per-packet roster reads were unthrottled (NAT echo, INIT, COOKIE_ECHO) | Medium: reflection, hitching | Unauthenticated | Fixed: 8 per second per address, 200 per second in total |
| 7 | "endround" menu response called `forceEnd()` for any client (`maps/mp/gametypes/_menus.gsc`) | High: integrity / DoS | Any connected client | Fixed: host only, as in IW4x |
| 8 | Admission row without a latched uid accepted any claimed xuid (`src/sv_client.c` `SV_DirectConnect`), so a client could bind another account's profile and progress | High: account impersonation | Admitted client | Fixed while admission is required: refused |
| 9 | Shared-IP fallback in `AdmissionMember` (`admission.c`) | Medium: account impersonation behind one public IP | Admitted client on the same public IP | **Open.** Needs the lobby to always publish member rows; then remove the fallback |
| 10 | Source RCON over TCP had no exposure restriction | Medium: remote password guessing | Unauthenticated (TCP) | Fixed: loopback only unless `sourceRconRemote 1` |
| 11 | Source RCON command path returned with the console redirect still active, leaving it pointed at a dead stack buffer (`src/hl2rcon.c`) | Memory corruption | Authenticated power-limited admin | Fixed |
| 12 | Source RCON logged the attempted password on failure | Info leak (logs) | Unauthenticated (TCP) | Fixed |
| 13 | UDP rcon password attempts were unlimited (only the deny reply was rate limited) (`src/sv_main.c`) | Medium: brute force | Any associated peer | Fixed: lockout after 5 failures per minute per address; loopback exempt |
| 14 | `Info_SetValueForKey` / `BigInfo_SetValueForKey` wrote a one-byte NUL past the buffer; `Info_RemoveKey` copied unbounded (`src/qshared.c`) | Memory corruption (1 byte) | Connect/userinfo paths | Fixed |
| 15 | Tokenizer wrote one slot past `cmd_argv`; `Cmd_Argv` accepted negative indices (`src/cmd.c`) | Memory corruption (latent) | Long command lines | Fixed |
| 16 | `GScr_LogPrint` passed script text as the format string (`_g_scr_main_mp.asm`); GSC passes player names | Format string (latent) | Player names | Fixed: `"%s"` |
| 17 | Chat logged with control characters (`G_Say`) | Low: log spoofing | Any connected client | Fixed |
| 18 | In-game admin output redirect read past the end of its string (`src/sv_ingameadmin.c`) | Info leak (stack bytes) | Any connected client (power-1 commands) | Fixed |
| 19 | CoD4x update-proxy relay and replies were served with no sender or length check (`src/sv_main.c`) | Medium: relay / DoS | Associated peers | Fixed: not served under CODOL |
| 20 | Dead host-migration receiver with unchecked offsets behind an early `return` | Latent heap corruption | None today | Fixed: removed |
| 21 | Usercmd offhand index validated on its low byte only (`src/sv_client.c`) | Low: logic | Any connected client | Fixed: full 10-bit index |
| 22 | Flood exemption matched the "mr"/"score" prefixes | Low: DoS amplification | Any connected client | Fixed: whole verbs only |
| 23 | `StrColorStrip` / `StrCtrlStrip` / `ToUpper` wrote into fixed buffers unbounded (`src/scr_vm_functions.c`) | Memory corruption | Map scripts only | Fixed |
| 24 | Elevator floor selector took the client's response text as an index (`common_scripts/_elevator.gsc`) | Low | Clients on maps with elevators | Fixed: clamped |
| 25 | ECDH server key is a fixed public scalar (C1 interop choice) | Design: no forward secrecy | Passive observer | **Open**, documented in the code; needs C1 testing of a random key |
| 26 | `bin/home/codol_sessions.txt` and the progress files are tracked in git | Info leak | Repository readers | **Open.** Untracking them would delete live runtime files on pull; move them outside the repo first |
| 27 | `G_MaterialIndex` strcpy (asm); scr_vm_fs path checks; `StringTable_Lookup` negative column; `Scr_EvalPlus` error path; `GScr_MakeCvarServerInfo` | Low | Map scripts only (trusted) | **Open**, script-only |
| 28 | `menuClass` trusts the class name | Low: self-DoS | The sending client only | **Open**, GSC hardening |

## Reproductions

These ran locally against isolated probe servers with their own homes and state directories. Nothing touched 17733.
- 17781 ran the patched build.
- 17782 ran a copy of the live exe with the live rawfiles.

| Check | Live exe (17782) | Patched (17781) |
|---|---|---|
| Loopback rcon | works | works |
| Source RCON from 127.0.0.1 | answered | answered |
| Source RCON from the LAN address | answered | refused |
| NAT echo replies to 50 requests from one address | 50 | 8 |
| INIT_ACKs for INITs from 10 ports of one IP | 10 | 4 |

The vote, info-string and handshake-staging behaviour are covered by unit tests rather than live probes.

## Reviewed and found safe

**Netchan**
- CoDOL fragment reassembly (`wire.c`, `ol_netchan.c`): index, stride, length and capacity are checked, and the reassembled size is bounded by `msg->maxsize`.
- The legacy CoD4x path keeps its fragment-length checks.
- Huffman decoding is not used by CoDOL.

**MSG_\* readers** (`src/msg.c`)
- Every read is bounded by `cursize`.
- `MSG_ReadString` / `MSG_ReadStringLine` are bounded and translate `%`.

**Out-of-band handlers**
- `getstatus` / `getinfo`: the challenge is capped at 128 bytes and replies are rate limited.
- The connect queue is bounded.
- `connect` userinfo is validated in `connect.c`: the xuid must be 16 hex digits, and control characters, `"` and `;` are rejected.
- `stats` needs an association; chunk index and size are exact.
- `heartbeat` needs an association.

**bdDTLS**
- DATA open checks the size and the HMAC before decrypting, and has a replay window.
- ECDH import is bounded and validated.
- INIT and COOKIE_ECHO have fixed lengths and the cookie is compared exactly.

**Client messages**
- The message body is checked against its buffer.
- `ReadClientCommand` is bounded and transactional.
- Sequence and lost-command checks are intact.

**Usercmds**
- The command count is bounded by the array.
- Weapon IDs are 10 bits against a 1024-entry map, then `BG_IsWeaponValid`.
- The asm readers re-check `bg_lastParsedWeaponIndex`; the known CoD4 weapon-index crash is closed.
- Floats are checked with `isfinite`.

**Client commands**
- `mr`: index 0..31, `serverId` checked.
- `n`: the action id is range checked.
- `say` is cut to 150 characters; `tell` is disabled.
- Cheat commands are gated by `CheatsOk`.
- `follow` and `mute` are range checked.
- `jr` is rate limited and its reply carries only a number.
- `matchdatadone` takes no argument.
- `cp` is bounded in `pure.c`.

**Disabled under CODOL**
- The CoD4x reliable-message protocol, so downloads, SApi/Steam auth, XAC and the IWD header parser are not reachable.

**In-game admin**
- `SV_ExecuteRemoteCmd` strips separators, checks power, and runs a single command.
- Commands that ordinary players (power 1) can run only report information.

**Files**
- Progress, `mpdata_s`, owned items and match reports are all named from a numeric uid, so there is no path traversal.
- The match report JSON is escaped.

**Asm arrays**
- Retained per-entity arrays are sized for 2048 entities.
- `CM_AreaEntities` callers cap their result counts.

**Voice**
- The CoDOL Opus relay bounds its frames and output and is rate limited.
- The talker is identified from the authenticated address.

## Tests

New regression tests (all in `makefile` under `test`):
- `test_vote_guard`: vote argument validation and one vote per client.
- `test_info_string`: the infostring length boundary and oversize keys.
- `test_handshake_guard`:
  - staging of a new INIT against a live association;
  - the per-IP and total pending caps;
  - retransmit expiry;
  - the NAT echo budget.

On both branches, the related existing tests pass: data_fast_path, peer_pool, session_reservation, pure, admission and connect.

## Operational notes

- The live server has `g_allowvote 0`, set as a stop-gap before this fix.
- Remote Source RCON is now off by default. Set `sourceRconRemote 1` to re-enable it.
- A client whose lobby seat has no latched uid is refused with `EXE_ERR_NOT_AUTHORIZED` while admission is required. The lobby republishes the seat once the uid is known.
