param(
    [string]$Executable = (Join-Path $PSScriptRoot '../target/release/visionselect-desktop.exe')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class VisionSelectThemeProbe {
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("dwmapi.dll")]
    public static extern int DwmGetWindowAttribute(IntPtr window, int attribute, out int value, int size);
}
'@

$executablePath = (Resolve-Path -LiteralPath $Executable).Path
$workspacePath = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$runDirectory = Join-Path $workspacePath ('.codex_tmp/desktop-themes/' + [guid]::NewGuid())
$previousData = $env:VISIONSELECT_DATA_DIR
$previousWebview = $env:WEBVIEW2_USER_DATA_FOLDER
$ownedProcess = $null

function Wait-Until([scriptblock]$Probe, [string]$Message) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt 20) {
        $result = & $Probe
        if ($result) { return $result }
        Start-Sleep -Milliseconds 100
    }
    throw $Message
}

function Find-Control([string]$Name, [System.Windows.Automation.ControlType]$Type) {
    $ownedProcess.Refresh()
    if ($ownedProcess.HasExited -or $ownedProcess.MainWindowHandle -eq [IntPtr]::Zero) { return $null }
    $root = [System.Windows.Automation.AutomationElement]::FromHandle($ownedProcess.MainWindowHandle)
    $condition = [System.Windows.Automation.AndCondition]::new(
        [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty, $Name),
        [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty, $Type))
    $root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $condition)
}

function Click-Button([string]$Name) {
    $control = Wait-Until { Find-Control $Name ([System.Windows.Automation.ControlType]::Button) } "未找到按钮：$Name"
    $pattern = $null
    if ($control.TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern, [ref]$pattern)) { $pattern.Invoke() }
    elseif ($control.TryGetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern, [ref]$pattern)) { $pattern.Expand() }
    else { throw "按钮未提供可调用的操作模式：$Name" }
}

function Select-Theme([string]$Name, [int]$Dark) {
    $control = Wait-Until { Find-Control $Name ([System.Windows.Automation.ControlType]::RadioButton) } "未找到皮肤：$Name"
    $control.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Select()
    Assert-Theme $Name $Dark
}

function Assert-Theme([string]$Name, [int]$Dark) {
    Wait-Until {
        $control = Find-Control $Name ([System.Windows.Automation.ControlType]::RadioButton)
        $null -ne $control -and $control.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Current.IsSelected
    } "皮肤未被选中：$Name" | Out-Null
    Wait-Until {
        $value = 0
        # DWMWA_USE_IMMERSIVE_DARK_MODE：检查实际原生标题栏，不依赖网页状态。
        $result = [VisionSelectThemeProbe]::DwmGetWindowAttribute($ownedProcess.MainWindowHandle, 20, [ref]$value, 4)
        $result -eq 0 -and $value -eq $Dark
    } "原生标题栏没有同步到 $Name 的明暗模式。" | Out-Null
}

function Start-OwnedWindow([switch]$SaveBaseline) {
    $script:ownedProcess = Start-Process -FilePath $executablePath -WorkingDirectory $workspacePath -WindowStyle Hidden -PassThru
    if ($SaveBaseline) {
        # 先保存空白方案，避免重开时恢复的未保存草稿触发正常关闭确认。
        Click-Button '保存方案'
        Wait-Until { Find-Control '已保存' ([System.Windows.Automation.ControlType]::Text) } '验证基准方案未保存。' | Out-Null
    }
    Click-Button '外观皮肤'
    Write-Output "已打开本次验证窗口：$($ownedProcess.Id)"
}

function Close-OwnedWindow {
    Click-Button '完成'
    $ownedProcess.Refresh()
    Write-Output "正在关闭本次验证窗口：$($ownedProcess.Id)"
    [VisionSelectThemeProbe]::PostMessage($ownedProcess.MainWindowHandle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    Wait-Until { $ownedProcess.Refresh(); $ownedProcess.HasExited } '切换外观后未能直接关闭，或错误标记了方案修改。' | Out-Null
    $script:ownedProcess = $null
}

try {
    $env:VISIONSELECT_DATA_DIR = Join-Path $runDirectory 'data'
    $env:WEBVIEW2_USER_DATA_FOLDER = Join-Path $runDirectory 'webview'
    Start-OwnedWindow -SaveBaseline
    Assert-Theme '石墨' 1
    Select-Theme '暖砂' 0
    Select-Theme '雾青' 0
    Select-Theme '石墨' 1
    Select-Theme '瓷白' 0
    Close-OwnedWindow
    # 复用本次隔离目录，模拟完全退出后的真实重开。
    Start-OwnedWindow
    Assert-Theme '瓷白' 0
    Select-Theme '石墨' 1
    Close-OwnedWindow
    Write-Output '实际桌面：四种皮肤切换、原生标题栏同步、退出重开恢复与直接关闭全部通过。'
} finally {
    if ($null -ne $ownedProcess) {
        $ownedProcess.Refresh()
        if (-not $ownedProcess.HasExited) { Stop-Process -Id $ownedProcess.Id }
    }
    $env:VISIONSELECT_DATA_DIR = $previousData
    $env:WEBVIEW2_USER_DATA_FOLDER = $previousWebview
}
