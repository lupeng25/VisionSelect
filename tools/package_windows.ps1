param(
    [string]$QtDir = "C:\Qt\Qt5.12.9\5.12.9\msvc2015_64",
    [string]$OutputDir = "dist\VisionSelect",
    [switch]$SkipBuild,
    [switch]$SkipInstaller,
    [string]$InnoSetupCompiler = "iscc.exe"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$distRoot = Join-Path $root "dist"
$output = if ([System.IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $root $OutputDir }
$distRootFull = [System.IO.Path]::GetFullPath($distRoot).TrimEnd('\')
$outputFull = [System.IO.Path]::GetFullPath($output).TrimEnd('\')
$exe = Join-Path $root "bin\VisionSelect.exe"
$lrelease = Join-Path $QtDir "bin\lrelease.exe"
$windeployqt = Join-Path $QtDir "bin\windeployqt.exe"
$vcRuntimeDir = "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\redist\x64\Microsoft.VC140.CRT"
$vcRuntimeDlls = @("vcruntime140.dll", "msvcp140.dll", "concrt140.dll")

if (-not (Test-Path -LiteralPath $lrelease)) { throw "lrelease not found: $lrelease" }
if (-not (Test-Path -LiteralPath $windeployqt)) { throw "windeployqt not found: $windeployqt" }
if (-not ($outputFull.Equals($distRootFull, [System.StringComparison]::OrdinalIgnoreCase) -or
          $outputFull.StartsWith($distRootFull + "\", [System.StringComparison]::OrdinalIgnoreCase))) {
    throw "OutputDir must be inside the repository dist directory: $distRootFull"
}

Push-Location $root
try {
    & $lrelease "resources\i18n\visionselect_zh_CN.ts" "resources\i18n\visionselect_en_US.ts"
    if (-not $SkipBuild) {
        & ".\build_msvc2015.bat"
        if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
    }
    if (-not (Test-Path -LiteralPath $exe)) { throw "Application executable not found: $exe" }

    if (Test-Path -LiteralPath $outputFull) {
        Remove-Item -LiteralPath $outputFull -Recurse -Force
    }
    New-Item -ItemType Directory -Path $outputFull -Force | Out-Null
    Copy-Item -LiteralPath $exe -Destination $outputFull
    & $windeployqt (Join-Path $outputFull "VisionSelect.exe") --release --no-translations --compiler-runtime
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE" }

    $missingRuntime = @($vcRuntimeDlls | Where-Object { -not (Test-Path -LiteralPath (Join-Path $outputFull $_)) })
    if ($missingRuntime.Count -gt 0 -and (Test-Path -LiteralPath $vcRuntimeDir)) {
        foreach ($dll in $missingRuntime) {
            $sourceDll = Join-Path $vcRuntimeDir $dll
            if (Test-Path -LiteralPath $sourceDll) {
                Copy-Item -LiteralPath $sourceDll -Destination $outputFull -Force
            }
        }
    }
    $missingRuntime = @($vcRuntimeDlls | Where-Object { -not (Test-Path -LiteralPath (Join-Path $outputFull $_)) })
    if ($missingRuntime.Count -gt 0) {
        throw "MSVC 2015 runtime DLLs were not deployed: $($missingRuntime -join ', '). Install Visual Studio 2015 Build Tools or VC++ Redistributable, then run this script again."
    }

    if (-not $SkipInstaller) {
        $iscc = Get-Command $InnoSetupCompiler -ErrorAction SilentlyContinue
        if (-not $iscc) {
            throw "Inno Setup compiler was not found. Install Inno Setup or pass -InnoSetupCompiler with the full path to ISCC.exe."
        }
        & $iscc.Source (Join-Path $root "installer\VisionSelect.iss")
        if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE" }
    }

    Write-Host "Package output: $outputFull"
}
finally {
    Pop-Location
}
