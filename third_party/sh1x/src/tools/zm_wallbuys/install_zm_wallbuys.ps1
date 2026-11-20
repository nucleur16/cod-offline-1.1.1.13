# Installs the zombies wall-buy outlines (zm_wallbuy_staging) into BOTH client installs. Client must be closed.
#   client_shipRetail\chinese\zm_theater_sh.ff, zm_factory_sh.ff, zm_sumpf_sh.ff   (the installed ported zones + outlines)
#   main\iw_model_21.iwd                                                            (Black Ops chalk images)
# -Rollback restores the .pre-wallbuy copies and removes the new iwd.
param([switch]$Rollback)
$ErrorActionPreference = 'Stop'
$src = Join-Path $PSScriptRoot 'out'
if (Get-Process -Name 'codoMP_client_shipRetail','C1' -ErrorAction SilentlyContinue) { Write-Output 'client running; not installing'; exit 2 }
$roots = @('D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13', 'D:\CoD\CODOL Builds\CODOL 1.1.1.13 map pack')
$replaced = @('client_shipRetail\chinese\zm_theater_sh.ff', 'client_shipRetail\chinese\zm_factory_sh.ff', 'client_shipRetail\chinese\zm_sumpf_sh.ff')
$added = @('main\iw_model_21.iwd')
$md5 = @{
  'client_shipRetail\chinese\zm_theater_sh.ff' = 'eafe8b53e258a4925098a0b03e8fbede'
  'client_shipRetail\chinese\zm_factory_sh.ff' = 'd22cc9442652e8afa0531df6ac71d8e3'
  'client_shipRetail\chinese\zm_sumpf_sh.ff'   = 'd96b8e9269be66a491058e65a733e87c'
  'main\iw_model_21.iwd' = '6398c76e8742426176a396b0c4b3fefc'
}
if (-not $Rollback) {
  foreach ($f in $md5.Keys) {
    $h = (Get-FileHash -Algorithm MD5 (Join-Path $src $f)).Hash.ToLower()
    if ($h -ne $md5[$f]) { Write-Output "staged $f has md5 $h, expected $($md5[$f]); not installing"; exit 3 }
  }
}
foreach ($r in $roots) {
  foreach ($f in $replaced) {
    $d = Join-Path $r $f
    if ($Rollback) { if (Test-Path "$d.pre-wallbuy") { Copy-Item "$d.pre-wallbuy" $d -Force } ; continue }
    if ((Test-Path $d) -and -not (Test-Path "$d.pre-wallbuy")) { Copy-Item $d "$d.pre-wallbuy" -Force }
    Copy-Item (Join-Path $src $f) $d -Force
  }
  foreach ($f in $added) {
    $d = Join-Path $r $f
    if ($Rollback) { Remove-Item $d -ErrorAction SilentlyContinue; continue }
    New-Item -ItemType Directory -Force (Split-Path $d) | Out-Null
    Copy-Item (Join-Path $src $f) $d -Force
  }
}
foreach ($r in $roots) {
  foreach ($f in $replaced + $added) {
    $d = Join-Path $r $f
    if (Test-Path $d) { "$((Get-FileHash -Algorithm MD5 $d).Hash.ToLower())  $d" }
  }
}
Write-Output ($(if ($Rollback) { 'rolled back' } else { 'installed' }))
