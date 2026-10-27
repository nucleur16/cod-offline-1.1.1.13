# Kernel33.dll — codoMP_client_shipRetail.exe (CoDOL 1.1.1.13) patch

Runs the packed 1.1.1.13 client the way C1.exe runs, without TenProtect executing, and applies
client fixes (render gate, dvar defaults, plaintext config, Alt+Tab, raw mouse input, scoreboard,
patch-folder zones). Each patch checks the original bytes first and logs to `Kernel33.log` next
to the exe. See the comments in `kernel33.c` for addresses and reasoning.

## Install
1. `python make_exe.py` renames the packed exe's `Kernel32.dll` import to `Kernel33.dll` (needs C1.exe
   and `codoMP_client_shipRetail.exe.bak`; also writes `unpacked.z`).
2. Put `Kernel33.dll` next to `codoMP_client_shipRetail.exe` (TenProxy.dll + TerSafe.dll must still be there).
3. `python build_zones.py` builds every patch zone into `output\` with the install's layout (see "Patch
   zones" below); it is the shipping build, with only the usermaps `shipping_maps.txt` marks `ship` (see "Shipping
   maps"). Copy `output\client_shipRetail\patch\*` into `<game>\client_shipRetail\patch\` (create the
   folder; remove the old zone names listed under "Consolidated zones"), `output\client_shipRetail\chinese\patch_common_gametype_zm.ff`
   over the stock one (keep it as `.pre-pap`) and `output\main\iw_image_16.iwd` into `<game>\main\`. The patch folder
   holds six zones, one per load list and language (see "Consolidated zones"):
   - `codol_common_ui_mp.ff`: the front-end movies (codo.gfx, codologin.gfx; codol-classic's, every text a localize
     key, see "English") with Options > Controls > Chat (Talk key), Options > Audio > Voice (mode, mic level bar,
     microphone choice), Options > Graphics > Max FPS and Options > Controller (the pad tab), and the usermaps'
     map-select images. Keep every codo.gfx change in one zone: each zone carrying codo.gfx carries the whole movie,
     and the first one loaded wins.
   - `codol_common.ff`: the in-match movies (the HUD codohud.gfx and the pause menu codoingame.gfx with gamepad focus
     navigation), the party modes' tables and rules, the death machine sounds and the usermaps' create-room data.
   - `codol_english_locale.ff` / `codol_chinese_locale.ff`: the strings of each language (see "English").
   - `codol_common_postgfx_mp.ff`: the gamepad button icons drawn in hints (CONTROLLER.md) and the field order menus;
     install it with the DLL. Without it the hints show the pad's button names as text.
   - `codol_infected_cyborg_postgfx_mp.ff`: the Infected mode's cyborg assets (on its own: see "Consolidated zones").

## Build (MSYS2 mingw32)
```sh
PATH=/c/msys64/mingw32/bin:$PATH sh build.sh
```
`unpacked.z` (C1.exe's decompressed code range) is embedded as a resource; `build.sh` copies the
result to `../Kernel33.dll` (`K33_NO_COPY=1` leaves it in this folder). Needs libopus
(`pacman -S mingw-w64-i686-opus`) and SDL2 2.32.10, which `build.sh` downloads once into the gitignored
`third_party/` (official SDL2-devel mingw package, SHA-256 checked) and links statically: the DLL has no
SDL2.dll dependency.

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

## Frame cap
`1848` is C1's `com_maxfps` (there is no `cg_fps`; `591`, cg_drawFPS, is registered but nothing reads it).
Stock C1 registers it 0..100, default 85, not archived, with IW's integer limiter (`1000 / maxfps` whole ms,
so 144 ran at 166 and 60 at 62.5). Kernel33 (`PatchFrameCap` in kernel33.c):
- default 0 and archive (saved in `players\config_mp.cfg`), domain raised to **0..1000**; 0 = no cap (the
  engine still waits at least 1 ms a frame, so 1000 fps is the ceiling);
- frames start on a fractional schedule (exact average cap) and the last ms of the wait yields instead of
  `Sleep(1)`, so caps hold within about 1 % (measured 30/60/75/90/120/144/165/240/250 exact; 360 and 500 are
  limited by the test machine);
- there is no separate menu, lobby or background cap: menus run at the same cap (about 460 fps uncapped on the
  test machine); a minimized window only adds `Sleep(5)` per loop. Vsync ("Sync every frame", r_vsync) still
  limits to the refresh rate when it is on.

**Options > Graphics > Max FPS** (`zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/views/option_sub/OptionView_video.as`): C1's hidden Aspect Ratio combo box, shown below
Anti-Aliasing (Vsync moves down a row): Unlimited / 30 / 60 / 75 / 90 / 120 / 144 / 165 / 240 / 360 / 500, set
with the screen's Apply button (Cancel discards it); a value set from the console shows as its own entry. C1
disables the whole Graphics tab in a room ("temporarily disabled"); set it from the main menu.

To see the frame rate in a match: `set 2614 1` (C1's loss/ping/FPS debug graphs; the FPS number is readable
below 100).

## Developer console

The developer console (IW4x-style `SH1X >` line, dvar names, autocomplete) lives on branch `dev` only;
main carries no console code.

## Controller (branch `feat/controller`)
`controller.c` adds gamepad play: SDL2 2.32.10 GameController (static; Xbox, PlayStation, Switch Pro, generic 
pads, hot-plug; XInput fallback) on its own ~1 kHz thread, pad buttons as real C1 keys (`BUTTON_A`.. `DPAD_RIGHT`, 
bindable and saved in config_mp.cfg, queued through C1's Sys_QueEvent), analog move/look in CL_MouseMove, a 
stick-driven cursor for the Scaleform menus (A click, B Escape) and `gpad_*` dvars. See `CONTROLLER.md` for C1's 
input path, the dvars, commands, default binds, what was verified and the phase 2/3 plan. `build.sh` builds it
into Kernel33.dll; `PATH=/c/msys64/mingw32/bin:$PATH sh build-controller.sh` writes a side copy,
`Kernel33_controller.dll`, and copies nothing.

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
  "Sound Presets" combo box. `zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/views/option_sub/OptionView_audio.as` is that class with the section shown and
  rewired, one control per row: **Voice Mode** (the presets box: Push to talk / Voice activation ->
  `k33_voiceMode`), **Voice** (chat volume -> `k33_voiceVolume`), **Microphone** (checkbox = mic on,
  `k33_voiceMicMute`; slider = activation level, `k33_voiceThreshold`), **Input Level** (a level bar
  drawn on the Microphone slider's track geometry: teal below the activation level, bright cyan at or
  above it, a white marker at the level, a decaying peak tick, and the number where the sliders show
  theirs; dimmed with "no mic" when there is no capture device; while the tab is shown it keeps
  `k33_voiceMeter` set so the mic is open for metering, nothing is sent). Subtitles moves up beside
  Output. That edit is not same-length, so `build_zones.py` compiles the class with JPEXS FFDec
  (`ffdec-cli`, `$CODOL_FFDEC`) when the .as changes and splices only its DoABC tag into codo.gfx, checking
  every other tag is unchanged (zonekit/gfx.py; the compiled tag is stored, so FFDec is needed only after an edit).
- **Options > Controls > Chat**: C1's own options screen (codo.gfx `OptionView_control_chat`) already
  has the push-to-talk row ("说话" with a key button and a "hold to talk" checkbox that switches the
  button between `toggletalk` and `+talk`), but its onInit hid it. voice_ui_mp's codo.gfx has those three
  `visible = false` flipped to true (same byte length; the stored OptionView_control_chat tag, decompiled in
  `zones/voice_ui_mp/.../as/`); it installs in `client_shipRetail\patch\codol_common_ui_mp.ff`, a *_ui_mp patch
  zone Kernel33 loads ahead of the UI zones.
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

Built by `build.sh` (libopus linked statically: `pacman -S mingw-w64-i686-opus`). The Controls-menu
push-to-talk row comes from voice_ui_mp's codo.gfx (`python build_zones.py voice_ui_mp` writes
`output\client_shipRetail\patch\codol_common_ui_mp.ff`, the zone it is part of).

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

**Shared C1 helpers.** `c1game.c` holds what features share: Com_Printf, Cbuf_AddText, the command
list (Cmd_AddCommand), Dvar_RegisterInt and keyCatchers, each byte-checked once. Voice prints go to
C1's own console and console_mp.log through Com_Printf.

### Sound voices (`soundvoices.c`, through `sounddiag.c`'s hooks)
The 3D play path's channel check (call of 0x736C10 at 0x73E4DB) decides two more things before C1 takes a voice:
- **Alias limit** (`k33_sndAliasLimit`, default 4, 0 = off): a one-shot alias on a channel of priority <= 1 holds at
  most that many 3D voices; one more play takes its farthest copy's voice when nearer, else it is not started.
  mp_abandon_sh_mk's fireworks launchers played `fireworks_launch` ~50 times a second and held 19.7 of the 32 3D voices
  on average (snddiag, 2026-10-19..21), starving the map's loops and the players' gear sounds.
- **Nearest wins at a cap** (`k33_sndNearest`, default 1): a 3D sound refused because its channel is at its cap
  (ambdist1/2 and physics 6, bulletimpact 10) takes the voice of that channel's farthest 3D sound when nearer. C1's
  own SND_StealVoice only runs after the cap check.
`+set codol_soundvoices 0` turns both off at startup. The voice diagnostics (`k33_snddiag`, `CODOL_SNDDIAG`) now
default to 0; at 1+ they also log `snd policy` totals and `why=limit` refusals to Kernel33.log.

## English (`k33_english`)
Both languages run the same UI movies, and every text those movies show is a localize key. The language is only which
string zones load. Kernel33 decides that at startup, from the command line:

    codoMP_client_shipRetail.exe +set dw_username <name>                        English (default)
    codoMP_client_shipRetail.exe +set dw_username <name> +set k33_english 1     English
    codoMP_client_shipRetail.exe +set dw_username <name> +set k33_english 0     Chinese

- **Default: English** (no option, or `1`). The last `k33_english` on the line wins; any value but `0`/`1` is ignored.
- Existing launchers may still use `+set codol_patchlang en` or `+set codol_patchlang zh`. An explicit valid
  `k33_english` takes precedence over that compatibility alias, regardless of argument order. Both selectors use
  one parsed language choice; they cannot independently filter out both sets of string zones.
- Read by the DLL from the command line only (`patch_locale_policy.h`, `CodolPatchLanguageFromArgv`): C1 execs
  `players\config_mp.cfg` before it registers its dvars and applies no `+set` to a dvar it does not know, so the
  option is not a saved dvar. Nothing writes it to config_mp.cfg, and a config write never changes it; it lasts until
  the client is started without it.
- What it switches: `client_shipRetail\patch\codol_english*.ff` load with `1`, `codol_chinese*.ff` with `0`
  (since the consolidation one zone each, codol_english_locale and codol_chinese_locale; the table below names the
  source zones they are built from);
  every other patch zone loads either way. Only one set loads because C1 holds at most 12000 localize assets ("Exceeded
  limit of 12000 'localize' assets", a fatal error at startup) and a name loaded by both sets takes two. The DLL has no
  other language-dependent code: GBK text input, fonts, voice, controller, usermaps and the frame cap are the same.

| zone | loads with | what |
|---|---|---|
| voice_ui_mp | both | the front end: codo.gfx (lobby, rooms, loadout, store, options; codol-classic's THEATRE screen and SURVIVAL / THEATRE buttons; Options > Voice, Max FPS, Controller) and codologin.gfx |
| controller_ingame | both | in a match: codohud.gfx (HUD, scoreboard) and codoingame.gfx (pause menu with pad navigation) |
| codol_english | `1` | 9,700 of C1's strings in English (menus, HUD, weapons, perks, challenges, killstreaks, errors) |
| codol_english_font | `1` | fonts_ingame.gfx with Latin glyphs drawn for English text |
| codol_english_ui, codol_english_ui_extra | `1` | the UI movies' K33UI_* keys in English (codo + login, hud + ingame) |
| codol_english_zombies_base | `1` | zombies weapon, perk, challenge and subtitle strings |
| codol_english_usermaps | `1` | usermaps_patch's map names and descriptions (those are GBK Chinese), filtered by shipping_maps.txt the same way |
| codol_chinese_ui, codol_chinese_ui_extra | `0` | the UI movies' K33UI_* keys in Chinese |

The movies are codol-classic's English ones (with Kernel33's Options classes and the pause menu's pad navigation),
converted by `ui_keys.py`. C1's Scaleform translator replaces a text that starts with `@` by the localize entry of
that name, both for a text field's text in the movie and whenever code sets `.text` / `.label` (C1's own
`codo.constants.LocalizationDict` is a list of `"@SF_UI_..."` names). A missing entry shows as `@NAME` and nothing
fails. ui_keys.py made:
- every static text whose stock text is Chinese and whose English text differs into `@K33UI_<MOVIE>_<tag>`, one key
  per distinct English/Chinese pair (309), with the stock Chinese text and codol-classic's English text as entries. The
  key is the whole text: `@KEY` inside an HTML text's tags is not translated;
- codol-classic's literals into keys, in the DoABC string pool (`zonekit/abcstr.py`) or, where the literal is also a
  name in the class or the class needed code changes, in the class's source (MatchMPCTMenuView, MatchRoomListView,
  MatchSelectModeHomeView are now hand-written, in `zones/voice_ui_mp/.../codo.gfx.gfxpatch/as/codo/views/`). C1's own
  `SF_UI_*` names are used where C1 has the text (Refresh, Map, Mode, Multiplayer, Zombies, Defaults);
- `LocalizationDict` C1's class again (codol-classic had put English text in place of six of its names).

**Adding a string** (English and Chinese):
1. In the `.as` source, show `"@K33UI_<NAME>"` (`.text`, `.label`, a list item's `label`) or, to build a string,
   `LocalizeUtil.localize("@K33UI_<NAME>")` (`codo.utils.LocalizeUtil`; `LocalizeUtil.replace(text, [a, b])` fills
   `#A#` `#B#`). OptionView's `k33L("K33UI_<NAME>")` does the same.
2. Add `"K33UI_<NAME>": ("English", "中文", "codo.gfx")` to `NAMED` in `ui_keys.py` (the movie: codo.gfx /
   codologin.gfx for codol_*_ui, codohud.gfx / codoingame.gfx for codol_*_ui_extra).
3. `python ui_keys.py` compiles the changed classes (FFDec), adds the entry to `zones/codol_english_ui*/localize.str`
   and `zones/codol_chinese_ui*/localize.str` and to their `zone_source` manifests; then `python build_zones.py`.
   (By hand: one `K33UI_<NAME><TAB>text` line in each localize.str, one `localize,K33UI_<NAME>` line in each manifest.)
To change a text, edit the localize.str line; ui_keys.py never overwrites an entry that exists.

Other notes:
- With Chinese text, a field keeps the size codol-classic chose for English (a few are 12 px instead of 16 px).
- The THEATRE screen is codol-classic's theatre playback: this project has no backend for it (its
  `codol_theatre_*` dvars), so the list stays empty and WATCH does nothing. The SURVIVAL button sets
  `codol_survival_direct_home` and opens Survival as in codol-classic; "Fullscreen Windowed" in Options > Video sets
  `codol_borderless`, which Kernel33 does not implement (it behaves as windowed).
- 000_codol_party_modes and codol_loading_mode_names name modes C1 does not have (in English, with no Chinese text to
  fall back to) and load in both languages.
- `Kernel33.log` says which: `english: on (k33_english 1): 1 codol_english* zone(s) queued`, `english: off
  (k33_english 0): 1 codol_english* zone(s) skipped, 1 codol_chinese* zone(s) queued`, or, with English on and no
  English zone installed, `english: on (k33_english 1), but no codol_english*.ff zone is in the patch folder: the
  game stays Chinese` (missing zones are never an error; a missing string shows as its `@NAME`).
- Install all of them together: an older `codol_english_ui.ff` / `codol_english_ui_extra.ff` (which carried English
  movies) would override the shared movies in English mode.
- The pure check lists `client_shipRetail\chinese\` and `main\` only, never `patch\`, so both states pass it.
- Launchers: add the option to the client's command line. `3-START-CLIENT.cmd` (game folder) and the lobby's
  `OFFLINE-DEV.cmd` (`CODOL_CLIENT_ARGS=+set k33_english 0`) pass extra arguments through; they must start
  `codoMP_client_shipRetail.exe`, since `C1.exe` never loads Kernel33.dll (and then shows the stock Chinese client).

## Discord Rich Presence (`discord.c`)
The launcher's `TCLS\CoDOLPresence.exe` helper (a PyInstaller build of the server repo's
`tools/codol_discord_presence.py`) now runs inside the client: no Python, no second process. It talks
to Discord's local IPC pipe (`\\.\pipe\discord-ipc-0..9`) as application 1554730441825718272 and shows
- front end: "In the lobby" / "Choosing a match", elapsed since the client started;
- in a match: "<mode> on <map>" (e.g. "Team Deathmatch on Killhouse"), elapsed since joining, and as state
  "N players, M bots" on a server at 127.0.0.1 (its `getinfo` reply) or "In a match" on a remote one;
- the `codol` art asset with the text "Call of Duty Online".

The map and mode come from the server's serverinfo configstring and the connection state from C1 itself,
not from `console_mp.log`. All pipe and socket I/O runs on a worker thread with timeouts; with no
Discord running it retries every 5 s, and it reconnects when Discord restarts. `codol_discord 0`
(archived) clears the activity and closes the pipe; `+set codol_discord 0` on the command line installs
nothing. `Kernel33.log` gets `discord:` lines for connection changes and each new activity.
With this DLL installed the launcher should stop starting `CoDOLPresence.exe`, or both publish for the
same process.

## Survival category (`survival_category.c`)
C1 1.1.1.13 knows three match categories, mp, coop and zm. The category is the string the client gets with a map
(`preloadmap <map> <category>` from the room, line 2 of the server's `loadingnewmap`, the gamestate's type). Kernel33
adds a fourth, **`survival`** (exact, any case), modelled on CoDOL 2.6.9.7's native survival:
- **Zones.** The zone-list builder 0x4CCDF0 loads `patch_common_gametype_survival`, `common_gametype_survival`,
  `patch_localized_common_gametype_survival` and `localized_common_gametype_survival` (alloc flags 4, C1's order;
  missing `patch_` zones only print), then the map's own zones as for mp: `patch_<map>`, `<map>`, `ffotd_client`.
  No common_gametype_coop and no coop session zone. The loading-bar estimate 0x4B6380 counts no gametype zone for it
  (the survival zones have no stock size).
- **Constant configstrings.** 0x612850 loads `mp/constConfigStrings_survival.csv` in place of the mp table. It ships in
  `codol_common` (source zone `codol_survival_category`): a copy of 1.1.1.13's `mp/constconfigstrings_coop.csv`
  (1803 rows). 2.6.9.7's own survival table is in 2.6.9.7's configstring layout, 72 slots off C1's in every section,
  so it cannot be used. The config string checksum of this table is not C1's VM checksum (0x83B094E, which nobody
  has reproduced) but CRC-32 (zlib `crc32`) over every cell followed by a NUL, row-major
  (`survival_category_policy.h`). The server computes the same from the same CSV: **0x62BD78F1** for the shipped
  table. When no loaded zone has the table, C1's default table and checksum stay, and the server refuses the match.
- Every other category string runs C1's code unchanged: `tests/test_survival_category_x86.py` runs 0x4CCDF0,
  0x4B6380 and 0x612850 stock and patched in Unicorn over 17 categories, 4 maps and 6 loaded-zone states and
  compares the zone lists, flags, size estimate, table, checksum and registers (`K33_C1_EXE=<C1.exe> sh
  tests/run_tests.sh`).
- `+set codol_survival 0` on the command line installs nothing. `Kernel33.log` says `survival category: ...`.
- The rest of a survival client is data: `common_gametype_survival` must carry the animlists that serverinfo 2482
  asks for (`animlists/common_actor_shared`, `common_human`, `common_dog`, as 2.6.9.7's does) and the survival
  menus. The cgame, UI and room code test `g_gametype` / the room gametype for "coop" and "zm" on their own; this
  change does not touch them.

## Patch zones (`zones/`, `zone_source/`, `build_zones.py`)
Every patch zone this project installs next to the stock game, as editable sources, built into `output/` by one
script. (The ported usermap map zones themselves, mp_*.ff / zm_sumpf_sh.ff, are not part of this repository; they
ship in the CODOL 1.1.1.13 map pack.)

```
build_zones.py                the only build entry point: zone_source + zones -> output\, files/ copied
zone_source/<zone>.csv        the build manifest: >settings, then one "type,name[,param=..]" line per asset, in order
zones/<zone>/                 the zone's contents
  rawfile/<name>              rawfiles as files (.txt, .arena, .vision, .atr, ...)
  rawfile/<name>.gfxpatch/    a Scaleform movie as a patch over the stock movie it was made from:
    patch.txt                   stock tags kept by index, new tags by file, one "as" line per as/ file (below)
    tags/*.bin                  the new tags, byte for byte
    tags/*_75_<font>.json       DefineFont3 tags as glyph patches (only the glyphs that differ)
    as/<package path>/*.as      the source of every new DoABC class: FFDec's decompile, or for voice_ui_mp
                                (OptionView, OptionView_audio, OptionView_video, k33_pad_diagrams.as.inc) and
                                controller_ingame (MainPause) the hand-written class it was compiled from
  stringtable/<name>          stringtables as CSV (UTF-8; ">enc" says what the zone stores: gbk, ascii)
  localize.str                localize entries, NAME<TAB>value, in zone order
  script_strings.json         the zone's script string list (C#-written zones only)
  image/<name>.png            images whose pixels are in the zone (A8R8G8B8): level 0, and <name>.mips.png the
                              smaller levels side by side (delete it to have the build make them, 2x2 box filter)
  <type>/NNNN_<name>.bin      assets kept as raw blobs (streamed images, materials, techsets, sounds, models, ...)
zonekit/                      container, zone image, asset codecs, gfx patches, manifests, the zone list
files/                        patch files that are not zones (sound_channels.txt, main\iw_image_16.iwd), copied (sound_channels.txt
                              filtered by shipping_maps.txt)
shipping_maps.txt             the usermaps whose UI ships ("ship") and the ones still in development ("dev")
```

### Build
Python 3.10+ with `pycryptodome` (`pip install pycryptodome`).
```
python build_zones.py                                   # every zone -> output\ (shipping build: "ship" maps only)
python build_zones.py --maps all                        # every zone with every usermap -> output_dev\ (local testing)
python build_zones.py codol_common_ui_mp voice_ui_mp    # some zones (a source zone builds the zone it is part of)
python build_zones.py --parts                           # every source zone on its own -> output_parts\ (the old layout)
python build_zones.py --list                            # the zones, their install folder and origin
python build_zones.py --verify                          # build and compare each with the original it came from
python build_zones.py --decompile [zone ...]            # refresh zones\ + zone_source\ from the shipped files
```
Inputs (read, never written): `--game` (default `D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13`): `C1.exe` for
the zone key and `client_shipRetail\chinese` for the stock zones the gfx patches and `@stock:` blobs come from;
`--zz` (codol-classic-zz) only for `--verify`/`--decompile`. Every output keeps the recorded key and timestamp so an
untouched tree rebuilds the original bytes; `--fresh-key` draws new ones.

Editing: change the file under `zones/<zone>/` and build. A changed `.gfxpatch/as/*.as` (any zone) is compiled
into its movie by JPEXS FFDec (`--ffdec`, default `C:\Program Files (x86)\FFDec\ffdec-cli.exe`) over the stock class,
and only that DoABC tag is replaced. Each as/ file's sha256 is recorded in patch.txt (with the diagrams include),
so FFDec is needed only after an edit; `--compile-as` also compiles the hand-written classes unchanged, which
reproduces their stored tags byte for byte (the decompiled ones are skipped). Rawfiles, localize, stringtables and blobs can be changed and
resized freely in the flat (Python-written) zones. In the C#-written zones (patch_common_gametype_zm,
codol_infected_cyborg_postgfx_mp) assets point back at earlier data, so a resized asset can move what later assets
point at: the manifest records each decoded asset's length (`len=`) and the build warns when an edit changes it.

### Where each zone goes
`client_shipRetail\patch\` (Kernel33 loads `*_ui_mp` zones ahead of the UI zones, `*_postgfx_mp` zones after
`code_post_gfx` (permanent, images allowed), the rest ahead of `code_pre_gfx`): codol_common, codol_common_ui_mp,
codol_common_postgfx_mp, codol_infected_cyborg_postgfx_mp, codol_english_locale, codol_chinese_locale, plus
`sound_channels.txt`.
`client_shipRetail\chinese\` (the 1.1.1.13 install and the CODOL 1.1.1.13 map pack): patch_common_gametype_zm (replaces
the stock zone; keep the stock one as `patch_common_gametype_zm.ff.pre-pap`). `main\`: iw_image_16.iwd (the PaP camo
images). The usermaps' data (usermaps_patch / usermaps_ui_mp sources, sound_channels.txt) list the usermaps in the
create-room map pool and fix their sound channels; they do nothing for a map that is not installed. Install `output\`
(the shipping build) for players and the map pack; `output_dev\` only on test clients that have the dev maps' zones
and iwds.

### Consolidated zones (`zone_packs/`, `zonekit/packs.py`)
The source zones under `zone_source/` + `zones/` stay as they are (each with its own shipping filter and its own
`--verify` against the file it came from), but what installs is one zone per load list and language:
`zone_packs/<zone>.csv` names its source zones (parts) in order and the build concatenates their assets.

| installed zone | list | languages | source zones (old installed names) |
|---|---|---|---|
| codol_common | ahead of code_pre_gfx | both | 000_codol_party_modes, codol_loading_mode_names, codol_survival_category, codol_zm_deathmachine_audio, controller_ingame, usermaps_patch (all but its strings) |
| codol_common_ui_mp | ahead of the UI zones | both | voice_ui_mp, usermaps_ui_mp |
| codol_common_postgfx_mp | after code_post_gfx | both | codol_field_orders_postgfx_mp, controller_glyphs_postgfx_mp |
| codol_english_locale | ahead of code_pre_gfx | `k33_english 1` | codol_english, codol_english_font, codol_english_ui, codol_english_ui_extra, codol_english_usermaps, codol_english_zombies_base |
| codol_chinese_locale | ahead of code_pre_gfx | `k33_english 0` (and no valid language) | codol_chinese_ui, codol_chinese_ui_extra, usermaps_patch's GBK strings |
| codol_infected_cyborg_postgfx_mp | after code_post_gfx | both | itself: C#-written, its assets point into its own runtime blocks, so they cannot move into another zone |

- Every asset keeps its list: graphics (images, materials, menus) only in `*_ui_mp` / `*_postgfx_mp` zones (an image
  ahead of code_pre_gfx crashes C1 at startup, AV read 0x0000000C).
- C1 keeps the first asset it links under a name, and the old zones loaded in alphabetical order, so the parts are
  listed in that order and a later part's copy of a name an earlier part carries is left out ("shadowed": the old
  install never used it). There are five: codol_loading_mode_names' gun.txt / oic.txt / shrp.txt / _gametypes.txt
  (000_codol_party_modes' load first) and codol_english_zombies_base's ZOMBIE_THUNDERGUN(_UPGRADED) (codol_english's
  load first). usermaps_patch's GBK map names, which codol_english_usermaps' English ones always covered in English,
  are in codol_chinese_locale only, so no name is in two zones a language loads.
- Kernel33 skips an old zone name while the zone that replaced it is in the patch folder (`kCodolPatchZoneMerged` in
  patch_locale_policy.h; `patch folder: skipped voice_ui_mp.ff: merged into codol_common_ui_mp.ff`), so stale files on
  an existing install never load twice, and a folder with only the old names (codol-classic-zz) still loads them.
  The glyphs (controller_glyphs.c) are looked up in codol_common_postgfx_mp, else in controller_glyphs_postgfx_mp.
- `tests/check_zone_packs.py [--maps all] [--old <folder>]` loads the built zones the way C1 does for each language
  and proves the same assets reach the game, byte for byte and in the same list, that no list carries a name twice
  and that every asset of every old zone is in exactly one new zone (or shadowed); it also checks codo.gfx's
  endless LevelChooser and comaplist column 23. `tests/test_zone_packs.py` (portable, CI) checks the packs against
  Kernel33's table. `--verify` compares each pack with its recorded `>build-md5` (`>build-md5-all` for `--maps all`).
- To add a source zone: give it a zone_source manifest as before and add a `part,<zone>` line to the pack of its list
  and language (and its old name to kCodolPatchZoneMerged if it was ever installed on its own).

### Shipping maps (`shipping_maps.txt`)
The usermap zones' sources carry the front-end data of every usermap worked on; `shipping_maps.txt` decides which
of it ships. One line per map, `<map> ship` or `<map> dev` (`#` comments). The shipping build (`python
build_zones.py`, `output\`) leaves out each `dev` map's:
- create-room map pool row (usermaps_patch `mp/comaplist.csv`; the 60 stock rows, checked against
  patch_code_post_gfx_mp's table through C1's cell hashes, are never touched),
- vlobby arena entry (usermaps_patch `mp/basemaps.arena`; the stock text, checked against code_post_gfx_mp's, is
  never touched),
- display-name / description strings no remaining row or entry names (usermaps_patch `localize.str`),
- vision override (`vision/<map>.vision`),
- map-select image (usermaps_ui_mp `sfproto_<map>_usermap`; its pixels are in a map pack iwd, and a client without
  that iwd fails at startup, so an image must not ship before its map),
- sound channel fixes (`files\...\sound_channels.txt`).

`python build_zones.py --maps all` (or `CODOL_MAPS=all`) keeps every listed map and writes `output_dev\`, so a
local test build never lands in the folder that ships. A map that the zones name but `shipping_maps.txt` does not
list fails both builds. `--verify` compares the unfiltered build with the recorded original (the installed zones
carry every map) and prints what the shipping build left out; `--list` prints both lists. Which assets are per map
is declared per zone in `zonekit/zonelist.py` (`maps=`); the filter is `zonekit/shipping.py`.

To add a map: add its data to the zone sources as before (pool row after the stock rows, arena entry after the stock
text, strings, image, vision, sound channels), list it as `dev`, test with `--maps all`; when its zones and iwd are
in the map pack, change it to `ship` and build.

### Provenance
| zone | made by |
|---|---|
| codol_english | `tools/codol_english/build_ff.py` (CoD4x_Server, localizedStrings-1.1.1.13) from `codol_english.tsv` |
| codol_english_font | `build_font_ff.py` (same tools); byte-identical to localizedStrings-1.1.1.13 `zones/` |
| codol_english_zombies_base | localizedStrings-1.1.1.13 `tools/localize_zone.py` from `strings/codol_english_zombies_base/strings.tsv` (identical) |
| codol_english_ui, codol_english_ui_extra, codol_chinese_ui, codol_chinese_ui_extra | `ui_keys.py`: the K33UI_* entries; the English text is codol-classic's (its English movies, launcher releases r22-r30 and `deploy/client-patches/revive-token-mode/build_token_mode.py`), the Chinese text the stock movies' |
| codol_english_usermaps | this repository (authored here, no shipped original): usermaps_patch's strings in English |
| codol_loading_mode_names | codol-classic `launcher/tools/build_loading_mode_names.py` (r21) |
| codol_zm_deathmachine_audio | codol-classic `deploy/client-patches/build_deathmachine_audio.py` |
| 000_codol_party_modes | codol-classic branch `codex/party-infected-20261017` (r22) |
| codol_infected_cyborg_postgfx_mp | codol-classic branch `codex/party-infected-20261017`, `build_infected_cyborg_assets.py` |
| voice_ui_mp, controller_ingame | codol-classic's English codo.gfx / codologin.gfx / codohud.gfx / codoingame.gfx (formerly codol_english_ui / _extra) made language-neutral by `ui_keys.py`, with this repository's hand-written classes in their `.gfxpatch/as/` (formerly `voice_ui/`, build_voice_ui.py / build_ingame_ui.py) |
| usermaps_patch, usermaps_ui_mp | the game folder's `usermaps\build_usermaps_patch.py` (now superseded by the sources here) |
| controller_glyphs_postgfx_mp | this repository (authored here, no shipped original): the former `glyphs/glyphs.bin` icons (Xelu's CC0 prompts) as image assets with C1's `hud_status_dead` material layout |
| patch_common_gametype_zm | `CodolSoundDumper zm-pap-patch` (CODOL-AssetExplorer + `pap_patch_staging\tool_src`) over the stock zone |

### Known gaps
- Asset types without a codec stay raw blobs: images, materials, techsets, shaders, sounds (alias lists with their
  loaded PCM), models, xanims, weapons, fx. Stringtables with back-referenced cells (the PaP zone's) are blobs too.
- patch_common_gametype_zm is the stock zone's blobs (taken from the install by sha1) plus 959 committed blobs. It was
  compressed by .NET's zlib; this builder uses Python's, so the file differs from the shipped one although the zone
  image is identical.
