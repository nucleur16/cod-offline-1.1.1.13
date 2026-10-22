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
   zones" below). Copy `output\client_shipRetail\patch\*` into `<game>\client_shipRetail\patch\` (create the
   folder), `output\client_shipRetail\chinese\patch_common_gametype_zm.ff` over the stock one (keep it as
   `.pre-pap`) and `output\main\iw_image_16.iwd` into `<game>\main\`. Among them:
   - `voice_ui_mp.ff`: the options-screen changes: Options > Controls > Chat (Talk key), Options > Audio > Voice
     (mode, mic level bar, microphone choice), Options > Graphics > Max FPS and Options > Controller (the pad
     tab). Keep every codo.gfx change in this one zone: each *_ui_mp patch zone carrying codo.gfx carries the
     whole movie, and the first one loaded wins.
   - `controller_ingame.ff`: the in-match pause menu (codoingame.gfx) with gamepad focus navigation.

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

**Options > Graphics > Max FPS** (`actionscript/OptionView_video.as`): C1's hidden Aspect Ratio combo box, shown below
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
  "Sound Presets" combo box. `actionscript/OptionView_audio.as` is that class with the section shown and
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
  `zones/voice_ui_mp/.../as/`); it installs as `client_shipRetail\patch\voice_ui_mp.ff`, a *_ui_mp patch zone
  Kernel33 loads ahead of the UI zones.
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
push-to-talk row comes from `voice_ui_mp.ff` (`python build_zones.py voice_ui_mp` writes
`output\client_shipRetail\patch\voice_ui_mp.ff`).

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

## Patch zones (`zones/`, `zone_source/`, `actionscript/`, `build_zones.py`)
Every patch zone this project installs next to the stock game, as editable sources, built into `output/` by one
script. (The ported usermap map zones themselves, mp_*.ff / zm_sumpf_sh.ff, are not part of this repository; they
ship in the CODOL 1.1.1.13 map pack.)

```
build_zones.py                the only build entry point: zone_source + zones -> output\, files/ copied
zone_source/<zone>.csv        the build manifest: >settings, then one "type,name[,param=..]" line per asset, in order
zones/<zone>/                 the zone's contents
  rawfile/<name>              rawfiles as files (.txt, .arena, .vision, .atr, ...)
  rawfile/<name>.gfxpatch/    a Scaleform movie as a patch over the stock movie it was made from:
    patch.txt                   stock tags kept by index, new tags by file, "source" classes (below)
    tags/*.bin                  the new tags, byte for byte
    tags/*_75_<font>.json       DefineFont3 tags as glyph patches (only the glyphs that differ)
    as/**.as                    FFDec's decompile of every other new DoABC class (for reading / --compile-as)
  stringtable/<name>          stringtables as CSV (UTF-8; ">enc" says what the zone stores: gbk, ascii)
  localize.str                localize entries, NAME<TAB>value, in zone order
  script_strings.json         the zone's script string list (C#-written zones only)
  <type>/NNNN_<name>.bin      assets kept as raw blobs (images, materials, techsets, sounds, models, ...)
actionscript/                 the hand-written ActionScript classes of voice_ui_mp (OptionView, OptionView_audio,
                              OptionView_video, k33_pad_diagrams.as.inc) and controller_ingame (MainPause)
zonekit/                      container, zone image, asset codecs, gfx patches, manifests, the zone list
files/                        patch files that are not zones (sound_channels.txt, main\iw_image_16.iwd), copied as is
```

### Build
Python 3.10+ with `pycryptodome` (`pip install pycryptodome`).
```
python build_zones.py                                   # every zone -> output\
python build_zones.py voice_ui_mp controller_ingame     # some zones
python build_zones.py --list                            # the zones, their install folder and origin
python build_zones.py --verify                          # build and compare each with the original it came from
python build_zones.py --decompile [zone ...]            # refresh zones\ + zone_source\ from the shipped files
```
Inputs (read, never written): `--game` (default `D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13`): `C1.exe` for
the zone key and `client_shipRetail\chinese` for the stock zones the gfx patches and `@stock:` blobs come from;
`--zz` (codol-classic-zz) only for `--verify`/`--decompile`. Every output keeps the recorded key and timestamp so an
untouched tree rebuilds the original bytes; `--fresh-key` draws new ones.

Editing: change the file under `zones/<zone>/` or `actionscript/` and build. A changed `actionscript/*.as` is compiled
into its movie by JPEXS FFDec (`--ffdec`, default `C:\Program Files (x86)\FFDec\ffdec-cli.exe`) over the stock class,
and only that DoABC tag is replaced; the compiled tags of the untouched sources are stored, so FFDec is needed only
after an edit (compiling all of them with `--compile-as` reproduces the stored tags byte for byte). `--compile-as`
also compiles edited files under a `.gfxpatch/as/`. Rawfiles, localize, stringtables and blobs can be changed and
resized freely in the flat (Python-written) zones. In the C#-written zones (patch_common_gametype_zm,
codol_infected_cyborg_postgfx_mp) assets point back at earlier data, so a resized asset can move what later assets
point at: the manifest records each decoded asset's length (`len=`) and the build warns when an edit changes it.

### Where each zone goes
`client_shipRetail\patch\` (Kernel33 loads it ahead of `code_pre_gfx`): 000_codol_party_modes, codol_english,
codol_english_font, codol_english_ui, codol_english_ui_extra, codol_english_zombies_base,
codol_infected_cyborg_postgfx_mp, codol_loading_mode_names, codol_zm_deathmachine_audio, controller_ingame,
usermaps_patch, usermaps_ui_mp, voice_ui_mp, plus `sound_channels.txt`.
`client_shipRetail\chinese\` (the 1.1.1.13 install and the CODOL 1.1.1.13 map pack): patch_common_gametype_zm (replaces
the stock zone; keep the stock one as `patch_common_gametype_zm.ff.pre-pap`). `main\`: iw_image_16.iwd (the PaP camo
images). usermaps_patch / usermaps_ui_mp / sound_channels.txt list the usermaps in the create-room map pool and fix
their sound channels; they do nothing for a map that is not installed.

### Provenance
| zone | made by |
|---|---|
| codol_english | `tools/codol_english/build_ff.py` (CoD4x_Server, localizedStrings-1.1.1.13) from `codol_english.tsv` |
| codol_english_font | `build_font_ff.py` (same tools); byte-identical to localizedStrings-1.1.1.13 `zones/` |
| codol_english_zombies_base | localizedStrings-1.1.1.13 `tools/localize_zone.py` from `strings/codol_english_zombies_base/strings.tsv` (identical) |
| codol_english_ui | codol-classic (zzVertigo/codol-classic) launcher releases r22-r30: method transplants on the English codo.gfx; the original translation has no source |
| codol_english_ui_extra | codol-classic `deploy/client-patches/revive-token-mode/build_token_mode.py` |
| codol_loading_mode_names | codol-classic `launcher/tools/build_loading_mode_names.py` (r21) |
| codol_zm_deathmachine_audio | codol-classic `deploy/client-patches/build_deathmachine_audio.py` |
| 000_codol_party_modes | codol-classic branch `codex/party-infected-20261017` (r22) |
| codol_infected_cyborg_postgfx_mp | codol-classic branch `codex/party-infected-20261017`, `build_infected_cyborg_assets.py` |
| voice_ui_mp, controller_ingame | this repository: `actionscript/` (formerly `voice_ui/`, built by build_voice_ui.py / build_ingame_ui.py into `patch/`, now folded into build_zones.py); decompiled from `patch/*.ff` of 3317112 |
| usermaps_patch, usermaps_ui_mp | the game folder's `usermaps\build_usermaps_patch.py` (now superseded by the sources here) |
| patch_common_gametype_zm | `CodolSoundDumper zm-pap-patch` (CODOL-AssetExplorer + `pap_patch_staging\tool_src`) over the stock zone |

### Known gaps
- Asset types without a codec stay raw blobs: images, materials, techsets, shaders, sounds (alias lists with their
  loaded PCM), models, xanims, weapons, fx. Stringtables with back-referenced cells (the PaP zone's) are blobs too.
- patch_common_gametype_zm is the stock zone's blobs (taken from the install by sha1) plus 959 committed blobs. It was
  compressed by .NET's zlib; this builder uses Python's, so the file differs from the shipped one although the zone
  image is identical.
