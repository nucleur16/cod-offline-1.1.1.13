@echo off
rem build.bat [1.1.1.13 install folder]
rem Builds everything an install needs into dist\: Kernel33.dll, the patched client exe, the zones and the match
rem server's scripts. Copy (drag) the contents of dist\ onto the game folder. See README.md.
setlocal
set "REPO=%~dp0"
set "REPO=%REPO:~0,-1%"
set "INSTALL=%~1"
if "%INSTALL%"=="" set "INSTALL=D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13"
set "DIST=%REPO%\dist"
if "%MSYS2_ROOT%"=="" set "MSYS2_ROOT=C:\msys64"
set "BASH=%MSYS2_ROOT%\usr\bin\bash.exe"

if not exist "%BASH%" ( echo MSYS2 not found at %MSYS2_ROOT% ^(set MSYS2_ROOT^). & exit /b 1 )
where python >nul 2>nul || ( echo Python 3 is not on PATH. & exit /b 1 )
if not exist "%INSTALL%\C1.exe" ( echo "%INSTALL%" is not a 1.1.1.13 install with C1.exe ^(give it as the first argument^). & exit /b 1 )
set "EXE=%INSTALL%\codoMP_client_shipRetail.exe.bak"
if not exist "%EXE%" set "EXE=%INSTALL%\codoMP_client_shipRetail.exe"

pushd "%REPO%" || exit /b 1
if exist dist rmdir /s /q dist
mkdir dist\main

rem MSYS2 login shell in this folder, with Windows' PATH (python) kept.
set CHERE_INVOKING=1
set MSYS2_PATH_TYPE=inherit
set MSYSTEM=MINGW32

echo === 1/4 match server (server\sh1x) and its scripts
"%BASH%" -lc "K33_P2P_STAGE=dist sh scripts/build-server.sh" || goto fail
move /y dist\p2p\home\main\sh1x_svr_scripts.iwd dist\main\ >nul || goto fail
rmdir /s /q dist\p2p

echo === 2/4 Kernel33.dll
"%BASH%" -lc "K33_NO_COPY=1 sh scripts/build.sh" || goto fail
copy /y build\Kernel33.dll dist\ >nul || goto fail

echo === 3/4 zones
python scripts\build_zones.py --game "%INSTALL%" --out "%DIST%" || goto fail

echo === 4/4 codoMP_client_shipRetail.exe
python scripts\patch_exe.py "%EXE%" dist\codoMP_client_shipRetail.exe || goto fail

popd
echo.
echo Done: %DIST%
echo Copy everything in it onto your 1.1.1.13 game folder and replace the files there.
exit /b 0

:fail
popd
echo.
echo Build failed (see above).
exit /b 1
