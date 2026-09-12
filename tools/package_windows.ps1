param(
    [string]$BuildDirectory = 'build/windows-msvc2022-release',
    [string]$OutputDirectory = 'dist/VisionSelect',
    [string]$InstallerCompiler = '',
    [switch]$SkipBuild,
    [ValidateSet('Runtime','LicenseTools')][string]$Component = 'Runtime'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    $buildPath = (Resolve-Path -LiteralPath $BuildDirectory).Path
    $cache = Get-Content -LiteralPath (Join-Path $buildPath 'CMakeCache.txt') -Raw
    if ($cache -notmatch 'CMAKE_BUILD_TYPE:STRING=Release') { throw '打包必须使用 Release 构建。' }
    $version = [regex]::Match($cache, '(?m)^CMAKE_PROJECT_VERSION:STATIC=([^\r\n]+)').Groups[1].Value
    if (-not $version) { throw '无法从 CMake 缓存读取项目版本。' }
    if (-not $SkipBuild) {
        cmake --build $buildPath --parallel
        if ($LASTEXITCODE -ne 0) { throw '构建失败。' }
    }
    # 不递归删除既有发布目录；在隔离目录验收后再同步指定发布文件。
    $stage = Join-Path $projectRoot ('.codex_tmp/package-' + [guid]::NewGuid().ToString('N'))
    cmake --install $buildPath --prefix $stage --component $Component
    if ($LASTEXITCODE -ne 0) { throw 'CMake 安装部署失败。' }
    $exe = if ($Component -eq 'Runtime') { 'VisionSelect.exe' } else { 'VisionSelectLicenseGenerator.exe' }
    if (-not (Test-Path -LiteralPath (Join-Path $stage "bin/$exe"))) { throw '安装目录缺少可执行文件。' }
    $outputPath = if ([IO.Path]::IsPathRooted($OutputDirectory)) { [IO.Path]::GetFullPath($OutputDirectory) }
        else { [IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDirectory)) }
    New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
    Get-ChildItem -LiteralPath $stage | Copy-Item -Destination $outputPath -Recurse -Force
    if ($InstallerCompiler) {
        if ($Component -ne 'Runtime') { throw '许可证工具只生成独立目录。' }
        & $InstallerCompiler "/DMyAppVersion=$version" "/DMyAppSource=$stage" (Join-Path $projectRoot 'installer/VisionSelect.iss')
        if ($LASTEXITCODE -ne 0) { throw '安装器编译失败。' }
    }
    Write-Output "发布目录：$outputPath；版本：$version；组件：$Component"
} finally { Pop-Location }
