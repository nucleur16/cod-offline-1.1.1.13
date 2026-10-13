# Kernel33.dll — codoMP_client_shipRetail.exe (CoDOL 1.1.1.13) patch

Runs the packed 1.1.1.13 client the way C1.exe runs, without TenProtect executing, and applies
client fixes (render gate, dvar defaults, plaintext config, Alt+Tab, raw mouse input, scoreboard,
patch-folder zones). Each patch checks the original bytes first and logs to `Kernel33.log` next
to the exe. See the comments in `kernel33.c` for addresses and reasoning.

## Install
1. `python make_exe.py` renames the packed exe's `Kernel32.dll` import to `Kernel33.dll` (needs C1.exe
   and `codoMP_client_shipRetail.exe.bak`; also writes `unpacked.z`).
2. Put `Kernel33.dll` next to `codoMP_client_shipRetail.exe` (TenProxy.dll + TerSafe.dll must still be there).

## Build (MSYS2 mingw32)
```sh
PATH=/c/msys64/mingw32/bin:$PATH sh build.sh
```
`unpacked.z` (C1.exe's decompressed code range) is embedded as a resource; `build.sh` copies the
result to `../Kernel33.dll`.

## Parity logger (branch `parity-log`)
`parity.c` adds a client-side snapshot-parity logger for bringing the server to retail parity. It
writes to the game's own console log (`main\console_mp-00N.log`) through C1's Com_Printf, one line
per finding:
```
^3PARITY t=<cg.time> st=<cg.snap serverTime> msg=<its messageNum> <category>: <details>
```
(`t/st/msg` are -1 outside a match). On load it logs `parity: logger active, N hooks installed,
M skipped` to `Kernel33.log`, and the console log gets `PARITY logger active: N hooks installed,
M skipped (parity_log L)` at the first menu or game frame.

Categories: `event-replay` (a playerState event slot changed under the same sequence, so C1 fires
it again: the double-shot/double-footstep class), `events` (slot overflow, unknown event),
`event-unknown`, `pred` (prediction miss: origin, velocity or a named playerState field, client
value vs. the value rebuilt from the snapshot at the same commandTime), `snap-entity` / `snap-client`
/ `snap-ps` (entity numbers, order, eType, weapon index, world-gun / ground / linked-weapon entities
missing from the snapshot, sentinel values), `asset` (a name no zone has, so the default is used,
with the configstring index that carries it), `menu`, `localvar`.

Each key prints 5 times (50 at level 2), then every 100th with `[xN]`, and each category prints at
most 30 lines a minute (300 at level 2), noting how many it held back; assets, menus and localVars
print once per name. Assets referenced only from inside a zone (renderer shaders, frontend images)
and localVars the HUD reads before a script menu sets them are level-2 only.

Toggle: dvar `parity_log` 0 off / 1 normal (default) / 2 verbose (lower prediction thresholds,
higher limits), registered with the archive flag so the console can change it. Startup value:
`+set parity_log N` on the command line, else the environment variable `CODOL_PARITY_LOG`, else 1.
A startup value of 0 installs no hooks at all.

Build without touching the live DLL: `PATH=/c/msys64/mingw32/bin:$PATH sh build-parity.sh` writes
`Kernel33_parity.dll`; copy it over `../Kernel33.dll` (keep a backup) to use it.

### Several clients on one PC (branch `parity-log`)
The branch also skips WinMain's "is already running." check (0x630C3A; the game's mutex
`Global\{5BD75B76-...}` still exists, it is just ignored), so a second client can watch another
real player's movement. Start every copy through `codoMP_client_shipRetail.exe` (`C1.exe` never
loads this DLL) with its own `dw_username`, and give each copy after the first its own console
log through dvar `1436`, the log-name prefix C1 already reads before opening `console_mp.log`:

    codoMP_client_shipRetail.exe +set dw_username devplayer
    codoMP_client_shipRetail.exe +set dw_username devplayer2 +set 1436 p2   (main\p2_console_mp.log)

Without `1436` the second copy takes over `console_mp.log` and the two logs mix. `config_mp.cfg`
and `Kernel33.log` stay shared (Kernel33.log lines carry each copy's pid at attach).
