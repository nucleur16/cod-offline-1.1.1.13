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

## Developer console (branch `dev`)
`dev` is `parity-log` plus `console.c`, an in-game console for C1, which has none left: IW4's
`toggleconsole`/`clear`/`condump`, the console field and `Con_DrawConsole` were removed, and
`CL_KeyEvent` (0x493240) no longer tests the `~` key. The console is an overlay instead:

- **Look (IW4/IW4x Con_DrawInput)**: `~` opens one thin input line across the top of the screen
  (con_inputBoxColor), prompt `SH1X >` in the version colour, the typed text and a cursor blinking every
  256 ms; no panel or title. While text is typed, IW4's hint box hangs under it (con_inputHintBoxColor):
  each match with the typed part highlighted and its current value, IW4's value/default/domain rows for
  an exact dvar, and "N matches (too many to show here, press shift+tab to view full list)" past 24.
  Under that, only what the last command printed. Shift+Tab or Shift+`~` opens the full-height log
  (con_outputWindowColor, IW4's scroll bar, the version string bottom right). Engine and PARITY lines
  appear only there.
- `k33shot [ms]` writes the console overlay as rendered, that many ms later, to `main\k33_console.bmp`.
- **Toggle**: the key left of 1 (`~` on US layouts, by scan code) or the virtual key in the archived
  dvar `k33_consoleKey` (default 112 = F1, 0 = none). Works at the Scaleform menus and in game. Esc
  closes it (first Esc only closes the hint box).
- **Input**: the window class's WndProc (0x62FB85) goes through `ConsoleWndProc`, which owns the
  keyboard while the console is open; key-ups and everything else still reach the game. IME results
  are inserted as GBK; with the console closed the GBK IME patch is unchanged.
- **Output**: a pre-hook on the four calls of `Com_LogPrintMessage` (0x594010) copies everything C1
  logs into a 4096-line scrollback. Quoted numeric dvar names get their readable name appended
  (`"591" (cg_drawFPS)`).
- **Drawing**: `Direct3DCreate9`'s import slot is wrapped; the device's `Present` draws the panel
  (GDI into a DIB, uploaded to a managed texture, one quad inside a `D3DSBT_ALL` state block).
- **Execute**: lines go to `Cbuf_AddText` (0x58BA60). Readable dvar names are rewritten to C1's
  numeric ones (`cg_drawFPS 1` -> `591 1`, also after `set/seta/reset/toggle/togglep/setfromdvar`);
  the rewrite is echoed as `-> ...`.
- **Autocomplete**: as you type, the hint box lists matching dvars (with their current value, C1
  name and domain) and commands, prefix matches first, then substring matches, the typed part
  highlighted; numeric IDs match too. Over 24 matches it shows the count. One exact dvar shows its
  value, default, type/flags, domain and IW4 description, as IW4's Con_DrawAutoCompleteChoice does.
  Tab completes the longest common prefix, then cycles the matches.
- **Sets** go through C1's own command path (Cbuf -> Dvar_Command -> Dvar_SetCommand ->
  Dvar_SetVariant), which validates type, domain and flags; the console never writes dvar memory.
  A readable name whose mapping is a positional guess prints a warning when used.
- **cg_draw2D at the menu**: setting `cg_draw2D` ("585") to 1 at the front end crashed C1. The name
  and the set were right; C1's CG_Draw2D just cannot run without a snapshot (it reads cg.nextSnap at
  0x43A205, and 0x453CCD after that), which is why the front end keeps it 0. The console now refuses
  to change it (set/reset/toggle or `cg_draw2D <v>`) until cgame has a snapshot, i.e. in a match.
- **Keys**: Up/Down history (saved to `main\k33_console_history.txt`), PgUp/PgDn/wheel scroll the full log,
  Ctrl+Home/End top/bottom, Ctrl+V paste, Ctrl+U/K cut, Ctrl+Backspace word, Ctrl+L clear.
- **Built-ins**: `dvarlist [filter]` and `cmdlist [filter]` with readable names, `find <text>`,
  `clear`, `condump [file]` (to `main\`), `k33help`.
- `+set k33_console 0` on the command line leaves the console out entirely.

### Dvar name table (`c1_dvars.h`, `gen_dvars.py`)
C1 registers most dvars under numbers ("591" = cg_drawFPS, "1848" = com_maxfps). `gen_dvars.py`
decodes every `Dvar_Register*` call in C1.exe and in `iw4x.exe` (stack simulation), groups them by
registration function (IW4 names from `iw4x.pdb` via cdb), aligns each C1 function with its IW4
counterpart by the longest common subsequence of type/default/domain, and merges the names with
alpha0022's `docs/dvars_c1exe.md` and hand-checked notes (doc and notes win). The header maps
1284 of the 1593 numeric dvars registered in `.text` to a name, type, default, domain and IW4
description, with a confidence; below 85 the name is a positional guess and the console says so.
Numbers without a name display as they are. Command names are not hashed in this build (all
`Cmd_AddCommand` names are plaintext), so commands are listed as registered.

Build without touching the live DLL: `sh build-dev.sh` writes `Kernel33_dev.dll`.

### Sound voice diagnostics (`sounddiag.c`, branch `dev`)
For sounds that sometimes do not play (gunshots on ported maps): the three play paths' channel-cap checks
(calls of 0x736C10 at 0x73E1FB 2D / 0x73E4DB 3D / 0x73EDFE stream) and the three voice allocators (calls of
0x737AC0 at 0x73E20E, 0x737B30 at 0x73E4EE, 0x737BB0 at 0x737F77) go through stubs that call the original
and log when it refuses. A background thread appends to `Kernel33.log` once a second:
```
snd refuse 3d alias=weap_ak47_fire_plr ch=weapon(2) t=522200875 why=pool 3d=32/32
snd refuse 3d alias=weap_ak47_fire_plr ch=weapon(2) t=522200875 why=pool 3d=32/32 repeated 49 more times in 1 s
snd voices t=522201875 2d=0/8 3d=32/32 stream=1/13 refusals=54
snd   v08 3d ch=ambient(5) alias=amb_abandon_wind_lp ent=1008 st=0
snd chans ambient=32/64(p5) music=1/1(p5)
```
`why=cap` = the channel was at its cap (active/cap shown), `why=pool` = no free voice and none of a lower
channel priority to steal. `t` is GetTickCount ms. Snapshots (`snd voices` + one line per busy voice +
`snd chans`) are written each second while a refusal happened in the last 3 s.
Toggle: dvar `k33_snddiag` 0 off / 1 (default) / 2 also a snapshot every 5 s; startup value from
`+set k33_snddiag N`, else `CODOL_SNDDIAG`, else 1; 0 at startup installs no hooks.
