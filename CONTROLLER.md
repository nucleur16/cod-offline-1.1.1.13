# Controller support for C1 (branch `feat/controller`)

Gamepad play for codoMP_client_shipRetail.exe 1.1.1.13, added by Kernel33 (`controller.c`, included by
`kernel33.c` after `voice.c`, installed by `InstallController()` in `TakeOver`). Every patch is
byte-checked against the running exe; a mismatch is logged to `Kernel33.log` and that part stays off.

Build: `PATH=/c/msys64/mingw32/bin:$PATH sh build-controller.sh` writes `Kernel33_controller.dll` and copies
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
  `83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2`; `build-controller.sh` fetches it into
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
  deflection, quadratic), with SetCursorPos while the game is in the foreground.
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
| `gpad_debug` (not archived) | 0 | 1 state twice a second, 2 also every button / click |

### Commands
`gpad_status` (backend, pad name, mode, every pad bind), `gpad_defaultbinds`,
`gpad_fake <lx> <ly> <rx> <ry> [lt rt [buttons [ms]]] | off` (synthetic pad: sticks -1..1, triggers 0..1,
XInput button bits A 0x1000 B 0x2000 X 0x4000 Y 0x8000 LB 0x100 RB 0x200 Start 0x10 Back 0x20 L3 0x40 R3 0x80,
D-pad 1/2/4/8).

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
  B / Start are Escape (PauseMenuView's own back / close). `voice_ui/MainPause.as` (the 79-line document
  class) moves focus to the nearest visible clickable object (C1's BaseButton sets buttonMode), sends it
  MouseEventEx MOUSE_OVER / ROLL_OVER and draws a frame; select sends MouseEventEx MOUSE_DOWN / UP / CLICK
  (C1's BaseButton reads `cEvt as MouseEventEx`; a plain MouseEvent is dropped). Shipped as
  `client_shipRetail\patch\controller_ingame.ff` (build_ingame_ui.py), a non-*_ui_mp patch zone, so it loads
  ahead of code_pre_gfx and its codoingame.gfx wins over patch_common_mp's. Recompiling the 1168-line
  PauseMenuView crashed C1 (AV 0x92F6F3) when the menu opened, and recompiling codo.gfx's BaseMain blanked the
  front end, so only MainPause is replaced. Front-end menus keep the stick cursor.
  Verified in a match: Start opens it, left stick down / right / up walks loadout -> sensitivity -> Return to
  game (gold frame), A there closes the menu and the pad is back in game mode; B closes it.
- **Button names by last-used device**: Key_GetBindingNames' call to Key_GetCommandAssignment (0x49393E)
  returns only the active device's keys (pad: buttons, triggers or a stick past 0.45; keyboard / mouse:
  any key or mouse button through Sys_QueEvent, mouse look of 0.5 deg in 300 ms, or the pointer moving 12 px).
  Pad names by SDL controller type: Xbox (A, B, X, Y, LB, RB, LT, RT, LS, RS, Start, Back), PlayStation
  (Cross, Circle, Square, Triangle, L1, R1, L2, R2, L3, R3, Options, Touchpad), Nintendo (positional);
  `gpad_glyphs` overrides. C1's zones ship no button icon materials (only `dpad_*` killstreak icons), so
  these are text; images need a font or patch zone (phase 3).
- **Aim assist (pad only)**: C1's AimAssist_UpdateScreenTargets fills aaGlob 0x1501A80 (targets at +0x134,
  0x34 each, count +0xE34, view origin / angles +0xD0 / +0xDC). Slowdown 0.4 hip / 0.5 ADS in a 90x90 box,
  lock-on 0.6 (needs pad input past 0.05), ADS snap over 0.25 s in a 160x120 box (IW4 console values from
  iw4mp AimAssist_RegisterDvars). Applied to the right stick's own delta after C1's CL_MouseMove; off unless
  the pad is the last-used device and the mouse has not moved for 1.5 s. C1's mouse-path helper 0x4020D0 is
  IW4 PC's melee lunge, unchanged and no aim assist. `gpad_aimassist` (0..1), `gpad_aimassist_range`,
  `gpad_aimassist_snap`.
- **Server visibility**: userinfo dvar `gpad` (flag 0x200) is 1 while the pad is the last-used input.
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
