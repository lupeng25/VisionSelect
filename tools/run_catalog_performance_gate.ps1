param(
    [string]$QtBin = "",
    [string]$TestExe = ".\bin\VisionSelectTests.exe"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (!(Test-Path $TestExe)) {
    throw "Test executable not found: $TestExe"
}

if ([string]::IsNullOrWhiteSpace($QtBin)) {
    $candidates = @(
        "C:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin",
        "D:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin"
    )
    $QtBin = ($candidates | Where-Object { Test-Path $_ } | Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($QtBin)) {
        throw "Qt bin directory not found. Pass -QtBin explicitly."
    }
}

$env:PATH = "$QtBin;$env:PATH"
$env:VISIONSELECT_PERF_GATE = "1"

& $TestExe catalogPerformanceGate -o -,txt
exit $LASTEXITCODE
