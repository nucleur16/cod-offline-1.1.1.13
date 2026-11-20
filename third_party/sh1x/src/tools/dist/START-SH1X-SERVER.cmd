@echo off
REM sh1x server - Call of Duty Online Closed Alpha Server (Windows).
REM
REM Settings live in main\server.cfg. The lobby (Demonware emulator) runs
REM separately on this host; server.cfg says where its state directory is.
REM
REM   SH1X_PORT        UDP port, 17733 unless set (the lobby must advertise the same)
REM   SH1X_MAXCLIENTS  player slots, 18 unless set (fixed at start, not settable in server.cfg)
REM   SH1X_INSTANCE    set by the lobby for extra instances: logs and sessions go to
REM                    instances\<name>\main instead of main\
REM   SH1X_MAP, SH1X_GAMETYPE, SH1X_ZONE_TYPE, SH1X_SESSION_MAP
REM                    boot straight into this match instead of server.cfg's sh1x_boot
REM   SH1X_EXTRA_ARGS  extra "+set name value" pairs, applied before server.cfg
REM
REM Scripts and rawfiles ship in main\sh1x_svr_scripts.iwd. A loose file under
REM main\scripts\ (same relative path) overrides the packed one.

setlocal
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
if not defined SH1X_PORT set "SH1X_PORT=17733"
if not defined SH1X_MAXCLIENTS set "SH1X_MAXCLIENTS=18"
set "HOME_DIR=%ROOT%"
if defined SH1X_INSTANCE set "HOME_DIR=%ROOT%\instances\%SH1X_INSTANCE%"

if not exist "%ROOT%\client_shipRetail\chinese\code_post_gfx_mp.ff" (
    echo Game files not found: expected "%ROOT%\client_shipRetail\chinese\*.ff".
    exit /b 1
)
if not exist "%ROOT%\main\sh1x_svr_scripts.iwd" (
    echo Script pack not found: "%ROOT%\main\sh1x_svr_scripts.iwd".
    exit /b 1
)
if not exist "%ROOT%\C1.exe" if not exist "%ROOT%\c1_keys.bin" (
    echo Neither C1.exe nor c1_keys.bin is in "%ROOT%"; the server cannot decrypt the game archives.
    exit /b 1
)

set "BOOT=+vstr sh1x_boot"
if defined SH1X_MAP (
    if not defined SH1X_GAMETYPE set "SH1X_GAMETYPE=dm"
    if not defined SH1X_ZONE_TYPE set "SH1X_ZONE_TYPE=mp"
    if not defined SH1X_SESSION_MAP set "SH1X_SESSION_MAP=%SH1X_MAP%"
)
if defined SH1X_MAP set "BOOT=+set g_gametype %SH1X_GAMETYPE% +set sv_zoneType %SH1X_ZONE_TYPE% +set sv_sessionMap %SH1X_SESSION_MAP% +map %SH1X_MAP%"

if not exist "%HOME_DIR%\main" mkdir "%HOME_DIR%\main"
echo sh1x server on udp/%SH1X_PORT% - settings main\server.cfg, log %HOME_DIR%\main\qconsole.log
cd /d "%ROOT%"
"%ROOT%\sh1x_dedrun.exe" ^
  +set dedicated 2 ^
  +set fs_basepath "%ROOT%" ^
  +set fs_homepath "%HOME_DIR%" ^
  +set net_port %SH1X_PORT% ^
  +set sv_maxclients %SH1X_MAXCLIENTS% ^
  +set logfile 2 ^
  +set logrcon 1 ^
  +set developer 0 ^
  +set sv_fps 60 ^
  +set scriptPath "fs:scripts" ^
  +set scriptOverlay "fs:scripts" ^
  +set scriptNativeOverlay "fs:scripts" ^
  %SH1X_EXTRA_ARGS% ^
  +exec server.cfg ^
  %BOOT%
endlocal
