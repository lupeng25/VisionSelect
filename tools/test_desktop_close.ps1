param(
    [string]$Executable = (Join-Path $PSScriptRoot "../target/release/visionselect-desktop.exe"),
    [switch]$VerifyOldBug
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class VisionSelectCloseProbe {
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
}
'@

$executablePath = (Resolve-Path -LiteralPath $Executable).Path
$workspacePath = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$runDirectory = Join-Path $workspacePath (".codex_tmp/desktop-close/" + [guid]::NewGuid().ToString())
New-Item -Path $runDirectory -ItemType Directory -Force | Out-Null
$previousData = $env:VISIONSELECT_DATA_DIR
$previousWebview = $env:WEBVIEW2_USER_DATA_FOLDER
$ownedProcess = $null

function Wait-Until([scriptblock]$Probe, [string]$Message, [int]$Seconds = 20) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $Seconds) {
        $value = & $Probe
        if ($value) { return $value }
        Start-Sleep -Milliseconds 100
    }
    throw $Message
}

function Find-Control([string]$Name, [System.Windows.Automation.ControlType]$Type) {
    $ownedProcess.Refresh()
    if ($ownedProcess.HasExited -or $ownedProcess.MainWindowHandle -eq [IntPtr]::Zero) { return $null }
    $root = [System.Windows.Automation.AutomationElement]::FromHandle($ownedProcess.MainWindowHandle)
    if ($null -eq $root) { return $null }
    $nameCondition = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::NameProperty, $Name)
    if ($null -ne $Type) {
        $typeCondition = [System.Windows.Automation.PropertyCondition]::new(
            [System.Windows.Automation.AutomationElement]::ControlTypeProperty, $Type)
        $condition = [System.Windows.Automation.AndCondition]::new($nameCondition, $typeCondition)
    } else { $condition = $nameCondition }
    $root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $condition)
}

function Click-Button([string]$Name) {
    $button = Wait-Until { Find-Control $Name ([System.Windows.Automation.ControlType]::Button) } "未找到按钮：$Name"
    $pattern = $button.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    $pattern.Invoke()
}

function Request-Close {
    $ownedProcess.Refresh()
    if (-not [VisionSelectCloseProbe]::PostMessage(
        $ownedProcess.MainWindowHandle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)) {
        throw "向本次验证窗口发送关闭请求失败"
    }
}

try {
    $scenarios = if ($VerifyOldBug) { @("旧版复现") } else { @("空白方案", "已保存方案", "未保存确认") }
    foreach ($scenario in $scenarios) {
        $scenarioDirectory = Join-Path $runDirectory $scenario
        $env:VISIONSELECT_DATA_DIR = Join-Path $scenarioDirectory "data"
        $env:WEBVIEW2_USER_DATA_FOLDER = Join-Path $scenarioDirectory "webview"
        $ownedProcess = Start-Process -FilePath $executablePath -WorkingDirectory $workspacePath -WindowStyle Hidden -PassThru
        $nameInput = Wait-Until {
            Find-Control "方案名称" ([System.Windows.Automation.ControlType]::Edit)
        } "桌面工作台未能正常启动"

        if ($scenario -in @("已保存方案", "未保存确认")) {
            $value = $nameInput.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
            $value.SetValue("关闭回归-" + $scenario)
            Wait-Until { Find-Control "未保存" $null } "修改方案后未出现未保存状态" | Out-Null
        }
        if ($scenario -eq "已保存方案") {
            Click-Button "保存方案"
            Wait-Until { Find-Control "已保存" $null } "方案未保存成功" | Out-Null
        }

        Request-Close
        if ($VerifyOldBug) {
            if ($ownedProcess.WaitForExit(2000)) { throw "旧程序正常退出，未复现本次权限问题" }
            Write-Output "已复现：工作台就绪后发送关闭请求，旧程序仍未退出。"
        } else {
            if ($scenario -eq "未保存确认") {
                Wait-Until { Find-Control "关闭当前方案？" $null } "关闭未保存方案时没有确认提示" | Out-Null
                Click-Button "返回"
                Start-Sleep -Milliseconds 200
                if ($ownedProcess.HasExited) { throw "返回编辑时程序意外退出" }
                # 模态框会令 WebView2 重建无障碍节点，返回后重新取得输入控件。
                $restoredInput = Wait-Until {
                    Find-Control "方案名称" ([System.Windows.Automation.ControlType]::Edit)
                } "取消关闭后未恢复编辑控件"
                $restoredValue = $restoredInput.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).Current.Value
                if ($restoredValue -ne ("关闭回归-" + $scenario)) { throw "取消关闭后编辑内容发生变化：$restoredValue" }
                Request-Close
                Wait-Until { Find-Control "关闭当前方案？" $null } "再次关闭时没有确认提示" | Out-Null
                Click-Button "继续操作"
            }
            if (-not $ownedProcess.WaitForExit(8000)) { throw "$scenario：请求关闭后进程未退出" }
            Write-Output "$scenario：实际桌面关闭通过。"
        }
        # 仅清理本脚本启动的隔离验证进程；不按名称查找或终止用户程序。
        if (-not $ownedProcess.HasExited) { Stop-Process -Id $ownedProcess.Id }
        $ownedProcess = $null
    }
} finally {
    if ($null -ne $ownedProcess -and -not $ownedProcess.HasExited) { Stop-Process -Id $ownedProcess.Id }
    $env:VISIONSELECT_DATA_DIR = $previousData
    $env:WEBVIEW2_USER_DATA_FOLDER = $previousWebview
}
