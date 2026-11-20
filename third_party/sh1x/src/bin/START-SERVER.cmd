@echo off
REM sh1x server - Call of Duty Online Closed Alpha Server launcher.
REM
REM Runs the native server on the port the alpha0022 lobby (dw-capture.py on
REM udp/3074) hands to the client, in place of the Python game-server.py.
REM Stop only the Python game server before starting this; leave the lobby up.
REM
REM Override any setting through the environment before calling, for example:
REM   set CODOL_MAP=mp_bog_sh & set CODOL_GAMETYPE=war & START-SERVER.cmd
REM For split installations, CODOL_INSTALL holds the game archives and
REM CODOL_CLIENT_EXE points to the matching C1.exe or c1_keys.bin elsewhere.
REM CODOL_PROFILE_DIR may point directly at a separate lobby's resources folder.

setlocal

REM --- Paths -------------------------------------------------------------
if not defined CODOL_INSTALL set "CODOL_INSTALL=D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13"
if not defined CODOL_REFERENCE set "CODOL_REFERENCE=%CODOL_INSTALL%\CODOL\1.1.1.13_server\alpha0022"
if not defined CODOL_HOME set "CODOL_HOME=%~dp0home"
REM The current GSC root is bin\home\codol_rawfiles. Edit it in place; the
REM former multi-root collation step has been retired.
REM Independent of CODOL_HOME: a separate home (a test instance) still runs
REM the repository's rawfiles unless CODOL_SCRIPTS says otherwise.
if not defined CODOL_SCRIPTS set "CODOL_SCRIPTS=%~dp0home\codol_rawfiles"

REM --- Network: same port and bind as 2-START-GAMESERVER.cmd (game-server.py
REM --- --port 17733 --bind 0.0.0.0, overridable through CODOL_MM_HOST_PORT).
if not defined CODOL_MM_HOST_PORT set "CODOL_MM_HOST_PORT=17733"
REM This local launcher owns one match port. Refuse a second launch before
REM touching lobby state or replacing the server pack.
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0..\tools\single_server_guard.ps1" -Port %CODOL_MM_HOST_PORT%
if errorlevel 1 (
    echo Server launch cancelled: UDP/%CODOL_MM_HOST_PORT% is occupied or could not be checked.
    exit /b 1
)

REM --- Session security: the lobby (dw-capture.py) writes the live 16-byte
REM --- session key to securitykey.bin at ready-up; the native transport reads it
REM --- to accept the client's encrypted handshake. Without it the client times out.
REM --- CODOL_STATE_DIR is the lobby's state directory (the one it was started with,
REM --- e.g. OFFLINE-DEV.cmd); the admission roster game_admissions.txt is read
REM --- from the same directory (admissionPath defaults beside the key).
if not defined CODOL_STATE_DIR set "CODOL_STATE_DIR=%CODOL_REFERENCE%"
if not exist "%CODOL_STATE_DIR%\" (
    echo Lobby state directory not found under "%CODOL_STATE_DIR%". Set CODOL_STATE_DIR to the lobby state directory.
    exit /b 1
)
if not defined CODOL_CLIENT_EXE goto default_client_exe
if not exist "%CODOL_CLIENT_EXE%" (
    echo CODOL_CLIENT_EXE not found at "%CODOL_CLIENT_EXE%". Set it to the matching C1.exe or c1_keys.bin.
    exit /b 1
)
if exist "%CODOL_CLIENT_EXE%\" (
    echo CODOL_CLIENT_EXE must be a file: "%CODOL_CLIENT_EXE%".
    exit /b 1
)
goto client_exe_ready
:default_client_exe
if not exist "%CODOL_INSTALL%\C1.exe" (
    echo C1.exe not found under "%CODOL_INSTALL%". Set CODOL_INSTALL to the CoDOL 1.1.1.13 directory or CODOL_CLIENT_EXE to the matching C1.exe or c1_keys.bin.
    exit /b 1
)
:client_exe_ready
if not exist "%~dp0sh1x_dedrun_next.exe" (
    echo Native server executable not found beside this launcher.
    echo From the repository root, run tools\BUILD-SERVER.ps1, then promote bin\sh1x_dedrun.exe to bin\sh1x_dedrun_next.exe.
    exit /b 1
)
if not exist "%CODOL_SCRIPTS%\maps\mp\gametypes\_callbacksetup.gsc" (
    echo GSC root not found under "%CODOL_SCRIPTS%".
    echo Set CODOL_SCRIPTS to bin\home\codol_rawfiles or a matching script root.
    exit /b 1
)
if defined CODOL_PROFILE_DIR goto profile_override
if not exist "%CODOL_REFERENCE%\" (
    echo alpha0022 reference not found under "%CODOL_REFERENCE%". Set CODOL_REFERENCE or CODOL_PROFILE_DIR.
    exit /b 1
)
set "CODOL_PROFILE_DIR=%CODOL_REFERENCE%\resources"
goto profile_ready
:profile_override
if not exist "%CODOL_PROFILE_DIR%\" (
    echo Lobby profile directory not found under "%CODOL_PROFILE_DIR%". Set CODOL_PROFILE_DIR to the lobby resources directory.
    exit /b 1
)
:profile_ready
REM --- Match ---------------------------------------------------------------
REM The lobby also stamps the exact map, playlist gametype and zone category it
REM sent to C1 into startgame.txt.  Consume that single decision when the caller
REM has not pinned CODOL_MAP.  This prevents C1 loading (for example)
REM mp_cargoship_sh while the native server publishes mp_killhouse_sh in its
REM connect response, serverinfo and gamestate.  Explicit CODOL_MAP keeps the
REM old launcher override semantics and therefore does not mix in a handoff for
REM a different map.
set "CODOL_HANDOFF=%CODOL_STATE_DIR%\startgame.txt"
if defined CODOL_MAP goto map_pinned
for /f "tokens=1,2,3,4 delims=|" %%A in ('powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0..\tools\ol_map_handoff.ps1" -Path "%CODOL_HANDOFF%" 2^>nul') do (
  set "CODOL_MAP=%%A"
  set "CODOL_GAMETYPE=%%B"
  set "CODOL_ZONE_TYPE=%%C"
  set "CODOL_SESSION_MAP=%%D"
)
:map_pinned
if not defined CODOL_MAP set "CODOL_MAP=mp_rust"
if not defined CODOL_GAMETYPE set "CODOL_GAMETYPE=dm"
if not defined CODOL_ZONE_TYPE set "CODOL_ZONE_TYPE=mp"
REM The playlist/session spelling; equals the map for everything but co-op.
if not defined CODOL_SESSION_MAP set "CODOL_SESSION_MAP=%CODOL_MAP%"
REM Shared local-only credential used by the lobby's match-start dispatcher.
REM Generate it once so source and launcher files do not contain a fixed secret.
set "CODOL_RCON_FILE=%CODOL_STATE_DIR%\native_server_rcon.txt"
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0..\tools\ol_rcon_password.ps1" -Path "%CODOL_RCON_FILE%"
if errorlevel 1 exit /b 1
set /p CODOL_MATCH_RCON_PASSWORD=<"%CODOL_RCON_FILE%"
if not defined CODOL_MATCH_RCON_PASSWORD (
    echo Could not create or read "%CODOL_RCON_FILE%".
    exit /b 1
)
REM developer 1 keeps the diagnostic traces and rawfile dumps; set 0 for quiet.
REM CODOL_EXTRA_ARGS may carry extra +set pairs, e.g. set CODOL_EXTRA_ARGS=+set rovAssertUnits 0
if not defined CODOL_DEVELOPER set "CODOL_DEVELOPER=0"
REM This development launcher starts a populated match by default. Set
REM CODOL_BOTS=0 to run without training bots.
if not defined CODOL_BOTS set "CODOL_BOTS=12"
REM Exact-build named bot profile: recruit, regular, hardened or veteran.
if not defined CODOL_BOT_DIFFICULTY set "CODOL_BOT_DIFFICULTY=recruit"
REM The lobby writes game_admissions.txt beside securitykey.bin. Require a
REM matching admission by default, as the packaged launcher does. Set
REM CODOL_ADMISSION_REQUIRED=0 only for isolated direct-connect probes.
if not defined CODOL_ADMISSION_REQUIRED set "CODOL_ADMISSION_REQUIRED=1"

REM --- Actor animation classes ------------------------------------------
REM Serverinfo key 2482 tells C1 which actor animation trees to register at map
REM load (C1 465C08 parses it into cgs+164, 450AF8 copies it to bgs+921F8, and
REM 408456 tests one bit per tree). human=1 dog=2 civilian=4 zombie=8. It must
REM be chosen BEFORE the client initialises the map, not when a streak fires.
REM Bit 2 (tree selector 1, "dog") is what the attack-dogs killstreak needs;
REM its assets ship in every MP match - animtrees/dog.atr in common_mp.zone and
REM animlists/common_dog.animlist plus the german_sheperd_dog model and 68 dog
REM xanims in common_gametype_mp.zone. C1 clears a bit whose animlist fails to
REM load (408377), so an unsupported class degrades instead of breaking.
REM Leave CODOL_ANIMLIST_MASK UNSET by default.  A hardcoded 2 here was right
REM for mp and silently wrong for zm: MEASURED at C1 0x0040840b, the five actor
REM classes are 0x01 generic_human, 0x02 dog, 0x04 generic_human,
REM 0x08 generic_human, 0x10 zombie_dog, and a zombies match needs 0x08 (which
REM also pulls common_zombie.animlist and its ai_zombie_* set).  The server now
REM derives the right mask for the mounted asset category from its own species
REM table, so the operator does not have to remember a number per game mode.
REM Setting CODOL_ANIMLIST_MASK still overrides it, and the server warns if the
REM value omits a class the category needs.
set "CODOL_ANIMLIST_ARG="
if defined CODOL_ANIMLIST_MASK set "CODOL_ANIMLIST_ARG=+set animlistMask %CODOL_ANIMLIST_MASK%"

if not exist "%CODOL_HOME%\main" mkdir "%CODOL_HOME%\main"
REM FS_ReadFile resolves bot CSVs through the normal search path. Build the
REM same server-only IWD used by the dedicated-server distribution so an
REM isolated fs_homepath mounts the waypoints and bot class table as well.
if not defined CODOL_PACK_SOURCE set "CODOL_PACK_SOURCE=%~dp0.."
python "%~dp0..\tools\stage_server_pack.py" --repo "%CODOL_PACK_SOURCE%" --home-main "%CODOL_HOME%\main"
if errorlevel 1 exit /b 1

echo sh1x server: %CODOL_MAP% %CODOL_GAMETYPE% ^(%CODOL_ZONE_TYPE%^) on udp/%CODOL_MM_HOST_PORT%
echo   bots      %CODOL_BOTS%
echo   difficulty %CODOL_BOT_DIFFICULTY%
echo   session   %CODOL_SESSION_MAP%
echo   install   %CODOL_INSTALL%
echo   scripts   %CODOL_SCRIPTS%
echo   home/log  %CODOL_HOME%\main\qconsole.log
echo.

REM Keep Discord presence available after a server restart. It is a separate
REM background process and the script leaves an existing instance alone.
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0..\tools\start_discord_presence.ps1" -Install "%CODOL_INSTALL%" -StateDirectory "%CODOL_STATE_DIR%"
if errorlevel 1 echo Warning: Discord presence did not start; the game server will continue.

cd /d "%~dp0"
REM logrcon 1: a map started over rcon runs SV_SpawnServer inside the rcon
REM output redirect, so every map-load line (asset category, weapon window,
REM waypoint graph, the zm stage list) went to the rcon reply and never
REM reached qconsole.log.  logrcon copies redirected output to the log too
REM (common_io.c Com_PrintMessage).
"%~dp0sh1x_dedrun_next.exe" ^
  +set dedicated 2 ^
  +set fs_basepath "%CODOL_INSTALL%" ^
  +set fs_homepath "%CODOL_HOME%" ^
  +set clientExe "%CODOL_CLIENT_EXE%" ^
  +set net_port %CODOL_MM_HOST_PORT% ^
  +set rcon_password "%CODOL_MATCH_RCON_PASSWORD%" ^
  +set logfile 2 ^
  +set logrcon 1 ^
  +set developer %CODOL_DEVELOPER% ^
  +set sv_fps 60 ^
  +set scriptPath "%CODOL_SCRIPTS%" ^
  +set scriptOverlay "%CODOL_SCRIPTS%" ^
  +set scriptNativeOverlay "%CODOL_SCRIPTS%" ^
  +set securityKeyPath "%CODOL_STATE_DIR%\securitykey.bin" ^
  +set admissionPath "%CODOL_STATE_DIR%\game_admissions.txt" ^
  +set admissionRequired %CODOL_ADMISSION_REQUIRED% ^
  +set profilePath "%CODOL_PROFILE_DIR%" ^
  +set sv_zoneType %CODOL_ZONE_TYPE% ^
  +set sv_sessionMap "%CODOL_SESSION_MAP%" ^
  %CODOL_ANIMLIST_ARG% ^
  +set g_gametype %CODOL_GAMETYPE% ^
  +set scr_bots %CODOL_BOTS% ^
  +set sv_botDifficulty %CODOL_BOT_DIFFICULTY% ^
  +scriptPath ^
  +scriptOverlay ^
  +scriptNativeOverlay ^
  +map %CODOL_MAP% %CODOL_EXTRA_ARGS%

endlocal
