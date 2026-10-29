# P2P public RCE review and host-filter repair

This is a bounded audit of public vulnerability disclosures and the selected
CoDOL implementation. It is **not clearance for arbitrary player-hosted matches**.
No exploitable C1 RCE was demonstrated or ruled out. No production component was
restarted, replaced, or retired. No exploit payload was sent to a game client.

## Reviewed inputs

- Client patch: `CODOL-EMU/codomp_clientShipRetail-patch` main
  `2e08b66ea8efdf1ff95114fdffc58a0dc211a0a3`. Its main history contains the P2P
  supervisor, in-process experiments and newer localhost lobby integration.
  `dev@dc259f53` was located first, then the audit advanced to main.
- Native server: `nucleur16/codol_1.1.1.13_alpha_server` main
  `70ac47806f23a8b05c90bdb3e5104e2cd7ab7362`.
- New backend integration baseline: private `zzVertigo/codol-classic`
  `8ec07788589e0f3be8ddb73de6410550d6793d6f`.
- Additional GitHub inventories: CODOL-EMU lobby, rawfiles, OLfinal client/server,
  private Classic application and original dedicated repository. OLfinal targets
  a different build and is not evidence of 1.1.1.13 safety.
- Read-only C1 reference on this machine: timestamp `0x5314def0`, SHA-256
  `d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60`.
  At `0x496482` it has the voice patch's expected instructions. This dump's hash
  is recorded independently; a timestamp or matching hook does not establish
  equality with every distributed executable. The embedded `unpacked.z` is only
  the high-address loader range, not a complete replacement for the C1 text.

## Public disclosures and applicability

| Public issue | Evidence in this implementation | Assessment |
| --- | --- | --- |
| [CVE-2018-20817](https://nvd.nist.gov/vuln/detail/CVE-2018-20817), unchecked Steam authentication blob | No `SV_SteamAuthClient` or `authBlob` implementation in the reviewed native server source/assembly. CoDOL bypasses the legacy reliable/SApi envelope in `src/sv_client.c` and rejects its sequence in `src/sv_main.c`. | The reviewed native route does not expose this Steam-auth path. Absence of a C1 string does not prove absence of an equivalent binary parser. |
| [CVE-2018-10718](https://nvd.nist.gov/vuln/detail/CVE-2018-10718), MW2 crafted-packet stack overflow; [public JoinParty disclosure](https://github.com/BlastsMods/JoinPartyRCE) | Native server has no `PartyHost_HandleJoinPartyRequest` implementation. C1 contains `joinParty` strings and a separate native BD/party layer. | The MW2 build-specific exploit is not proof of a CoDOL vulnerability. C1 party/BD applicability remains unresolved; no cross-build address patch is justified. |
| [IW4x public security fixes](https://github.com/iw4x/iw4x-client/blob/main/src/Components/Modules/Security.cpp), Huffman expansion, deferred packet copy, relay and UI directive boundaries | CoDOL native receive path copies after a size check instead of using Huffman. C1's gamestate, snapshot and BD paths are distinct native code. | Honest-server receive bounds are present. Client-side equivalents still need exact-build analysis and hostile-input regression coverage. |
| [IW4x disclosure and later correction](https://forum.alterware.dev/t/important-security-notice-remote-execution-vulnerabilities-found-in-iw4x/86), oversized localized OOB text | The June 2024 correction identifies unchecked copying in localization; the earlier `va` explanation is not reliable. C1's connectionless dispatcher is native and includes protected call targets. | No verified CoDOL instance. A source-IP allowlist does not validate an allowed host's text or other payloads. |
| IW4x `openLink` / HTTP helper attack chains, documented in the same maintainer disclosure | Kernel33's known local commands, supervisor configuration and launcher bridge do not add that IW4x script API. Native downloadable zones/IWDs remain a separate parser surface. | Do not import IW4x-specific conclusions into C1. Untrusted content loading remains unqualified. |

The old `SECURITY_AUDIT_CLIENT_20261026.md` is evidence of an earlier pass, not
a security certificate. Its unproven native rows 14–18 remain open. Likewise the
old server audit's broad shared-IP admission finding is stale: current
`CODOL_AdmissionMember` requires the exact UID/member or exact UID/IP admission.

## Confirmed host-filter defects repaired here

These are security-boundary defects and an availability weakness, **not confirmed
RCEs**:

1. Shipping accepted `+set k33_hostGate 1`, forwarding denied hosts in log-only
   mode despite the stated shipping trust requirement. Only explicit local-test
   builds can now use this mode.
2. Missing or mismatched import slots caused `InstallHostGate` to return while
   C1 startup continued. It now returns a checked success result; the caller
   stops startup on failure. The resolver import is validated too, and all
   three installed pointers are read back before success.
3. A continuously readable queue of denied datagrams could keep one receive
   call draining indefinitely. Each call now yields after 64 rejected packets.

The filter still trusts configured lobby addresses, loopback, SSDP/default
gateways and any explicitly vouched address. It operates on IPv4 addresses,
not authenticated packet contents. It is not a firewall against source spoofing,
a compromised router/lobby, a malicious approved host, or native parsing bugs.
No remote host-ticket path is enabled by this change.

## Verification

`tests/hostgate_windows_fixture.c` includes the actual filter/installer and runs
in a disposable 32-bit Windows process with a private surrogate import table.
It never loads a game. Ten isolated scenarios pass in each build profile:
unmapped/missing imports; substituted receive/send/resolver imports; each of the
three failed writes; successful installation despite the diagnostic switch;
and packet allow/drop, peek consumption and bounded-drain behavior. Queue
readability uses an actual loopback UDP socket; packet source metadata is a
test double. These are **20 profile/scenario runs**, not real C1 acceptance.

Compile each profile with MinGW GCC13:

```
i686-w64-mingw32-gcc -std=c99 -O2 -Wall -Wextra -Werror -Wno-cast-function-type -DCODOL_LOCAL_TESTING=0 tests/hostgate_windows_fixture.c -o hostgate-0.exe -lws2_32 -liphlpapi
python tests/test_hostgate_windows.py hostgate-0.exe
```

Repeat with `CODOL_LOCAL_TESTING=1`. The normal Windows test runner includes
both profiles. The native reviewer separately compiled and ran eight exact-main
source fixtures: handshake guard, vote guard, info strings, data fast path,
peer pool, session reservations, connect parsing and gamestate validation.
All passed; some deliberately use network/crypto doubles. Server gamestate
validation checks its own output, which a modified host can bypass.

No full client DLL or native game runtime is qualified by these fixtures.
Real C1 startup, normal lobby login and trusted-server joining remain required
before distributing these client bytes. No binary deployment is part of this PR.

## Gate before public P2P or dedicated-server retirement

- Resolve or independently validate C1 gamestate/configstring, snapshot,
  connectionless/localization, BD/party/relay and downloadable-content paths.
- Authenticate and expire host authorizations by session, joiner, endpoint and
  exact build. A signed host ticket is an authorization, not a proof of safety.
- Replace/test the native fixed ECDH scalar in coordination with actual C1;
  it is not a newly demonstrated RCE and cannot safely be changed by guesswork.
- Qualify remote clients, NAT/relay, departure/recovery and rollback. Keep host
  match reports out of authoritative careers until the trust model is settled.
- Later script work must use signed releases, fresh authoritative match-start
  version checks and authenticated encryption, with no loose-file fallback.
  Encryption/hashes cannot make a player-controlled process tamperproof.

Backend/script prototypes may run in isolation after this audit checkpoint, but
must not enable unqualified remote P2P or retire production capacity. Coupled
client/backend/native rollouts require their ordinary release qualification and
the announced maintenance/continuous-empty checks.
