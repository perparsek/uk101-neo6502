# bygg.ps1 - bygger bada malen i uk101-neo6502.
#
#   .\tools\bygg.ps1             bygger vardharnesset och pico-firmwaren
#   .\tools\bygg.ps1 -Bara host  bara vardharnesset
#   .\tools\bygg.ps1 -Bara pico  bara firmwaren
#   .\tools\bygg.ps1 -Ren        river byggkatalogen forst
#
# Kraver MSYS2 i C:\msys64 med ucrt64-kedjan. Se README for de tva
# versionskraven pa pico-sdk och binutils.

param(
    [ValidateSet('bada', 'host', 'pico')]
    [string]$Bara = 'bada',
    [switch]$Ren
)

# Continue, inte Stop: cmake och gcc skriver normal information till stderr, och
# PowerShell 5.1 gor ErrorRecord av varje sadan rad. Felen fangas istallet pa
# $LASTEXITCODE efter varje anrop.
$ErrorActionPreference = 'Continue'
$rot = Split-Path -Parent $PSScriptRoot
$ucrt = 'C:\msys64\ucrt64\bin'

if (-not (Test-Path "$ucrt\gcc.exe")) {
    throw "Hittar inte $ucrt\gcc.exe. Installera ucrt64-kedjan, se README."
}
$env:PATH = "$ucrt;$env:PATH"

# ROM-headern genereras ur roms\*.rom och behovs av firmwaren.
if ($Bara -ne 'host') {
    Write-Host '== genererar uk101_roms.h' -ForegroundColor Cyan
    python "$rot\tools\rom2h.py"
}

if ($Bara -ne 'pico') {
    Write-Host '== vardharness' -ForegroundColor Cyan
    $ut = "$rot\host\uk101host.exe"
    & gcc -O2 -std=c11 -Wall -Wextra -o $ut "$rot\host\main.c" "$rot\src\uk101.c"
    if ($LASTEXITCODE -ne 0) { throw 'gcc misslyckades' }
    Write-Host "   $ut" -ForegroundColor Green
}

if ($Bara -ne 'host') {
    Write-Host '== pico-firmware' -ForegroundColor Cyan
    $sdk = "$rot\vendor\pico-sdk"
    if (-not (Test-Path "$sdk\pico_sdk_init.cmake")) {
        throw "Hittar inte pico-sdk i $sdk. Klona den enligt README."
    }
    $bygg = "$rot\pico\build"
    if ($Ren -and (Test-Path $bygg)) { Remove-Item -Recurse -Force $bygg }

    $sdkFram = $sdk.Replace('\', '/')
    & cmake -S "$rot\pico" -B $bygg -G Ninja "-DPICO_SDK_PATH=$sdkFram"
    if ($LASTEXITCODE -ne 0) { throw 'cmake-konfigurering misslyckades' }
    & cmake --build $bygg
    if ($LASTEXITCODE -ne 0) { throw 'cmake-bygge misslyckades' }

    & arm-none-eabi-size "$bygg\uk101.elf"
    Write-Host "   $bygg\uk101.uf2" -ForegroundColor Green
    Write-Host '   Flasha: hall BOOTSEL nere vid instickning, slapp uf2-filen pa enheten.'
}
