# VisionSelect

VisionSelect 是一个基于 Qt Widgets 的工业机器视觉元器件选型工具。项目唯一支持的技术基线为 C++20、Qt 6.10.1 和 CMake，包含相机、镜头、光源目录管理、二维选型、三维相机辅助评估、PDF 报告及许可证生成工具。

## 目录约定

- `build/windows-*/`：CMake 开发构建目录，可随时删除并重新生成；可执行文件位于其 `bin/` 子目录。
- `dist/VisionSelect/`：通过 `cmake --install` 生成的可发布目录。
- 根目录不再使用 `bin/`，项目也不再包含 qmake `.pro` 文件或 Qt 5 构建脚本。

## MinGW 13.1 构建

在 PowerShell 中设置 Qt 6.10.1 和 MinGW 13.1 路径，然后使用统一构建脚本：

```powershell
$env:QT_ROOT = 'D:\Qt6\6.10.1\mingw_64'
$env:MINGW_ROOT = 'D:\Qt6\Tools\mingw1310_64'
.\build.ps1 -Toolchain MinGW -Configuration Debug -Test
.\build.ps1 -Toolchain MinGW -Configuration Release -Test -Install
```

Debug 应用位于 `build/windows-mingw64/bin/VisionSelect.exe`，Release 应用位于 `build/windows-mingw64-release/bin/VisionSelect.exe`。安装步骤统一输出到 `dist/VisionSelect/`。

## MSVC 2022 构建

安装 Qt 6.10.1 MSVC 2022 x64 套件，在 Visual Studio 2022 x64 开发者 PowerShell 中执行：

```powershell
$env:QT_ROOT = 'D:\Qt6\6.10.1\msvc2022_64'
.\build.ps1 -Toolchain MSVC -Configuration Release -Test -Install
```

也可以直接使用 `CMakePresets.json` 中的 `windows-mingw64`、`windows-mingw64-release`、`windows-msvc2022` 和 `windows-msvc2022-release` 预设。

## 测试

CTest 会自动把 Qt 运行库目录加入测试进程的 `PATH`，并使用 Windows 平台插件运行：

- `VisionSelectTests`：选型算法、候选排序、BOM、CSV、PDF 和许可证等非视觉逻辑。
- `VisionSelectUiTests`：相对匹配度、主流程、状态持久化、目录工具栏、键盘焦点和无障碍属性。

单独运行当前 MinGW Debug 测试：

```powershell
ctest --test-dir build/windows-mingw64 --output-on-failure
```

## UI 设置

窗口几何、侧栏偏好、舒适/紧凑密度、分隔器、表头和 3D 高级筛选折叠状态由 `QSettings` 持久化。普通模式使用工业浅色主题，高对比模式跟随 Windows 系统对比度设置。

## 编码约定

所有项目文本文件均使用 UTF-8。修改中文界面、CSV 或文档后执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_text_encoding.ps1
```
