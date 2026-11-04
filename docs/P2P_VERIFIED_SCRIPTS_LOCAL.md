# Local verified script handoff

This local source change connects a trusted Python service SDK to the in-process
host's immutable script provider. It is not yet real C1/VM acceptance, a complete
local game package, or a production release. No installed client is modified.

The local runner starts and owns C1, retains its authenticated process capability,
and creates a private, random, local named pipe. It must verify the connecting
pipe client PID equals that exact C1 process. Before C1's hosted server is loaded,
the client asks that runner to prepare the current selection. The runner must
derive membership/host/selection from authenticated DW state, fetch the current
release through verified TLS with a pinned publisher key, verify signature,
authenticated encryption and every file hash, complete each member's fresh
verification, and obtain the host's Ready approval. Only then does it reply with
the complete source blob. Refusal uses a zero-length response.

The client validates the pipe name and server PID, bounds all I/O under one
30-second deadline, fetches into transient memory, then loads the host DLL.
`Sh1xHost_UseVerifiedScripts(const void *, unsigned)` must clone the entire source
inventory before returning 1. C1 erases/frees its temporary pipe buffer before
allowing `Sh1xHost_Init` or a server frame. The provider's selected mode refuses
missing/invalid scripts without consulting loose files. A host DLL without the
export is refused. Selected service mode neither requires a plaintext GSC tree
nor supports `mode=child`; it cannot silently substitute that path.

## Development opt-in

The runner sets these in the owned child's environment, never on the command line:

```text
CODOL_VERIFIED_SCRIPTS_REQUIRED=1
CODOL_VERIFIED_SCRIPT_PIPE=\\.\pipe\CoDOLVerifiedScripts-<32 lowercase hex digits>
CODOL_VERIFIED_SCRIPT_PARENT_PID=<runner process ID>
```

Only `CODOL_LOCAL_TESTING=1` builds may use this bridge. P2P hosting requires
`CODOL_VERIFIED_SCRIPTS_REQUIRED=1`, `mode=inprocess`, and
`admission_required=1`. Missing/0 or malformed flags refuse hosting; shipping
builds also refuse the local bridge. There is no loose-GSC fallback. A missing or
unusable selected bridge fails before the host DLL is
loaded, with a short non-sensitive status message. The pipe uses identification
security quality of service and cannot name a remote machine. The SDK must use
an appropriate pipe ACL and reject remote clients. This IPC protocol does not
independently authenticate release signatures or establish remote attestation;
the trusted SDK and the bound child form its local trust boundary.

The separate local multi-client guard accepts only an exact five-line
`dwauth.ini` whose lobby and auth hosts are both `127.0.0.1`, or both
`127.0.0.3`, with the same valid port. The second literal supports the isolated
local test backend when another process owns the normal route. Mixed hosts,
other loopback addresses, hostnames, suffixes, shorthand and wildcards do not
qualify. The explicit command-line opt-in and local build guard remain required;
shipping retains the normal single-client path.

## Wire format

The request is LE32 byte length followed by ASCII JSON:

```json
{"operation":"prepare","map":"mp_killhouse_sh","gametype":"war","type":"mp"}
```

`map` is the current session map, falling back to the base map only when the
session-map field is absent. Tokens admit only lowercase ASCII letters, digits
and underscore, with maxima 63/31/15 bytes. No arbitrary command/path input exists.

The reply is LE32 byte length followed by the CVSP source blob:

```text
"CVSPv1\0\0"[8], LE32 file count, LE32 total source bytes, manifest SHA256[32]
repeat count: LE32 path length, LE32 source length, ASCII path, source bytes
```

The transfer ceiling is 96 MiB (the service's outer encrypted-payload ceiling).
The native provider independently applies the actual 8192-file, 4 MiB-per-file,
64 MiB-total source limits, canonical-path/inventory checks and exact packet
length. The client and native provider copy in their own heaps; memory never
crosses CRT ownership. The native source copy lives for that host DLL instance.
No script plaintext is written by this handoff, but ordinary process memory may
be paged and can be inspected/modified by the machine's owner.

## Focused verification

`tests/verified_scripts_pipe_fixture.c` exercises the actual Win32 local named
pipe consumer, including fragmented I/O and a cloning sink ABI, in a disposable
x86 process. It does not load C1. Build with MinGW and run on Windows:

```text
i686-w64-mingw32-gcc -std=c99 -O2 -Wall -Wextra -Werror -Wno-cast-function-type -DCODOL_LOCAL_TESTING=1 tests/verified_scripts_pipe_fixture.c -o pipe-local.exe
python tests/test_verified_scripts_pipe.py pipe-local.exe
```

Repeat with `CODOL_LOCAL_TESTING=0` and `shipping` as the Python runner's final
argument. The fixture uses a 250 ms deadline to prove timeout cancellation and
cleanup. The real client retains the 30-second limit. Server authorization,
cryptographic verification and VM compilation are qualified separately.

The November 1 policy repair and lobby mode correction were rebuilt as PE32
client DLL SHA256
`cca78352548d7ce210d1ea755f25468b0d73097ba4db377a0cb822b2580fc90d`.
The local and shipping fixture translation units compiled to PE/i386 objects,
SHA256 `1a848e912f0c874b74899e8ee3b00ddd3a9f2415f198a63cc94d2b68a76c44f1`
and `f0680ce101fe25973660707e828ad612f689b3e941865fc75c969d48ad61b6bb`.
Those objects were not executed. This DLL has not been staged into the private
Host/Guest clients or exercised in C1; the last staged client remains the older
candidate recorded in the overnight checkpoint.

The lobby Host path and its pending-restart path now select `inprocess`; they
previously forced `child`, which the verified script install cannot support.
This is a compiled source correction, not a demonstrated C1 room/match run.

The local host policy permits an empty `extra` field or the trial's exact
`+set codol_exitToLobby 1 +set codol_oneShotMatch 1` suffix. Other suffixes
are refused, so this configuration field cannot undo the required admission
setting after validation. This does not protect against a player who modifies
their own client or native host binary.
