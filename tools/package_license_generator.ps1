param(
    [string]$QtDir = "C:\Qt\Qt5.12.9\5.12.9\msvc2015_64",
    [string]$OutputDir = "dist\LicenseKeyGenerator",
    [string]$BuildDir = "build\license_key_generator",
    [string]$Jom = "C:\Qt\Qt5.12.9\Tools\QtCreator\bin\jom\jom.exe",
    [string]$VcVars = "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat",
    [string]$WindowsSdkRoot = "C:\Program Files (x86)\Windows Kits\10",
    [string]$WindowsSdkVersion = "10.0.18362.0"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$distRoot = Join-Path $root "dist"
$buildRoot = Join-Path $root "build"
$output = if ([System.IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $root $OutputDir }
$build = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $root $BuildDir }
$distRootFull = [System.IO.Path]::GetFullPath($distRoot).TrimEnd('\')
$buildRootFull = [System.IO.Path]::GetFullPath($buildRoot).TrimEnd('\')
$outputFull = [System.IO.Path]::GetFullPath($output).TrimEnd('\')
$buildFull = [System.IO.Path]::GetFullPath($build).TrimEnd('\')
$project = Join-Path $root "tools\license_key_generator\license_key_generator.pro"
$exe = Join-Path $root "bin\LicenseKeyGenerator.exe"
$qmake = Join-Path $QtDir "bin\qmake.exe"
$windeployqt = Join-Path $QtDir "bin\windeployqt.exe"
$sdkBin = Join-Path $WindowsSdkRoot "bin\$WindowsSdkVersion\x64"
$sdkInclude = Join-Path $WindowsSdkRoot "include\$WindowsSdkVersion"
$sdkLib = Join-Path $WindowsSdkRoot "Lib\$WindowsSdkVersion"
$vcRuntimeDir = "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\redist\x64\Microsoft.VC140.CRT"
$vcRuntimeDlls = @("vcruntime140.dll", "msvcp140.dll", "concrt140.dll")

if (-not (Test-Path -LiteralPath $qmake)) { throw "qmake not found: $qmake" }
if (-not (Test-Path -LiteralPath $windeployqt)) { throw "windeployqt not found: $windeployqt" }
if (-not (Test-Path -LiteralPath $Jom)) { throw "jom not found: $Jom" }
if (-not (Test-Path -LiteralPath $VcVars)) { throw "vcvarsall.bat not found: $VcVars" }
if (-not (Test-Path -LiteralPath (Join-Path $sdkInclude "ucrt"))) { throw "Windows SDK include directory not found: $sdkInclude" }
if (-not (Test-Path -LiteralPath (Join-Path $sdkLib "ucrt\x64"))) { throw "Windows SDK library directory not found: $sdkLib" }
if (-not ($outputFull.Equals($distRootFull, [System.StringComparison]::OrdinalIgnoreCase) -or
          $outputFull.StartsWith($distRootFull + "\", [System.StringComparison]::OrdinalIgnoreCase))) {
    throw "OutputDir must be inside the repository dist directory: $distRootFull"
}
if (-not ($buildFull.Equals($buildRootFull, [System.StringComparison]::OrdinalIgnoreCase) -or
          $buildFull.StartsWith($buildRootFull + "\", [System.StringComparison]::OrdinalIgnoreCase))) {
    throw "BuildDir must be inside the repository build directory: $buildRootFull"
}

if (Test-Path -LiteralPath $buildFull) {
    Remove-Item -LiteralPath $buildFull -Recurse -Force
}
New-Item -ItemType Directory -Path $buildFull -Force | Out-Null

$cmdFile = Join-Path $buildFull "build_license_key_generator.cmd"
@"
@echo off
call "$VcVars" amd64
if errorlevel 1 exit /b 1
set "PATH=$sdkBin;$QtDir\bin;%PATH%"
set "INCLUDE=$sdkInclude\ucrt;$sdkInclude\shared;$sdkInclude\um;$sdkInclude\winrt;%INCLUDE%"
set "LIB=$sdkLib\ucrt\x64;$sdkLib\um\x64;%LIB%"
"$qmake" "$project" -spec win32-msvc CONFIG+=release
if errorlevel 1 exit /b 1
"$Jom" release
exit /b %ERRORLEVEL%
"@ | Set-Content -LiteralPath $cmdFile -Encoding ASCII

Push-Location $buildFull
try {
    & cmd.exe /c $cmdFile
    if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
}
finally {
    Pop-Location
}

if (-not (Test-Path -LiteralPath $exe)) { throw "LicenseKeyGenerator.exe was not built: $exe" }

if (Test-Path -LiteralPath $outputFull) {
    Remove-Item -LiteralPath $outputFull -Recurse -Force
}
New-Item -ItemType Directory -Path $outputFull -Force | Out-Null
Copy-Item -LiteralPath $exe -Destination $outputFull

& $windeployqt (Join-Path $outputFull "LicenseKeyGenerator.exe") --release --no-translations --compiler-runtime
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

Write-Host "License generator package output: $outputFull"
