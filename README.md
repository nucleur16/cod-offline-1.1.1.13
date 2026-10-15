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

## Developer console

The developer console (IW4x-style `SH1X >` line, dvar names, autocomplete) lives on branch `dev` only;
main carries no console code.

## Voice chat
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
  `toggletalk` latches transmit on/off. The microphone stream runs only while transmitting.
- **Options > Controls > Chat**: C1's own options screen (codo.gfx `OptionView_control_chat`) already
  has the push-to-talk row ("说话" with a key button and a "hold to talk" checkbox that switches the
  button between `toggletalk` and `+talk`), but its onInit hid it. `build_voice_ui.py` flips those three
  `visible = false` to true in the codo.gfx C1 uses (patch_ui_mp's; same byte length) and installs it as
  `client_shipRetail\patch\voice_ui_mp.ff`, a *_ui_mp patch zone Kernel33 loads ahead of the UI zones.
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
  440 Hz tone instead of the microphone, for tests).
- **Commands**: `+talk`/`-talk`, `toggletalk`, `k33voice` (status: devices, counters, who is
  talking), `k33voicemute <client>` (toggle a client-side mute).
- **UI**: the Scaleform callbacks `EnableVoiceTalk` (voice on/off) and `SetVoiceMicMute` (mic mute),
  both `ret 4` stubs in C1, now reach `voice.c`.
- C1 applies no command-line `+set` to dvars registered this late and runs no `+command`, so
  `voice.c` re-issues `+set <its dvar> <n>` and honours `+toggletalk` itself.

Built by `build.sh` (libopus linked statically: `pacman -S mingw-w64-i686-opus`). The Controls-menu
push-to-talk row comes from `python build_voice_ui.py`, which writes `client_shipRetail\patch\voice_ui_mp.ff`.

Test automation (only with `+set k33_cmdFile 1` on the command line): the lines of
`<exe dir>\k33_cmd_<pid>.txt` are executed every half second, and `k33ready [0|1]` presses the room's
Ready button (AS_SetReadyState), so `room.match.cyborg` then `k33ready 1` puts a test client into a
match without the UI.

**HUD (hud menu ownerdraws).** C1's `ui_mp/hud_720.txt` (common_mp) still has IW4's voice items,
`talking` (ownerdraw 266) and `talker1`..`talker4` (193..196), but C1's CG_OwnerDraw (0x459E10) had
lost their code (193-196 return, 266 falls to the default). `voice.c` hooks CG_OwnerDraw's entry and
draws them like IW4's CG_DrawPlayerTalking / CG_DrawTalkerNum with C1's own calls
(Material_RegisterHandle "voice_on", UI_DrawHandlePic, UI_DrawText), at the menu's own rects and
text scale and under its visibility rules. `talking` shows while you transmit and dims when the mic
is silent; `talkerN` lists the players heard in the last 500 ms with their names (configstring
1280 + clientNum), mutes excluded. With `k33_voiceLoopback 1` you appear in the list yourself.

**Shared C1 helpers.** `c1game.c` holds what features share: Com_Printf, Cbuf_AddText, the command
list (Cmd_AddCommand), Dvar_RegisterInt and keyCatchers, each byte-checked once. Voice prints go to
C1's own console and console_mp.log through Com_Printf.
