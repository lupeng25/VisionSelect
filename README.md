# VisionSelect

VisionSelect 是一个基于 Qt Widgets 的工业机器视觉元器件选型工具。项目使用 C++20、Qt 6 和 CMake，包含相机、镜头、光源目录管理，二维选型、三维相机辅助评估、PDF 报告及许可证生成工具。

## 本机构建

本机已安装 Qt 6.10.1 MinGW，路径为 `D:\Qt6\6.10.1\mingw_64`；MinGW 工具链路径为 `D:\Qt6\Tools\mingw1310_64`。在 PowerShell 中执行：

```powershell
$env:QT_ROOT = 'D:\Qt6\6.10.1\mingw_64'
$env:MINGW_ROOT = 'D:\Qt6\Tools\mingw1310_64'
.\build.ps1 -Toolchain MinGW -Configuration Debug -Test
```

应用程序位于 `build\windows-mingw64\bin\VisionSelect.exe`，测试程序位于 `build\windows-mingw64\bin\VisionSelectTests.exe`。

## MSVC 构建与发布

安装 Qt 6.10.1 的 MSVC 2022 x64 套件并设置 `QT_ROOT` 后，在已初始化的 Visual Studio 2022 x64 开发者终端中执行：

```powershell
.\build.ps1 -Toolchain MSVC -Configuration Release -Test -Install
```

可部署目录会生成在 `dist\VisionSelect`。使用 Inno Setup 编译 `installer\VisionSelect.iss` 可生成安装包。

## 编码约定

所有项目文本文件必须是 UTF-8。修改中文界面、CSV 或文档后，执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_text_encoding.ps1
```
