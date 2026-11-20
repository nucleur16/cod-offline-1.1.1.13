@echo off
setlocal
set "CODOL_MAP="
set "CODOL_GAMETYPE="
set "CODOL_ZONE_TYPE="
for /f "tokens=1,2,3 delims=|" %%A in ('powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0..\tools\ol_map_handoff.ps1" -Path "%~dp0fixtures\startgame_handoff.txt" 2^>nul') do (
  set "CODOL_MAP=%%A"
  set "CODOL_GAMETYPE=%%B"
  set "CODOL_ZONE_TYPE=%%C"
)
if not "%CODOL_MAP%"=="mp_cargoship_sh" exit /b 1
if not "%CODOL_GAMETYPE%"=="war" exit /b 1
if not "%CODOL_ZONE_TYPE%"=="mp" exit /b 1
echo CoDOL launcher map handoff: OK
exit /b 0
