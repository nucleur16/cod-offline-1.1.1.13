@echo off
rem build.bat [1.1.1.13 install folder]
rem Builds everything an install needs into dist\ by running build.sh in MSYS2: Kernel33.dll, the patched client exe,
rem the zones and the match server's pack. Copy (drag) the contents of dist\ onto the game folder. See README.md.
setlocal
set "REPO=%~dp0"
set "REPO=%REPO:~0,-1%"
set "INSTALL=%~1"
if "%INSTALL%"=="" ( echo usage: build.bat "path\to\Call of Duty Online 1.1.1.13" & exit /b 1 )
if "%MSYS2_ROOT%"=="" set "MSYS2_ROOT=C:\msys64"
set "BASH=%MSYS2_ROOT%\usr\bin\bash.exe"

if not exist "%BASH%" ( echo MSYS2 not found at %MSYS2_ROOT% ^(set MSYS2_ROOT^). & exit /b 1 )
where python >nul 2>nul || ( echo Python 3 is not on PATH. & exit /b 1 )
if not exist "%INSTALL%\client_shipRetail\chinese" ( echo "%INSTALL%" is not a 1.1.1.13 install ^(give it as the first argument^). & exit /b 1 )

rem MSYS2 login shell in the repository, with Windows' PATH (python) kept. Forward slashes: both MSYS2 and Windows'
rem python take them.
set CHERE_INVOKING=1
set MSYS2_PATH_TYPE=inherit
set MSYSTEM=MINGW32
set "K33_GAME=%INSTALL:\=/%"
pushd "%REPO%" || exit /b 1
"%BASH%" -lc "sh build.sh" || goto fail
popd
exit /b 0

:fail
popd
echo.
echo Build failed (see above).
exit /b 1
