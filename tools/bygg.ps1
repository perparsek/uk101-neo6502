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
    $flaggor = @('-O2', '-std=c11', '-Wall', '-Wextra', "-I$rot\src")
    $delat = @("$rot\host\hostbus.c", "$rot\src\uk101.c")

    Write-Host '== uk101host.exe (skriptad rigg)' -ForegroundColor Cyan
    $ut = "$rot\host\uk101host.exe"
    & gcc @flaggor -o $ut "$rot\host\main.c" @delat
    if ($LASTEXITCODE -ne 0) { throw 'gcc misslyckades for uk101host' }
    Write-Host "   $ut" -ForegroundColor Green

    # Det interaktiva fonstret kraver SDL2. Saknas den byggs bara riggen.
    Write-Host '== uk101gui.exe (interaktivt fonster)' -ForegroundColor Cyan
    $sdlDll = "$ucrt\SDL2.dll"
    if (-not (Test-Path $sdlDll)) {
        Write-Host '   SDL2 saknas, hoppar over. Installera med:' -ForegroundColor Yellow
        Write-Host '   pacman -S mingw-w64-ucrt-x86_64-SDL2' -ForegroundColor Yellow
    } else {
        $sdlFlaggor = (& pkg-config --cflags --libs sdl2) -split '\s+' | Where-Object { $_ }
        $ut = "$rot\host\uk101gui.exe"
        & gcc @flaggor -o $ut "$rot\host\gui.c" @delat @sdlFlaggor
        if ($LASTEXITCODE -ne 0) { throw 'gcc misslyckades for uk101gui' }
        # DLL:en bredvid exe-filen, sa den kan koras utan MSYS2 i PATH.
        Copy-Item $sdlDll "$rot\host\SDL2.dll" -Force
        Write-Host "   $ut" -ForegroundColor Green
    }
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
