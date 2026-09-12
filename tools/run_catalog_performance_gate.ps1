param([string]$QtBin='', [string]$TestExe='build/windows-msvc2022-release/bin/VisionSelectTests.exe', [int]$Runs=3)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    if (-not $QtBin -and $env:QT_ROOT) { $QtBin=Join-Path $env:QT_ROOT 'bin' }
    if ($QtBin) { $env:PATH="$QtBin;$env:PATH" }
    if (-not (Test-Path -LiteralPath $TestExe)) { throw '未找到测试程序，请先构建对应 Release 配置。' }
    if ($Runs -lt 1) { throw '重复次数必须大于零。' }
    $env:VISIONSELECT_PERF_GATE='1'
    for($run=1;$run -le $Runs;$run++) {
        Write-Output "大目录性能验证 $run/$Runs"
        $logPath = Join-Path (Split-Path -Parent (Resolve-Path -LiteralPath $TestExe).Path) "performance-gate-$run.txt"
        & $TestExe catalogPerformanceGate -o "$logPath,txt"
        $testExitCode = $LASTEXITCODE
        if(Test-Path -LiteralPath $logPath) { Get-Content -LiteralPath $logPath -Encoding utf8 }
        if($testExitCode -ne 0) { throw '性能门槛未通过。' }
    }
} finally { Pop-Location }
