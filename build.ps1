param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$Test,
    [switch]$Install,
    [ValidateSet('MinGW', 'MSVC')]
    [string]$Toolchain = 'MinGW'
)

$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot

if (-not $env:QT_ROOT) {
    throw 'QT_ROOT is required, for example D:\Qt6\6.10.1\mingw_64.'
}

if ($Toolchain -eq 'MinGW' -and -not $env:MINGW_ROOT) {
    throw 'MINGW_ROOT is required for MinGW, for example D:\Qt6\Tools\mingw1310_64.'
}

if ($Toolchain -eq 'MinGW') {
    $env:PATH = "$env:MINGW_ROOT\bin;$env:PATH"
}

$preset = if ($Toolchain -eq 'MSVC') {
    if ($Configuration -eq 'Release') { 'windows-msvc2022-release' } else { 'windows-msvc2022' }
} else {
    if ($Configuration -eq 'Release') { 'windows-mingw64-release' } else { 'windows-mingw64' }
}

cmake --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Test) {
    ctest --test-dir "build/$preset" --output-on-failure
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if ($Install) {
    $installPrefix = Join-Path $PSScriptRoot 'dist\VisionSelect'
    cmake --install "build/$preset" --prefix $installPrefix --component Runtime
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
