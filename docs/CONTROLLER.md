# Controller support for C1 (branch `feat/controller`)

Gamepad play for codoMP_client_shipRetail.exe 1.1.1.13, added by Kernel33 (`src/controller/controller.c`, included by
`src/kernel33.c` after `src/audio/voice.c`, installed by `InstallController()` in `TakeOver`). Every patch is
byte-checked against the running exe; a mismatch is logged to `Kernel33.log` and that part stays off.

Build: `PATH=/c/msys64/mingw32/bin:$PATH sh scripts/build-controller.sh` writes `build/Kernel33_controller.dll` and copies
it nowhere. Kill switch: `+set gpad_enabled 0` on the command line installs nothing; `gpad_enabled 0` at run
time stops all pad input.

## 1. What C1 still has

C1 is IW4 PC. The console gamepad layer (GPad_*, CL_GamepadMove, gamepad keys, `bindgp`) is not in it, and
neither is XInput. What survives:

| what | where | use |
|---|---|---|
| IW4 aim-assist module (PC subset) | `0x401190`..`0x402DB0`; aaGlob `0x1501A80`, stride `0xE60` | phase 2 aim assist |
| AimAssist_Init | `0x4012D0` (called `0x450F55`), loads `aim_assist/view_input_%d.graph` (string `0xC2FE84`) | the view-input response graphs IW4 uses for sticks |
| AimAssist_UpdateMouseInput (lock-on for the mouse) | `0x4020D0`, called from CL_MouseMove `0x4903A4` when byte `0x1501B30` is set | its AimInput/AimOutput frame is the one CL_GamepadMove would fill |
| likely AimAssist_UpdateScreenTargets / debug overlay | `0x401C90` (called `0x478C27`), `0x402DB0` (called `0x478BFB`), both from CG_DrawActiveFrame | screen targets for slowdown / auto-aim |
| gamer-profile layout | table `0xC3D860`: autoAim, invertedPitch, viewSensitivity, gpadButtonsConfig, gpadSticksConfig, gpadEnabled | data layout only, no code reads it for input |
| menu expression | "AttachedControllerCount is only valid on platforms that support gamepads." `0xC67F40` (`0x5E4C98`) | the old UI's controller query, disabled |
| vehicle stick dvars | `2255` vehHelicopterRightStickDeadzone, `2264` vehHelicopterYawOnLeftStick (helicopter physics) | unrelated to input |
| unused keynums | IW4's gamepad codes 1-6, 14-23, 28-31 have no names in C1 and PC input never produces them | the pad's keys |

No `gpad_*` dvars, no `aim_*` dvar names (C1's dvar names are numeric; the aim-assist dvars are among them but
were not mapped here), no DirectInput joystick code in the game (the IDirectInputJoyConfig string at `0xC1AAC0`
is library code).

## 2. Where C1 builds the usercmd

All in the client module, IW4 layout. `cl` (clientActive) = `[0x16FC318] ^ 0x9056BBC8`.

- **CL_CreateNewCommands** `0x490F90` → **CL_CreateCmd** `0x490D50` (eax = usercmd out, arg localClientNum):
  clears the 0x28-byte usercmd, then
  - CL_AdjustAngles `0x48F8E0` (keyboard turning, +left/+right/+lookup),
  - CL_CmdButtons `0x490430` (kbuttons → `cmd+4` button bits; tests keyCatchers at `0x4905C5`),
  - CL_KeyMove `0x48FB60` (`cmd+0x1A` forwardmove, `cmd+0x1B` rightmove),
  - **CL_MouseMove** `0x490010` (eax = lc; `cmd`, float frametime), call site `0x490E2F`,
  - pitch clamp against the angle saved at entry, then CL_FinishMove `0x490630` (viewangles → cmd angles).
- CL_MouseMove reads CL_GetMouseMovement `0x48FF40` (cl_mouseAccel `972`), applies m_yaw/m_pitch
  (`0x16F8080` / `0x16F8084`) and sensitivity, clamps to cgameMaxYawSpeed / cgameMaxPitchSpeed (`cl+0x3458` /
  `cl+0x3454`, deg/s; turrets and vehicles), and writes **viewangles: pitch `cl+0x34AC`, yaw `cl+0x34B0`**.
  It skips when `cl+0x14` (pm_flags) has `0x800`. Then the aim-assist call above.
- kbuttons: `0x16F4DE0 + lc*0x258`, 0x14 each (`down[2], downtime, msec, active, wasPressed`).
- **Button bits measured**: `+attack` 0x1; `+speed_throw` (ADS) 0x800 | 0x100000.
- Frame time: `cls.frametime` ms at `0x6C45098` (C1's aim code multiplies it by 0.001).
- Per frame: **IN_Frame** `0x62DA90` (from CL_Frame `0x4978D5`, front end included) → IN_MouseMove `0x62D990`
  (GetCursorPos delta, `0x48FD40` mouse event; Kernel33's raw mouse hooks `0x62DA03`).

## 3. Keys and binds

- Window messages → WndProc `0x633590` → **Sys_QueEvent** `0x62EF60` (eax = time or 0; type, value, value2,
  ptrLength, ptr; SE_KEY = 1). It holds critical section `0x2B62C60`, as do its consumers `0x62F030` /
  `0x62F2A0`, so another thread may queue events.
- Com_EventLoop (`0x5963F0`) → **CL_KeyEvent** `0x493240` (lc, key, down): `playerKeys` at `0x16F515C + lc*0xD28`,
  `keys[k]` = {down, repeats, binding} 12 bytes. keyCatchers `0x16FC2E0`: 0x01 console, 0x08 location
  selection, 0x10 UI, 0x20 chat. Escape opens/closes menus through UI mode `0x83DA993`. With no catcher the
  binding runs through `0x491580` (eax = lc; index, key); the key-up path (`0x4934E5`) runs index+1 for
  "+" commands.
- **Binds are indices**, not strings: the command-name table `0xCD62C0` has 86 entries (`+attack` 1, `+melee` 3,
  `+frag` 5, `+smoke` 7, `+tact` 9, `+breath_sprint` 11, `+usereload` 13, `+speed_throw` 15, `+actionslot 1..5`
  17..25, `+stance` 27, `+gostand` 29, ..., `+activate` 53, `+reload` 55, `+prone` 57, `+toggleads_throw` 61,
  `+sprint` 63, `+scores` 65, `togglemenu` 71, `weapnext` 72, `weapalt` 73, `weapprev` 77, `toggletalk` 79,
  `togglecrouch` 80, `toggleprone` 81, `toggleads` 84, `leaveads` 85; voice.c adds `+talk` 87). A bind can only
  name one of these.
- Key names: Key_StringToKeynum `0x492A20` (also "0x1F"-style hex), Key_KeynumToString `0x492AF0` (edi =
  display names), Key_WriteBindings `0x492FD0`. Tables `0xCD5C88` ({name, keynum}, TAB..COMMAND) and
  `0xCD5F90` (display, KEY_TAB...). Sys_Milliseconds `0x6326A0`.

## 4. How the Scaleform menus take input

The Scaleform front end (lobby, rooms, options, end-of-match) is driven by window messages: the WndProc offers
messages to Scaleform first, and its cursor comes from GetCursorPos (import slot `0xB423A4`). A posted
`WM_MOUSEMOVE` / `WM_LBUTTONDOWN` / `WM_LBUTTONUP` / `WM_KEYDOWN VK_ESCAPE` to the game window works exactly
like the real device (voice.c's `k33mouse` found the same). keyCatchers is 0x10 in the front end; outside a
match `connstate` (`0x16FC2E8`) is not 9. C1 treats a window that is not in the foreground as inactive
(GetForegroundWindow / GetActiveWindow, slots `0xB423F4` / `0xB42454`). The old IW menu path also still exists:
mouse via `0x5F3900`, keys via `0x4931E0` / UI_KeyEvent `0x5F3760`.

## 5. What phase 1 does

- **Backend: SDL2 2.32.10 GameController**, linked statically from the official
  `SDL2-devel-2.32.10-mingw.tar.gz` (i686 `libSDL2.a`, SHA-256
  `83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2`; `scripts/build-controller.sh` fetches it into
  `third_party/` and checks it). Link line:
  `-Ithird_party/SDL2-2.32.10/i686-w64-mingw32/include ... -Lthird_party/SDL2-2.32.10/i686-w64-mingw32/lib -lSDL2
  -lsetupapi -lcfgmgr32 -lversion -loleaut32 -luuid ... -limm32 -lgdi32 -lole32 -lwinmm` (no SDL2.dll).
  Xbox (XInput), PlayStation 4/5, Switch Pro and other pads in SDL's mapping database; hot-plug; the first
  game controller is used. `SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC)` only, on its own thread; hints:
  `JOYSTICK_ALLOW_BACKGROUND_EVENTS 1` (focus is decided by Kernel33), `JOYSTICK_RAWINPUT 0` (no
  RegisterRawInputDevices beside Kernel33's raw mouse), `JOYSTICK_WGI 0`, `TIMER_RESOLUTION 0`,
  `GAMECONTROLLER_USE_BUTTON_LABELS 0` (Nintendo pads by position), `JOYSTICK_HIDAPI 1`. No video subsystem, so SDL
  never touches the game window, keyboard, mouse or IME. If SDL_Init fails, the thread polls XInput
  (xinput1_4, then xinput9_1_0, then xinput1_3) instead.
- **Input thread, ~1 kHz**: `SDL_PumpEvents` + `SDL_PeepEvents` limited to joystick/controller events, the rest
  flushed. Every button or trigger change (triggers past `gpad_button_deadzone`, with 30 % hysteresis) is
  handled at once:
  - in a match with no catcher: an **SE_KEY event through Sys_QueEvent** with the pad keynum, so it reaches
    CL_KeyEvent with C1's timestamp and in order with the keyboard, and a tap shorter than a frame is kept
    (verified with a 1 ms tap);
  - in menus (keyCatchers 0x10, or not in a match): A = left mouse button at the cursor, B / Start = Escape,
    posted to the game window;
  - console or chat line open: only B / Start (Escape).
  A button held across a mode change is ignored until released, and whatever the old mode was told is released.
- **Sticks**: published by the thread and sampled once per usercmd in the CL_MouseMove hook (`0x490E2F`, after
  C1's own mouse code), so mouse and pad add up:
  - left stick → `forwardmove` / `rightmove` with IW4x's scale (full diagonal stays full speed), radial
    deadzone `gpad_stick_deadzone_min` / `_max` (IW4's GPad_ConvertStickToFloat);
  - right stick → viewangles: response curve `|v|^gpad_view_curve`, turn rate `gpad_turnrate_yaw` /
    `_pitch` (IW4's aim_turnrate defaults 260 / 90 deg/s), lerped to the `_ads` rates over 0.15 s while the
    usercmd has the ADS bit 0x800, times `gpad_view_sensitivity`, capped by cgameMax*Speed, IW4's turn-rate
    acceleration (`gpad_view_accel` deg/s², rising only), invert with `gpad_invert_y`; nothing while
    pm_flags has 0x800.
- **Menu cursor** (IN_Frame): the left stick moves the cursor (`gpad_menu_cursor_speed` px/s at full
  deflection, quadratic), with SetCursorPos while the game is in the foreground. It snaps to the movies'
  controls (magnetism, D-pad / flick jumps, settling): [PADSNAP.md](PADSNAP.md), `src/controller/padsnap.c`.
- **Keys**: BUTTON_A, BUTTON_B, BUTTON_X, BUTTON_Y, BUTTON_LSHLDR, BUTTON_RSHLDR, BUTTON_START, BUTTON_BACK,
  BUTTON_LSTICK, BUTTON_RSTICK, BUTTON_LTRIG, BUTTON_RTRIG, DPAD_UP/DOWN/LEFT/RIGHT (IW4 keynums 1-6, 14-23).
  Both key-name tables are copied with these appended (`0x492AAC`, `0x492AB2`, `0x492B5B`, `0x49303C`,
  `0x493042`, `0x492B52`), so `bind BUTTON_A +gostand` works and is saved in `players\config_mp.cfg`. The
  display names ("Pad A", "Pad RT", ...) are what C1's HUD hints print: the spectator line reads
  "Pad RT 或者 鼠标左键 - 下一位玩家" and the equipment slots show "Pad RB" / "Pad LB" with no further work.
- **Default binds**, applied 8 s after start and whenever no pad key has any bind (C1's first-run key-layout
  screen and the controls defaults run `unbindall`): RT `+attack`, LT `+speed_throw` (ADS), A `+gostand`,
  B `+stance`, X `+usereload`, Y `weapnext`, LB `+frag`, RB `+smoke`, L3 `+breath_sprint`, R3 `+melee`,
  Back `+scores` (PlayStation touchpad click too), Start `togglemenu`, D-pad `+actionslot 1..4`. `+talk`
  (voice) is bindable to any pad key. `gpad_defaultbinds` re-applies all of them.
- **The real pad only drives a foreground game window.** A background client never reacts to it.

### Dvars (archived unless noted)
| dvar | default | |
|---|---|---|
| `gpad_enabled` | 1 | 0 = no pad input at all (`+set gpad_enabled 0` installs nothing) |
| `gpad_stick_deadzone_min` | 0.2 | radial deadzone at the centre |
| `gpad_stick_deadzone_max` | 0.01 | deflection treated as full |
| `gpad_view_sensitivity` | 1.0 | scales the turn rates (0.1..5) |
| `gpad_invert_y` | 0 | |
| `gpad_view_curve` | 2.0 | response exponent (1 linear) |
| `gpad_turnrate_yaw` / `_pitch` | 260 / 90 | deg/s at full deflection |
| `gpad_turnrate_yaw_ads` / `_pitch_ads` | 90 / 55 | while aiming down sights |
| `gpad_view_accel` | 1200 | deg/s²; 0 = instant |
| `gpad_button_deadzone` | 0.13 | trigger press threshold |
| `gpad_menu_cursor_speed` | 1100 | px/s |
| `gpad_glyphs` | 0 | what button hints show (section 14): 0 automatic, 1 Xbox 360, 2 PS3, 3 Wii U, 4 generic, 5 text names only, 6 keyboard / mouse |
| `k33_glyphinfo` (not archived, read only) | "" | what hints show now: `family\|device\|textOnly\|glyphs` (section 14) |
| `k33_glyphSerial` (not archived, read only) | 0 | counts every change of the family, device or text-only state in `k33_glyphinfo` (section 14) |
| `gpad_debug` (not archived) | 0 | 1 state twice a second, 2 also every button / click |

### Commands
`gpad_status` (backend, pad name, mode, every pad bind), `gpad_defaultbinds`, `gpad_ksnext`, `gpad_ksuse`,
`gpad_lookpreset`, `gpad_swaptriggers`, `gpad_glyphdump`.

Test commands are registered only on a test client (`+set k33_cmdFile 1`) or with `+set gpad_testcmds 1` on
the command line: `gpad_fake <lx> <ly> <rx> <ry> [lt rt [buttons [ms]]] | off` (synthetic pad: sticks -1..1,
triggers 0..1, XInput button bits A 0x1000 B 0x2000 X 0x4000 Y 0x8000 LB 0x100 RB 0x200 Start 0x10 Back 0x20
L3 0x40 R3 0x80, D-pad 1/2/4/8), `gpad_testmouse`, `gpad_cursor`, `gpad_sfkey`, `gpad_aadump`,
`gpad_notifytest`, `gpad_looktest`, `gpad_glyphtest`.

### Test clients
With `+set k33_cmdFile 1` (a test client beside the player's game) the real pad is never read and never
opened (SDL only lists it: HIDAPI opens can change a PlayStation pad's report mode), and while `gpad_fake`
drives the menu cursor, C1's GetCursorPos / GetForegroundWindow / GetActiveWindow slots report the virtual
cursor and the game window; SetCursorPos is a no-op for the whole session, so the test client can never move
the player's pointer.

## 6. Verified (2026-10-20, k33-devtest copy, synthetic pad)

A pad (Xbox 360 controller) was connected but belonged to the user's session, so everything ran through
`gpad_fake` on a background test client, with PrintWindow captures:
- SDL 2.32.10 started in-process and enumerated the pad ("Xbox 360 Controller"), without opening it.
- Menus: left stick moved Scaleform's cursor at the computed speed; A clicked (first-run key layout OK, skip
  tutorial, Multiplayer, matchmaking, Ready); B went back (lobby → mode select); a 1 ms A tap gave both
  click-down and click-up.
- In a match (mp_killhouse_sh on the local server): RT ran `+attack` (usercmd 0x1; spectator next player),
  R3 ran `+melee` (stop following), LT gave usercmd 0x100800; left stick forward gave forwardmove 127, 0.6/0.6
  gave 104/104; right stick full right for 1 s turned yaw by 232.96° (model: 260 deg/s with the 1200 deg/s²
  ramp ≈ 232°).
- Binds saved in `config_mp.cfg` as `bind BUTTON_A "+gostand"` etc.; `unbindall` → defaults back within 2 s.

**Still needs a real pad:** feel of the defaults (deadzones, curve, turn rates), PlayStation / Switch mappings
and hot-plug, Start in a match (togglemenu; whether the in-match Scaleform menu sets keyCatchers 0x10 so the
pad switches to cursor mode), the menu cursor on a focused window (SetCursorPos path), trigger thresholds.

## 7. Plan

**Phase 2**
1. Aim assist on C1's own module: fill AimInput like IW4x's CL_GamepadMove and add IW4x's
   AimAssist_ApplyTurnRates / ApplyLockOn / CalcSlowdown on aaGlob (`0x1501A80`), using C1's screen targets
   (`0x401C90`) and view-input graphs (AimAssist_Init `0x4012D0`); map C1's numeric `aim_*` dvars
   (BG/aim registration) or add `gpad_slowdown_enabled` / `gpad_lockon_enabled`; respect a server switch.
2. Real menu navigation: D-pad / left stick focus movement and A = activate, through Scaleform focus
   (codo.gfx ActionScript, as voice_ui does) instead of the cursor; scoreboard scrolling; right stick scroll
   (WM_MOUSEWHEEL); Options > Controls rows that capture pad keys (the bind screen's key capture runs under
   the UI catcher, which phase 1 does not feed).
3. Rumble: SDL_GameControllerRumble (haptics initialised) on damage (cg damage events), firing and explosions;
   `gpad_rumble`.
4. Hold-to-use (`gpad_use_hold_time`, IW4x's Player_UseEntity split of X between use and reload), sprint
   toggle on L3, location selection (killstreak maps: stick moves the marker, A/B confirm/cancel).
5. Akimbo trigger swap (IW4x: LT = left weapon) and vehicle / remote-missile steering (`0x490B40` path).

**Phase 3**
1. Button glyphs: images instead of "Pad A" text in hints (fonts/materials in a patch zone), Xbox / PlayStation
   styles from SDL_GameControllerGetType.
2. Gyro aiming (SDL sensors) and touchpad.
3. Per-pad profiles / `gpad_sticksConfig` presets (southpaw, legacy), Options > Controls > Controller page.
4. Split-screen / second local client (lc 1) only if C1 ever runs it.

## 8. Phase 2 work in progress (2026-10-20, built, not yet run in a match)

Compiled into `Kernel33_controller.dll`; in-game testing stopped because the main install's
`client_shipRetail\chinese\patch_common_gametype_zm.ff` became an unsigned IWffu100 build at 17:50 (other
work), which fails C1's client pure check (0x617FA0, "WIN_FILE_CORRUPT") on every launch from that folder.

- **Binds (CTL_BINDS_VERSION 2)**: LB `+smoke` (C1's tactical, keyboard Q), RB `+frag` (lethal, G),
  D-pad up `+tact` (equipment, 4), left `weapalt` (attachment / underbarrel, 3), down `gpad_ksnext`, right
  `gpad_ksuse`, X `+reload`, BUTTON_X_HOLD `+activate`. Configs still holding a phase-1 default for a key
  are moved to the new one once (`gpad_binds_version`); changed keys are kept.
- **Killstreaks**: C1 keeps them in action slots 1..5 (`_killstreaks.gsc` setActionSlot; playerState
  actionSlotType[5] at +0x544). `gpad_ksnext` selects the next filled slot, `gpad_ksuse` presses
  `+actionslot <selected>`; both are added to the bind table as 89 / 90 (after voice.c's +talk) and as
  console commands.
- **Hold to use**: X shorter than `gpad_use_hold_time` (250 ms) is BUTTON_X (reload), longer is
  BUTTON_X_HOLD (`+activate`: weapon pickups, equipment reclaim, every "Press [Use]" prompt). Client-side
  only; no server change needed. Hints for `{+activate}` read "Hold X" on the pad.
- **Pause menu** (Start / Esc in a match). It is not codo.gfx but **scaleform/default/codoingame.gfx**
  (common_mp / patch_common_mp; document class MainPause, view PauseMenuView), loaded into the movie slot
  0x810C2A4 by UI mode 2 (0x6B786E). C1 routes in-match keys to the HUD movie (0x6B7F80 picks 0x810C50C while
  byte 0x6C44D6C is set), so stock C1 gives this menu no keyboard at all. Whether it is up: C1's menu-type
  word **0x6C44D64 == 3** (MenuType.PAUSE; measured 3 open, 2 closed; keyCatchers' UI bit flickers with the
  pointer). While it is up the pad drives it, no cursor: D-pad / left stick (0.55, hysteresis 0.4, repeat
  350 / 140 ms) and A are queued (input thread) and handed by IN_Frame straight to the movie's HandleEvent
  (vtable +0x8C, event {5/6, key}) as private key codes 240 up / 241 down / 242 left / 243 right / 244 select;
  B / Start are Escape (PauseMenuView's own back / close). `zones/controller_ingame/rawfile/scaleform/default/codoingame.gfx.gfxpatch/as/MainPause.as` (the 79-line document
  class) moves focus to the nearest visible clickable object (C1's BaseButton sets buttonMode), sends it
  MouseEventEx MOUSE_OVER / ROLL_OVER (its own hover look, no extra frame); select sends MouseEventEx MOUSE_DOWN / UP / CLICK
  (C1's BaseButton reads `cEvt as MouseEventEx`; a plain MouseEvent is dropped). Shipped as
  `client_shipRetail\patch\controller_ingame.ff` (`build_zones.py controller_ingame`), a non-*_ui_mp patch zone, so it loads
  ahead of code_pre_gfx and its codoingame.gfx wins over patch_common_mp's. Recompiling the 1168-line
  PauseMenuView crashed C1 (AV 0x92F6F3) when the menu opened, and recompiling codo.gfx's BaseMain blanked the
  front end, so only MainPause is replaced. Front-end menus keep the stick cursor.
  Verified in a match: Start opens it, left stick down / right / up walks loadout -> sensitivity -> Return to
  game (hover state), A there closes the menu and the pad is back in game mode; B closes it.
- **Button names by last-used device**: Key_GetBindingNames' call to Key_GetCommandAssignment (0x49393E)
  returns only the active device's keys (pad: buttons, triggers or a stick past 0.45; keyboard / mouse:
  any key or mouse button through Sys_QueEvent, mouse look of 0.5 deg in 300 ms, or the pointer moving 12 px).
  Pad names by SDL controller type: Xbox (A, B, X, Y, LB, RB, LT, RT, LS, RS, Start, Back), PlayStation
  (Cross, Circle, Square, Triangle, L1, R1, L2, R2, L3, R3, Options, Touchpad), Nintendo (positional);
  `gpad_glyphs` overrides the family and, since section 14, the device as well. C1's zones ship no button icon materials (only `dpad_*` killstreak icons), so
  Scaleform shows these as text; hints drawn by C1's text renderer get icons (section 9).
- **Aim assist (pad only)**: C1's AimAssist_UpdateScreenTargets fills aaGlob 0x1501A80 (targets at +0x134,
  0x34 each, count +0xE34, view origin / angles +0xD0 / +0xDC). Slowdown 0.4 hip / 0.5 ADS in a 90x90 box,
  lock-on 0.6 (needs pad input past 0.05), ADS snap over 0.25 s in a 160x120 box (IW4 console values from
  iw4mp AimAssist_RegisterDvars). Applied to the right stick's own delta after C1's CL_MouseMove; off unless
  the pad is the last-used device and no keyboard / mouse cooldown runs: any in-game key, mouse button,
  mouse turn past the device threshold (0.5 deg in 300 ms) or pointer move in a match turns assist off for
  `gpad_aimassist_kbm_cooldown` seconds (default and minimum 20, may only be raised), restarted by every new
  use and kept even if the pad is used meanwhile. Keys and buttons count only in game mode (not menus, chat,
  console, front end); Kernel33's own posted keys and clicks never count. `gpad_status` prints the state
  ("aim assist off, KBM cooldown 14 s"). C1's mouse-path helper 0x4020D0 is
  IW4 PC's melee lunge, unchanged and no aim assist. `gpad_aimassist` (0..1), `gpad_aimassist_range`,
  `gpad_aimassist_snap`.
- **Server visibility**: userinfo dvar `gpad` (flag 0x200): 0 keyboard / mouse, 1 pad with aim assist,
  2 pad during the keyboard / mouse cooldown (no assist).
- Test-only: `gpad_testmouse <dx> <dy> <ms>` (mouse counts into C1's accumulator), `gpad_aadump`,
  `gpad_sfkey <code> [hud]` (one key code straight to a movie).

### Verified in matches (2026-10-20, devtest, synthetic pad)
- Pause menu: see above (navigation, select, close, back to game mode).
- Tap X: BUTTON_X -> bind 55 `+reload`; hold X 250 ms: BUTTON_X_HOLD -> bind 53 `+activate`, usercmd 0x8 (use).
- D-pad down -> `gpad_ksnext` (bind 89; "no killstreak ready" with none earned), D-pad right -> `gpad_ksuse`
  (bind 90). Selecting / calling a real streak still needs one earned in a match.
- Aim assist with a bot in front (C1 screen target ent 7, 395 units): "slowdown", LT -> "ads-snap" (view
  turned 6.25 -> 12.58 deg onto the target), stick -> "slowdown+lock-on"; with `gpad_testmouse` mouse movement
  during pad ADS: assist OFF, device kb/mouse. aaGlob view origin / angles are +0xDC / +0xE8 (fixed); targets
  +0x134, count +0xE34; the regions scale with C1's own tweakables (+0xC0 / +0xC4 = auto-melee 320x240).
- Key names follow the last-used device (Options > Controls showed RT/LT/X/Y/RS/"Hold X"/RB/LB/D-pad for the
  pad, MOUSE1/R/E/F/G/Q/4/3/TAB after a key press); the HUD shows "RB" over the lethal grenade.

## 9. Phase 3: button icons (2026-10-20)
Pad keys in hints and prompts are drawn as icons, IW4's way: C1's text renderer still understands IW4's
inline icon escape (`^`, 1 or 2, width, height, a raw 4-byte `Material *`; 0xAAB26E / 0xAAB8B2 step over it,
0xAAA7D0 draws it at `((size - 16) * fontHeight + 16) / 32` px). Source: `src/controller/controller_glyphs.c`.

- **Art**: Xelu's FREE Controllers & Keyboard Prompts (Nicolae "Xelu" Berbece / Those Awesome Guys,
  https://thoseawesomeguys.com/prompts/, public domain under CC0 1.0; attribution not required, given with
  thanks): Others/Xbox 360, Others/PS3, Others/WiiU (positional: our bottom button is the Wii U B), and for the
  generic set Others/Positional Prompts (face buttons) plus Steam Deck (the rest). Hold-X is the set's left face
  button inside a ring; the killstreak arrow is Others/Arrows/Directional_Arrow_Straight, recoloured.
- **Zone**: the icons are real C1 assets in the patch zone `controller_glyphs_postgfx_mp.ff`
  (`zones/controller_glyphs_postgfx_mp/`, built by `scripts/build_zones.py`; install it in `client_shipRetail\patch\`
  with the DLL). Per set (xbox360, ps3, wiiu, generic) and glyph (a b x y lb rb start back ls rs dpad_up
  dpad_down dpad_left dpad_right lt rt x_hold arrow): an image `k33pad_<set>_<glyph>`, 64x64 A8R8G8B8 with 7 mip
  levels, its pixels in the zone (`image/<name>.png` + `.mips.png` in the sources), and a material of the same
  name copied from common_mp's `hud_status_dead` (HUD icon: technique set `,2d`, sort 0x30, one colorMap,
  sampler 0xE2, its alpha-blend state bits). A `*_postgfx_mp` patch zone is appended to C1's code_post_gfx list
  (0x5984D2, alloc flags 1, kernel33.c): it loads once the renderer is up and stays loaded in the front end and
  in matches (the `*_ui_mp` list is freed on match entry; images ahead of code_pre_gfx crash).
- **Lookup**: Material_RegisterHandle (0xA78170) by name on the main thread, once the zone has been handed to the
  loader and the front end is up; every material must come back under its own name with its image's texture
  made. Without the zone (or with a material missing) the icons stay off, hints keep the family's text names
  (below), and the reason is logged once (`controller: button icons off, ...`) and printed by `gpad_glyphdump`.
- **Slots**: the escape carries the material pointer as four raw bytes inside a C string, so each glyph has a
  slot (a copy of its set's 0x60-byte zone material header, pointing at the zone's technique set, texture table
  and image) in a 64 KB block whose address bytes are letters or digits (0x4B4B0000 first, then every other
  combination): the escape has no 0, `^`, `%`, `&` and survives every string copy (Key_GetBindingNames strncpy's
  0x7F bytes; the escape is 8). At the icon drawer's two call sites (0xAAB320, 0xAAB92D, render thread) the set
  in use is copied into the slot, so icons switch live. The hook sets our icons' vertex colour to white (C1
  multiplies text icons by the text colour, and hint key names are yellow), so the art keeps its own colours;
  the black shadow pass stays black.
- **Names**: Key_GetBindingNames' two Key_KeynumToString calls (0x49397C, 0x4A19A1) return the icon escape
  for a pad key. Scaleform (AS key names, 0x5B080D: Controls screen, pause menu, Scaleform HUD) and GSC
  (0x55A8B6) keep the text names: Scaleform has no IW4 icon escape, so it keeps the family names below.
- **Set**: `gpad_glyphs` 0 by SDL controller type (Xbox 360 / One / Series and XInput -> Xbox 360; PS3/4/5 ->
  PS3; Switch Pro / Joy-Cons / a name with "Wii U" -> Wii U; anything else -> generic), 1 Xbox 360, 2 PS3,
  3 Wii U, 4 generic, 5 text names only, 6 keyboard / mouse (family by SDL type, as 0). Text names (Scaleform,
  mode 5) follow the same family; generic text is "Button South/East/West/North", L1/R1/L2/R2. Since section 14
  the values 1..4 and 6 also fix which device's bindings a hint names.
- The spectator hint asks for `+toggleads_throw` before `+speed_throw`; with the pad in use a keyboard-only
  `+toggleads_throw` no longer wins, so the hint shows LT (or L2 / ZL) instead of "MOUSE2".
- If any check fails the icons stay off and every hint keeps the text names; `gpad_glyphdump [material]`
  prints the state and the template's bytes. Test-only: `gpad_glyphtest [caller|off]` draws every icon and a
  sample hint each frame (it hooks R_AddCmdDrawText 0xA7EF80 and the end-of-frame call 0x60AB19 only while on).

### Verified (2026-10-20, devtest, synthetic pad)
- Front end with `gpad_glyphtest`: all four sets render, `gpad_glyphs 1..4` switches them live, 5 gives text.
- In a Killhouse match: the cgame spectator hint showed the RT / LT icons for Xbox 360, R2 / L2 for PS3,
  ZR / ZL for Wii U, drawn white beside the hint text.

## 10. Killstreaks on the pad (2026-10-20, built; in-match test pending)
- **Location selection** (airstrike, precision strike, mortar, any `beginLocationSelection` streak):
  - **How C1 does it:**
    - At 0x490DF6, while keyCatchers bit 8 is set, CL_CreateCmd calls 0x490700(cmd, lc). That step:
      - moves the selector (cg+0xC8B08 / +0xC8B0C, 0..1) with the mouse;
      - rotates the heading (cg+0xC8B10, degrees) while MOUSE2 is held on a directional streak (ps.locationSelectionInfo +0x230 bit 7);
      - writes usercmd 0x20000 (confirm) / 0x40000 (cancel) plus the heading byte (+0x23).
    - It writes those bits when CL_KeyEvent has left state 1 (MOUSE1) or 2 (Escape) at 0x16F5D5C + lc*0xD28.
    - The server's CODOL_LocationSelectionAction then notifies `confirm_location` (position, yaw).
  - **The pad hook** (CtlLocSelect, ahead of 0x490700):
    - The left stick moves the selector at `gpad_locsel_speed` map widths per second (default 0.45). The response is quadratic, and speed doubles over 0.6 s of full push.
    - The right stick sets the heading.
    - A or RT sets state 1 and B sets state 2, the same path as the mouse click, so the server sees an identical usercmd.
    - Mode CTL_MODE_LOCSEL keeps A / B / RT from reaching their game binds; Start / Back still work.
- **Cycling**:
  - `gpad_ksnext` (D-pad down) walks the filled action slots (ps actionSlotType, cg+0x544) and wraps around. `gpad_ksuse` (D-pad right) presses `+actionslot <selected>`.
  - The selection snaps to the first filled slot when it is empty or used up.
  - C1's GSC has no selection of its own: each streak is its own `+actionslot N`, `codolRewardActionWaiter`. So no server change is needed.
- **HUD keys**:
  - The killstreak HUD names each slot by its `+actionslot N` key, and the pad has no per-slot key.
  - With the pad last used, the selected streak shows the D-pad-right key (use) and the others show D-pad down (cycle). They are drawn as icons, and change live with the device and the selection.
- **Fix (2026-10-20, after the user's mp_subbase_sh test):**
  - **Cause:** the user's `players/config_mp.cfg` had no DPAD_DOWN, DPAD_RIGHT or BUTTON_X_HOLD binds.
    - A Kernel33 build without `gpad_ksnext` / `gpad_ksuse` / BUTTON_X_HOLD loaded the config. This was the older DLL another session used for the shadow bisect.
    - C1 dropped the binds it could not resolve and wrote the file without them.
    - `gpad_binds_version` stayed 2, so nothing re-applied them.
  - **Effects:** D-pad right did nothing. The killstreak HUD kept keyboard 5–8, because no pad key was bound to gpad_ksuse / ksnext. Hold-X use was lost too.
  - **Fix:** a pad button that is unbound, and whose default command no other pad key carries, gets its default back after the config runs ("repaired binds lost by an older config load").
- `gpad_ksuse` falls back to the first filled slot. `gpad_ksnext` prints "Killstreak N selected - D-pad right calls it in" as the bold centre line (print channel 5; `gpad_notifytest <ch>` tests channels).
- With `gpad_debug 1`, Kernel33.log records:
  - `ksnext -> slot N (slots 1:t 2:t ...)` and `ksuse -> +actionslot N (bind I; slots ...)`, where t is actionSlotType at cg+0x544;
  - `HUD key lookup "+actionslot N": device, slot type, selected, keys`, once per 2 s per slot. This line also shows whether the HUD asks through Key_GetBindingNames.
- **Fix (2026-10-20, second user test):**
  - **Location selection never ran on the pad.** C1 calls its selection step with lc first, then cmd (it pushes cmd, then lc). CtlLocSelect declared them the other way round, so it read the cmd pointer as lc and returned on every frame. The stick, heading, confirm and cancel never reached the selector.
    - The server log for that session shows `mortar_mp refused` twice: once with userinfo gpad 1 and once with gpad 0. The mortar's use function returns false when the selection is cancelled or put away.
    - With the arguments fixed, the hook runs.
    - `gpad_debug 1` logs `location selection active: info, keyCatchers, pad, state, selector, heading, sticks` once a second while a selection is open. Confirm and cancel are always logged.
  - The "Killstreak N selected" centre line and "no killstreak ready" show only with `gpad_debug`. The HUD key icons show the selection.
  - **Icon placement:**
    - C1 lays out text in runs. The 0x6B7510 runs are measured by 0x6B7350 from a per-character advance table.
    - The icon drawer gets the pen x of the icon's slot, and the slot C1 then leaves is wider than the drawn icon. So the icon sat at the slot's left edge, with all the gap on its right.
    - The draw hook now moves it right by `gpad_glyph_xshift` × its drawn width (default 0.08, measured on devtest at text scales 0.35, 0.6 and 0.75 and on the spectator hint) to centre it.
- **Fix (2026-10-21, third user test):**
  - **Mortar points:** only the mortar's first point was placed with A. The pad's confirm stayed set, and C1 re-arms (cg+0xC8B14 = 0) only on a frame with state 0, which C1 itself sets on the key release (0x493710). The pad's press now lasts one frame.
  - **Back:** pad B, and Escape the same way, with a point placed takes that point back. The client marker at cg+0xC8B18 is set back to -1; C1 still sends its cancel. In rawfiles 78dbc61, `_mortar.gsc` `undoOrCancel` drops the last target on `cancel_location`, and closes the selection only when none is placed.
  - **Icon placement:**
    - R_TextWidth 0xA78580, which C1 uses to centre and right-align text (cursor hints, menu items, the HUD), skips "^digit" colour codes but not the inline icon code. It measured an icon as eight characters.
    - A test row with a bar at x + measured width × scale showed it: the bar ended far right of a line with an icon, and exactly at the end of plain text.
    - The hook measures the text without the icon codes and adds each icon's slot, (1 + 2 × `gpad_glyph_xshift`) × its drawn width. Bars now end at the line's end in both cases.
  - **Selection arrow:** the killstreak HUD's selected slot is pseudo key 29. It renders as a yellow arrow (Xelu's Others/Arrows/Directional_Arrow_Straight, recoloured when the textures were generated, glyph 18 in every set) followed by the gpad_ksuse key. In text mode it is "> name".

## 11. Movement parity, look feel, dive (2026-10-21)
- **Movement:**
  - C1's wire usercmd has no analog movement. It carries four direction bits, and the server rebuilds forwardmove/rightmove as ±127 (CoD4x_Server `codol_game.c`).
  - The stick's analog cmd therefore moved the server at full key speed in the octant its signs gave, while the client predicted the analog move and was corrected every snapshot:
    - a 10-degree drift became a 45-degree strafe on the server;
    - a half tilt ran at full speed on the server while the client predicted 0.38.
  - `gpad_move_digital 1` (default) maps the stick to the eight ±127 key combinations by its octant (6-degree hysteresis), once past `gpad_move_threshold` (0.3). Client and server then see exactly what W/A/S/D produce.
  - Walking at half speed is not possible on this protocol (the server ignores magnitude). Sprint and stance rules are the server's, on the same bits as the keys.
- **Look:**
  - dt is the QueryPerformanceCounter time between usercmds (`gpad_view_hires`); before, it was C1's whole-millisecond frametime.
  - The turn rate reaches `gpad_view_accel_start` (0.6) of the target at once, and only the rest ramps at `gpad_view_accel` (3000 deg/s²).
  - New defaults: deadzone 0.12, curve 1.7, pitch 120. A one-time migration (`gpad_look_version`) moves only archived values that still equal the old defaults.
  - Presets: `gpad_lookpreset snappy|classic|smooth`. Measurement: `gpad_looktest <rx> <ms>`.
- **Dive:** B pressed while sprinting (cg+0x15C ≥ [0xC845B8] and pm_flags 0x4000, C1's own test) runs `toggleprone`. C1 turns that into stance state 3, usercmd 0x800000, which is the server's dive. Otherwise B stays BUTTON_B (+stance: tap crouch, hold prone). Controlled by `gpad_dive`.

## 12. Options > Controller, microphone choice (2026-10-21)
- **The tab:**
  - `zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/views/OptionView.as` is C1's `codo.views.OptionView`. Its hidden "Other" tab (`myTabOther`) is shown as 手柄 and opens a panel built in code.
  - It is compiled by FFDec and spliced into the same codo.gfx in `voice_ui_mp.ff` (`scripts/build_zones.py`), so there is still one UI zone.
- **Page 手柄图示 (diagram):**
  - A line-art diagram for the pad family (glyph family: Xbox 360 / PS3 / Wii U / generic), pre-generated in `zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/views/k33_pad_diagrams.as.inc` (original line art; inserted at `// @K33DIAGRAMS@` when the class is compiled).
  - Each button has a live callout with what it does, read from `k33_padbinds`.
  - Settings rows: swap sticks (`gpad_swap_sticks`), swap triggers (`gpad_swaptriggers` swaps the two trigger binds), look feel (`gpad_lookpreset`), look sensitivity, invert Y, aim assist, deadzone, and dive with the crouch button (`gpad_dive`).
  - With no pad, the generic diagram shows with 未连接手柄 ("no controller connected"); `gpad_glyphs 1..4` forces a family.
  - Scaleform note: drawCircle / drawRoundRect stroke a line from the pen to their start, so the pen is moved first.
- **Page 按键设置 (bindings):**
  - Click an action, then press a pad button. The tab sets `k33_padcapture`; `src/controller/controller.c` catches the next pad button in menu mode, binds it, unbinds the action's other pad buttons, and sets `gpad_binds_custom 1`.
  - `gpad_binds_custom 1` stops the bind repair and the "no pad key bound" defaults.
  - Hold X binds BUTTON_X_HOLD. Triggers and sticks are not bindable.
  - 清除所选 clears an action (`k33_padclear`). 恢复默认 runs `gpad_defaultbinds`, which also resets `gpad_binds_custom`.
  - The dive follows whichever pad button is bound to +stance.
- **Published state:** `k33_padinfo` ("family|connected|name|forced|glyphs"; the fifth field, the `gpad_glyphs`
  value in effect, was added in section 14) and `k33_padbinds` ("A=cmd;...;HOLDX=cmd").
- **Microphone (Options > Audio > 语音, 输入设备):**
  - `src/audio/voice.c` lists the active capture endpoints and their friendly names (GBK, code page 936).
  - It publishes `k33_voiceInputList` ("Default;name;...") and `k33_voiceInputIndex`, and turns the dropdown's pick (`k33_voiceInputPick`) into the archived `k33_voiceInputDevice` (endpoint ID; empty = the Windows default communications device).
  - Capture reopens on the chosen device right away.
  - A missing device falls back to the default (logged) and is taken again when IMMNotificationClient reports it back.
- **Test clients:** a test client with a `k33patch\` folder next to its exe loads its patch zones from there, so a UI build can be tried while the shared `client_shipRetail\patch\` is in use.

## 13. Killstreak control on the pad (2026-10-23)
What each killstreak's control mode does with the pad, measured on the k33-devtest copy (mp_rust, synthetic pad,
external reads of clientActive / cg / the usercmd ring at clientActive +0x34B8, 256 x 0x28):

| mode | killstreaks | path in C1 | pad before | pad now |
|---|---|---|---|---|
| remote control | Predator missile | CL_CreateCmd 0x490DBE: pm_flags 0x80000 -> 0x490B40 only | **ignored**: 0 of 770 usercmds carried a steering byte, the missile's angles never changed | sticks steer it |
| linked gunner view | AC-130, chopper gunner | normal path, playerState +0x1EC = link entity | turned at the full on-foot rate (256 deg/s), aim assist on | FOV-scaled turn rate, no aim assist |
| map location | precision / stealth / Harrier airstrike, mortar | 0x490700 (keyCatchers 8) | selector fine; heading a quarter turn off | heading follows the stick |
| vehicle | RC-XD | normal path | left stick throttle, right stick steers (unchanged) | unchanged |
| carried / thrown | sentry, SAM turret, care packages | on foot | unchanged | unchanged |

- **Predator missile.** While pm_flags has 0x80000 CL_CreateCmd skips everything, CL_MouseMove and Kernel33's
  stick hook included, and calls 0x490B40 (eax = lc, esi = cmd): +attack (boost) and two other kbuttons into
  the button bits, 0x200000, and the mouse into two signed wire bytes, pitch cmd+0x24 =
  clamp(my * m_pitch, -1, 1) * 127, yaw cmd+0x25 = clamp(-mx * m_yaw, -1, 1) * 127. The server turns the
  missile by rate * msec / 1000 * byte / -127 per command (C1 0x40EC50, CoD4x_Server
  `codol_remote_missile.c`), so a byte is a turn rate. `CtlRemoteMove` (call at 0x490DCF) adds the sticks after
  C1's mouse: the right stick, or the left one when it is pushed further; the look curve (`gpad_view_curve`),
  `gpad_view_sensitivity` x `gpad_remote_sensitivity` (new, default 1), `gpad_invert_y`, the mouse's
  directions. Being a rate, it is frame-time independent by construction (the server multiplies by each
  command's msec). Measured: stick 0.25 / 0.5 / 0.75 / 1 -> byte 5 / 31 / 73 / 127 (about 1.4 / 8.5 / 20 /
  35 deg/s), the same byte on every command at 60 fps, 144 fps and uncapped (about 250); half stick down for
  1.47 s at 60 fps pitched the missile 53.5 -> 65.9 deg (8.5 deg/s); RT = boost (buttons 0x200001). Mouse
  steering is unchanged and still adds; a mouse turn in the missile counts as keyboard / mouse use.
- **Linked gunner views** (playerLinkWeaponViewToDelta). C1's mouse is multiplied by the FOV sensitivity scale
  (clientActive +0x3450, CL_GetMouseMovement 0x48FFB1; 1.18 on foot, 0.82 chopper gunner, 0.65 AC-130); the pad's
  turn rates now are too in these views, as IW4's consoles do (full right stick: AC-130 256 -> 168 deg/s,
  chopper gunner 260 -> 211 deg/s). Aim assist is off whenever pm_flags has 0x80000 or playerState +0x1EC names an entity (`CtlRemoteView`).
- **Location selection.** The heading was a quarter turn off: C1's drag stores atan2(-dx, -dy) of the drag
  (map y down) and the arrow follows the drag; the hook used atan2(-dy, -dx), so stick up drew the arrow
  pointing left and stick right pointed it down. It is now atan2(-rx, ry): up 0 (arrow up), right 270, left 90.
  The selector already moved at the same speed at every frame rate (left stick 0.6: 0.137 map widths/s at 60,
  144, 500 and uncapped, under a pixel off a straight line); its step now uses the real time between frames
  (QueryPerformanceCounter, capped at 50 ms) like the look code, and the full-push boost counts that time.
  A / RT confirm, B takes back a mortar point, then cancels, as before.

## 14. Button-glyph override (`gpad_glyphs` 0..6)
The player's choice of what button hints show. An explicit choice overrides the detection **for display**; it
never changes what the game does with the input. The mapping is the pure function `CtlGlyphPolicy` in
`src/controller/controller_glyph_policy.h` (tested by `tests/controller_glyph_policy_test.c`); `controller.c` reaches it
through `CtlDisplayNow` / `CtlPadFamily` / `CtlDisplayPad`.

| `gpad_glyphs` | family shown | bindings a hint names | pad keys drawn as |
|---|---|---|---|
| 0 automatic (default) | detected pad type (SDL) | last-used device | icons |
| 1 Xbox 360 | Xbox | the pad's, always | icons |
| 2 PS3 | PlayStation | the pad's, always | icons |
| 3 Wii U | Nintendo | the pad's, always | icons |
| 4 generic | generic | the pad's, always | icons |
| 5 text names only | detected pad type | last-used device | text names |
| 6 keyboard / mouse | detected pad type | keyboard / mouse, always | icons |

- 0 and 5 are unchanged. 6 is new. A value outside 0..6 cannot be set (the dvar's range) and would act as 0.
- **Behaviour change for 1..4.** Before, 1..4 only chose the icon family and the device still followed the last
  input. Now they mean "always show this pad's buttons": a player who has `gpad_glyphs` 1..4 saved in
  `config_mp.cfg` sees pad icons while playing on keyboard and mouse. Set 0 for the old automatic behaviour.
- "Bindings a hint names" is the first choice only, as before: a command bound on the other device only still
  shows that key (`CtlCommandKeys`' second pass). So in mode 6 a command with only a pad bind still shows the pad
  button, as an icon of the detected family; in modes 1..4 a command with only a keyboard bind shows that key.
- With no pad connected the detected type is Xbox (`g_padType` 0), as before.
- **Display only.** The last-used device (`g_lastDevice`, `CtlSetDevice`) is untouched and still drives everything
  that is not a label: aim assist (`CtlAimAllowed`), the userinfo dvar `gpad`, the pointer watch, the menu cursor's
  snap scan (`padsnap.c`), `gpad_status` / `gpad_aadump`. With `gpad_glyphs 1` on keyboard and mouse there is no
  aim assist and the server sees `gpad 0`; with `gpad_glyphs 6` on the pad aim assist works and the server sees
  `gpad 1`. The readers of the display helper are `CtlCommandKeys` (the device, with its spectator-hint and
  killstreak-HUD special cases), `CtlApplyPadNames` (the family's text names), `CtlGlyphFrame` / `CtlGlyphPrepare`
  (the icon set), the published state below, and one line each in `gpad_status` / `gpad_glyphdump`.

### Published for the menus and the HUD
Set through C1's command path like `k33_padinfo`; "read only" is by convention (flags 0; Kernel33 never reads
them back).

- `k33_glyphinfo` (string): `family|device|textOnly|glyphs`, one digit each. family 0 Xbox, 1 PlayStation,
  2 Nintendo, 3 generic; device 0 keyboard / mouse, 1 pad; textOnly 0 / 1; glyphs = `gpad_glyphs` (0..6).
  Example: `1|1|0|2` = PS3 forced. Empty until the first client frame with dvars.
- `k33_glyphSerial` (int, 0..1000000000): 0 until the first publish, then +1 on every change of family, device
  or textOnly (it wraps to 1). It does not move when `gpad_glyphs` changes without changing those three
  (0 -> 6 while already on keyboard and mouse; `k33_glyphinfo`'s last digit still updates). Both are set by one
  command line, so a reader that sees a new serial also sees the matching `k33_glyphinfo`. The state is checked
  every client frame; C1 runs the `set` on its next command-buffer pass. A menu polls
  `Dvar_GetInt("k33_glyphSerial")` and re-queries its key labels when the number differs from the one it last saw.
- `k33_padinfo`: a fifth field, the `gpad_glyphs` value: `family|connected|name|forced|glyphs`. `forced` is still
  1 only for 1..4. OptionView.as splits on `|` and reads fields 0..3 by index, so a build of it that does not know
  the fifth field ignores it. The display device is deliberately not in this string: OptionView redraws its
  diagram and rebuilds its bind list whenever the string changes, and in modes 0 and 5 the device flips with every
  keyboard <-> pad change (also while a bind capture is waiting for a pad button).

### What refreshes when
From the callers of Key_GetBindingNames `0x493920`, UI_GetKeyBindingLocalizedString `0x604A40` and the `[{cmd}]`
conversion `0x603E50` in C1.exe 1.1.1.13 (SHA-256 `d990610732258b1d...16115390d`), read in the code; nothing here
was traced at run time.

| what | refresh | why |
|---|---|---|
| icon family inside any drawn text | next frame | the slot the escape points at is refilled on the render thread at draw time (section 9) |
| cgame's own hint drawers (`0x456130`, `0x456270`, `0x456370` the RC-XD lines, `0x457120`, `0x457610`, `0x457800`, `0x4579A0`) | next frame, keyboard <-> pad included | each converts into a stack buffer every time it draws |
| HUD element strings (`0x448E40`, `0x449710`) | next frame | converted in the hudelem draw path (inferred from their callers `0x44994E`, `0x44AF18`, `0x44B122`) |
| centre-print lines (`0x439170`) and game-message lines (`0x448E10`) | the next line printed | converted once when printed and stored (`0x4391D4` copies the result into the line); a line already on screen keeps its key names until it expires, a few seconds. An icon in it still follows the family |
| text names (mode 5) <-> icons | next frame for per-frame text; stored lines keep what they were printed with | the escape or the name is chosen when the string is built |
| Scaleform (Controls screen, pause menu, Scaleform HUD) and GSC | when their script asks again | they ask through `0x5B080D` / `0x55A8B6` and keep the answer: poll `k33_glyphSerial`, or reopen the view |
| Options > Controller diagram | within 250 ms | `k33_padinfo` is republished when it changes |

Not handled: re-converting stored centre-print / game-message lines (it would need every line's source string;
they expire in seconds). The Scaleform natives that call Key_GetCommandAssignment `0x492E30` directly (`0x5B06CC`,
`0x5B0746`, `0x5B091E`, `0x5B09E9`) and `0x4551C3`, `0x604F89` are not hooked, as before: they return every
device's keys, whatever `gpad_glyphs` says.

### Verified / not verified
- Built with the production profile (`K33_NO_COPY=1 sh scripts/build.sh`) and syntax-checked with the diagnostics profile;
  `tests/controller_glyph_policy_test.c` passes (105 checks).
- Not run in a game: every row of the refresh table, the `set` of the two new dvars, and the menus that will
  read them.
