param(
    [string]$ApplicationId = $(if ($env:CODOL_DISCORD_APP_ID) { $env:CODOL_DISCORD_APP_ID } else { '1554730441825718272' }),
    [string]$Install = $(if ($env:CODOL_INSTALL) { $env:CODOL_INSTALL } else { 'E:\codol-classic' }),
    [string]$Stack = (Join-Path $env:TEMP 'codol-11113-e2e-20260928'),
    [string]$StateDirectory
)

$ErrorActionPreference = 'Stop'
if ($ApplicationId -notmatch '^\d+$') {
    throw 'Set CODOL_DISCORD_APP_ID to the CoDOL Discord application ID.'
}

$scriptPath = Join-Path $PSScriptRoot 'discord_presence.py'
if (-not $StateDirectory) {
    $StateDirectory = if ($env:CODOL_STATE_DIR) { $env:CODOL_STATE_DIR } else { Join-Path $Stack 'lobby' }
}
$logPath = Join-Path $Install 'main\console_mp.log'
$rconPath = Join-Path $StateDirectory 'native_server_rcon.txt'
$logDirectory = Join-Path (Split-Path -Parent $StateDirectory) 'local-stack-logs'
foreach ($path in @($scriptPath, $rconPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing presence input: $path"
    }
}
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null

$alreadyRunning = @(Get-CimInstance Win32_Process -Filter "Name = 'pythonw.exe' OR Name = 'python.exe'" |
    Where-Object { $_.CommandLine -and $_.CommandLine.Contains('discord_presence.py') })
if ($alreadyRunning.Count -gt 0) {
    Write-Output "Discord presence already running (PID $($alreadyRunning[0].ProcessId))."
    return
}

$python = (Get-Command python.exe -ErrorAction Stop).Source
$arguments = @(
    ('"' + $scriptPath + '"'), '--app-id', $ApplicationId,
    '--log', ('"' + $logPath + '"'),
    '--rcon-file', ('"' + $rconPath + '"')
)
$process = Start-Process -FilePath $python -ArgumentList $arguments -PassThru -WindowStyle Hidden `
    -RedirectStandardOutput (Join-Path $logDirectory 'discord-presence.stdout.log') `
    -RedirectStandardError (Join-Path $logDirectory 'discord-presence.stderr.log')
Write-Output "Discord presence started in the background (PID $($process.Id))."
