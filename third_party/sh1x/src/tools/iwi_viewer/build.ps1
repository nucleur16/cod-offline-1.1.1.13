$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (-not (Test-Path '.venv/Scripts/python.exe')) {
    python -m venv .venv
}
& .venv/Scripts/python.exe -m pip install -r requirements.txt
if ($LASTEXITCODE -ne 0) { throw 'Dependency install failed' }
# Do not collect unrelated DLLs from game/tool directories on the calling PATH.
# Qt requires a newer VC runtime than the one beside some Python installations.
$viewerOriginalPath = $env:PATH
try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    & .venv/Scripts/python.exe -m PyInstaller --noconfirm --clean --onefile --windowed --name sh1x-IWI-Viewer --add-binary '.venv/Lib/site-packages/PySide6/vcruntime140.dll;.' --add-binary '.venv/Lib/site-packages/PySide6/vcruntime140_1.dll;.' desktop.py
} finally {
    $env:PATH = $viewerOriginalPath
}
if ($LASTEXITCODE -ne 0) { throw 'EXE build failed' }
Write-Host "Built $PSScriptRoot/dist/sh1x-IWI-Viewer.exe"
