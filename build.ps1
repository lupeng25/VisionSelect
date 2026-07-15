param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$Test,
    [switch]$Install,
    [ValidateSet('MinGW', 'MSVC')]
    [string]$Toolchain = 'MinGW'
)

$ErrorActionPreference = 'Stop'

if (-not $env:QT_ROOT) {
    throw '请设置 QT_ROOT，例如 D:\Qt6\6.10.1\mingw_64。'
}

if ($Toolchain -eq 'MinGW' -and -not $env:MINGW_ROOT) {
    throw 'MinGW 构建还需要设置 MINGW_ROOT，例如 D:\Qt6\Tools\mingw1310_64。'
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
    cmake --install "build/$preset" --prefix "dist/VisionSelect"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
