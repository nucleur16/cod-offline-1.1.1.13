# Plutonium T5 MP (windowed, LAN) for the rccap capture: tools/t5cap/zz_rccap.gsc
# goes in %LOCALAPPDATA%\Plutonium\storage\t5\scripts\mp\ for the run only.
param([string]$map,[string]$extra,[int]$timeout=600)
$pl="$env:LOCALAPPDATA\Plutonium"; $g='D:\SteamLibrary\steamapps\common\Call of Duty Black Ops'
$a='t5mp "'+$g+'" -lan +set r_fullscreen 0 +set r_mode 800x600 +set com_pauseOnLostFocus 0 +set logfile 2 '+$extra+' +devmap '+$map
$p=Start-Process -FilePath "$pl\bin\plutonium-bootstrapper-win32.exe" -ArgumentList $a -WorkingDirectory $pl -PassThru
"pid $($p.Id)"
if(-not $p.WaitForExit($timeout*1000)){ Stop-Process -Id $p.Id; "timeout-killed" } else { "exited $($p.ExitCode)" }
