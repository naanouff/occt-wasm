# Rebuild the image (OCCT layer cached) and copy occt-kernel into the local consumers.
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
docker build -t occt-wasm . | Out-Null
docker rm -f occt-out 2>$null | Out-Null
docker create --name occt-out occt-wasm | Out-Null
if (Test-Path dist) { Remove-Item -Recurse -Force dist }
docker cp occt-out:/opt/dist ./dist | Out-Null
docker rm occt-out | Out-Null
foreach ($cible in $args) { Copy-Item dist\occt-kernel.js, dist\occt-kernel.wasm $cible -Force }
