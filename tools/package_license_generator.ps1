param([string]$BuildDirectory='build/windows-msvc2022-release', [string]$OutputDirectory='dist/VisionSelectLicenseGenerator', [switch]$SkipBuild)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'package_windows.ps1') -BuildDirectory $BuildDirectory -OutputDirectory $OutputDirectory -Component LicenseTools -SkipBuild:$SkipBuild
