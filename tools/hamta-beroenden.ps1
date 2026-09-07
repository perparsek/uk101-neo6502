# hamta-beroenden.ps1 - klonar tredjepartsberoendena till vendor\.
#
#   .\tools\hamta-beroenden.ps1
#
# pico-sdk maste vara 2.2.0 eller senare. I 2.1.1 saknar
# tools/pioasm/output_format.h sitt #include <cstdint>, och pioasm bygger inte
# med GCC 13 och uppat. Taggen nedan ar den som projektet ar provat mot.
#
# Se ocksa README om arm-none-eabi-binutils: fran 2.45 avvisar ld att samma
# lankskript forekommer tva ganger, vilket pico-sdk gor. Kedjan maste vara
# 2.44 eller aldre.

param(
    [string]$PicoSdkTag = '2.3.1'
)

$ErrorActionPreference = 'Continue'
$rot = Split-Path -Parent $PSScriptRoot
$vendor = Join-Path $rot 'vendor'

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'git saknas i PATH.'
}
if (-not (Test-Path $vendor)) { New-Item -ItemType Directory $vendor | Out-Null }

$sdk = Join-Path $vendor 'pico-sdk'
if (Test-Path $sdk) {
    Write-Host "pico-sdk finns redan i $sdk, hoppar over." -ForegroundColor Yellow
} else {
    Write-Host "== klonar pico-sdk $PicoSdkTag" -ForegroundColor Cyan
    & git clone --depth 1 --branch $PicoSdkTag https://github.com/raspberrypi/pico-sdk.git $sdk
    if ($LASTEXITCODE -ne 0) { throw 'git clone pico-sdk misslyckades' }
    # Bara tinyusb behovs, inte hela undermodulsuppsattningen.
    & git -C $sdk submodule update --init --depth 1 lib/tinyusb
    if ($LASTEXITCODE -ne 0) { throw 'git submodule tinyusb misslyckades' }
}

$dvi = Join-Path $vendor 'PicoDVI'
if (Test-Path $dvi) {
    Write-Host "PicoDVI finns redan i $dvi, hoppar over." -ForegroundColor Yellow
} else {
    Write-Host '== klonar PicoDVI' -ForegroundColor Cyan
    & git clone --depth 1 https://github.com/Wren6991/PicoDVI.git $dvi
    if ($LASTEXITCODE -ne 0) { throw 'git clone PicoDVI misslyckades' }
}

Write-Host ''
Write-Host 'Klart. Bygg med .\tools\bygg.ps1' -ForegroundColor Green
