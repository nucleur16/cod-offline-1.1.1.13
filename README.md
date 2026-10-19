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

## Voice chat (branch `feat/voicechat`)
`voice.c` puts back in-game voice chat, which C1 stripped (no capture, no codec, no `+talk`; the "v"
packet branch only printed "voice communication is disabled"). It uses Opus (libopus, linked
statically from MSYS2 `mingw-w64-i686-opus`) and the CoD4x server's relay
(`src/codol/net/codol_voice.c` on its `feat/voicechat` branch).

- **Push-to-talk**: `+talk`, a real C1 key binding like `+melee`. C1 stores binds as indices into its
  bind table (0xCD62C0, 86 names, no `+talk`), so voice.c extends the table with `+talk`/`-talk`, lets
  the key-up path pair them like the other +/- commands, and runs them (and `toggletalk`, an empty case
  in C1) in the binding executor 0x491580. Rebind it in **Options > Controls > Chat** (C1's own
  OptionView_control_chat rows: chatmodeteam, toggletalk, +talk) or with `bind <key> +talk`; it is
  saved in `players\config_mp.cfg` with the other binds. Default **V**, applied only when no key is
  bound to `+talk` and V is unbound. C1's key routing keeps it from firing in menus or the chat line.
  `toggletalk` latches transmit on/off. In push-to-talk mode the microphone stream runs only while
  transmitting.
- **Voice activation** (`k33_voiceMode 1`, Options > Audio > Voice Mode): open mic. While in a match
  and with the game window in the foreground (scoreboard, chat line and console do not stop it; alt-tab,
  the lobby and the main menu do), the mic stays open and a level gate decides what is sent: RMS level
  of each 20 ms frame on a 0..100 scale (-60..0 dBFS), opens after two frames at or above
  `k33_voiceThreshold` (or at once 15 above it), holds for `k33_voiceHangMs` after the level drops
  below threshold-8, and sends the 120 ms before it opened first (pre-roll) so word starts and endings
  are not clipped. Opus DTX stays on. `+talk` still transmits regardless of level. The HUD `talking`
  icon follows what is actually sent (key or open gate).
- **Options > Audio**: C1's audio panel (`OptionView_audio`) has a Voice section its onInit hides:
  a voice-chat volume slider, a microphone checkbox + slider, an input-level indicator, and an unused
  "Sound Presets" combo box. `voice_ui/OptionView_audio.as` is that class with the section shown and
  rewired, one control per row: **Voice Mode** (the presets box: Push to talk / Voice activation ->
  `k33_voiceMode`), **Voice** (chat volume -> `k33_voiceVolume`), **Microphone** (checkbox = mic on,
  `k33_voiceMicMute`; slider = activation level, `k33_voiceThreshold`), **Input Level** (a level bar
  drawn on the Microphone slider's track geometry: teal below the activation level, bright cyan at or
  above it, a white marker at the level, a decaying peak tick, and the number where the sliders show
  theirs; dimmed with "no mic" when there is no capture device; while the tab is shown it keeps
  `k33_voiceMeter` set so the mic is open for metering, nothing is sent). Subtitles moves up beside
  Output. That edit is not same-length, so `build_voice_ui.py` compiles the class with JPEXS FFDec
  (`ffdec-cli`, `$CODOL_FFDEC`) and splices only its DoABC tag into codo.gfx, checking every other tag
  is unchanged.
- **Options > Controls > Chat**: C1's own options screen (codo.gfx `OptionView_control_chat`) already
  has the push-to-talk row ("说话" with a key button and a "hold to talk" checkbox that switches the
  button between `toggletalk` and `+talk`), but its onInit hid it. `build_voice_ui.py` flips those three
  `visible = false` to true in the codo.gfx C1 uses (patch_ui_mp's; same byte length) and installs it as
  `client_shipRetail\patchoice_ui_mp.ff`, a *_ui_mp patch zone Kernel33 loads ahead of the UI zones.
  Rebinding there goes through C1's own AS_SetKeyBindingForCommand.
- **Path**: WASAPI shared-mode capture (default communications device) -> 48 kHz mono -> Opus VOIP,
  20 ms frames, 24 kbps, in-band FEC, DTX -> OOB `"v"` packets to `clc.serverAddress` through C1's
  NET_OutOfBandData (the game socket and its bdDTLS association) -> server relay -> OOB `"v"` back to
  each listener -> CL_ConnectionlessPacket's "v" branch (0x496482) -> per-talker jitter buffer (40 ms
  prebuffer, FEC/PLC on loss) -> mix -> WASAPI playback.
- **Talk balloon**: the server sets es.eFlags 0x100 on a talking player's entity for 500 ms, the bit
  C1's head-icon code already draws `headicontalkballoon` for (teammates only).
- **Dvars**: `codol_voice` (1; `+set codol_voice 0` installs nothing),
  `k33_voiceVolume` (100 %), `k33_voiceBitrate` (24000), `k33_voiceLoopback` (0; 1 plays your own
  voice back locally without a server), `k33_voiceDebug` (0; 1 prints status every 5 s, 2 every packet), `k33_voiceTone` (0; 1 transmits a
  440 Hz tone instead of the microphone, 2 the tone in 600 ms bursts every 1.5 s, for tests).
  Archived (saved in config_mp.cfg): `k33_voiceMode` (0 push-to-talk, 1 voice activation),
  `k33_voiceThreshold` (30 = -42 dBFS RMS; 0..100 = -60..0 dBFS), `k33_voiceHangMs` (400),
  `k33_voiceMicMute` (0). Read only: `k33_voiceLevel` (live mic level, -1 no microphone);
  `k33_voiceMeter` (set by Options > Audio).
- **Commands**: `+talk`/`-talk`, `toggletalk`, `k33voice` (status: devices, counters, mode,
  threshold, live mic level and gate state, who is talking), `k33voicemute <client>` (toggle a
  client-side mute).
- **UI**: the Scaleform callbacks `EnableVoiceTalk` (voice on/off) and `SetVoiceMicMute` (mic mute,
  stored in `k33_voiceMicMute`), both `ret 4` stubs in C1, now reach `voice.c`.
- C1 applies no command-line `+set` to dvars registered this late and runs no `+command`, so
  `voice.c` re-issues `+set <its dvar> <n>` and honours `+toggletalk` itself.

Build without touching the live DLL: `PATH=/c/msys64/mingw32/bin:$PATH sh build-voice.sh` writes
`Kernel33_voice.dll`.

Test automation (only with `+set k33_cmdFile 1` on the command line): the lines of
`<exe dir>\k33_cmd_<pid>.txt` are executed every half second, and `k33ready [0|1]` presses the room's
Ready button (AS_SetReadyState), so `room.match.cyborg` then `k33ready 1` puts a test client into a
match without the UI. `k33mouse <x> <y> [click|off]` drives the Scaleform menus of an unfocused test
client: C1's GetCursorPos / GetForegroundWindow / GetActiveWindow import slots report that client-area
position and the game window, and the click is posted (e.g. `k33mouse 1441 47 click` opens Options
at 1680x1050).

**HUD (hud menu ownerdraws).** C1's `ui_mp/hud_720.txt` (common_mp) still has IW4's voice items,
`talking` (ownerdraw 266) and `talker1`..`talker4` (193..196), but C1's CG_OwnerDraw (0x459E10) had
lost their code (193-196 return, 266 falls to the default). `voice.c` hooks CG_OwnerDraw's entry and
draws them like IW4's CG_DrawPlayerTalking / CG_DrawTalkerNum with C1's own calls
(Material_RegisterHandle "voice_on", UI_DrawHandlePic, UI_DrawText), at the menu's own rects and
text scale and under its visibility rules. `talking` shows while you transmit and dims when the mic
is silent; `talkerN` lists the players heard in the last 500 ms with their names (configstring
1280 + clientNum), mutes excluded. With `k33_voiceLoopback 1` you appear in the list yourself.

**Shared C1 helpers / no-console builds.** `c1game.c` holds what features share: Com_Printf, Cbuf_AddText,
the command list (Cmd_AddCommand), Dvar_RegisterInt and keyCatchers, each byte-checked once. `voice.c`
needs only that file; `console.c` uses it too. `kernel33.c` includes console.c and calls InstallConsole()
under `#ifndef K33_NO_CONSOLE`; `build-voice.sh` also links `Kernel33_noconsole.dll` with
`-DK33_NO_CONSOLE` and fails if it does not build. Without the console, voice prints go to C1's own
console and console_mp.log through Com_Printf.
