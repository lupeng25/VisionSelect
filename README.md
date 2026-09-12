# VisionSelect

VisionSelect 是一个基于 Qt Widgets 的工业机器视觉元器件选型工具。项目唯一支持的技术基线为 C++20、Qt 6.10.1 和 CMake，包含相机、镜头、光源目录管理、二维选型、三维相机辅助评估、PDF 报告及许可证生成工具。

Qt SDK 需包含 Qt SVG 和 LinguistTools（Qt Tools 中的 `lrelease`）。中英文 QM 从 TS 随 CMake 构建生成，无需手动复制旧翻译文件。

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

## 发布与目录导入

主程序和许可证生成器使用独立安装组件，发布主程序不会附带许可证生成器。Release 构建通过测试后，可执行：

```powershell
.\tools\package_windows.ps1 -BuildDirectory build/windows-msvc2022-release -SkipBuild
.\tools\package_license_generator.ps1 -BuildDirectory build/windows-msvc2022-release -SkipBuild
```

主程序输出到 `dist/VisionSelect/bin/VisionSelect.exe`；许可证工具默认输出到独立目录。指定 `-InstallerCompiler` 为 Inno Setup 的 `ISCC.exe` 可生成安装器，版本取自 CMake 项目版本。两个安装组件均部署 Qt 运行库；MSVC 构建另部署应用本地 C++ 运行库。

界面导入 CSV 默认合并，以厂家和型号确定身份，预览显示新增、更新和删除数量。整类替换需要明确选择“替换”。提交前自动备份 SQLite 产品库，成功消息显示备份位置。旧 `load*Csv()` API 仍用于内部整类加载，交互入口应使用带明确模式的 `importCsv()`。

相机 CSV 可提供 `pixel_format`、`bandwidth_source`（`specified` / `estimated` / `unknown`）、`source_url` 和 `source_date`。历史 CSV 缺少来源时，带宽按估算值处理；不能仅凭接口名称确认吞吐能力。镜头 `dof_conditions_confirmed` 默认关闭，只有确认目录景深适用于当前光圈、倍率和清晰度要求时才开启。

全项目审查及修复记录见 [审查报告](docs/project_review_2026-09-12.md) 和 [修复说明](docs/project_review_fixes_2026-09-12.md)。

## 测试

CTest 会自动把 Qt 运行库目录加入测试进程的 `PATH`，并使用 Windows 平台插件运行：

- `VisionSelectTests`：选型算法、候选排序、BOM、CSV、PDF 和许可证等非视觉逻辑。
- `VisionSelectUiTests`：相对匹配度、主流程、状态持久化、目录工具栏、键盘焦点和无障碍属性。

CTest 会保存并转发 QtTest 文本日志，避免部分 Windows 非交互环境缺少标准输出。十万条目录门槛单独执行 `tools/run_catalog_performance_gate.ps1 -Runs 3`，每轮日志保存在测试程序目录的 `performance-gate-轮次.txt`。

单独运行当前 MinGW Debug 测试：

```powershell
ctest --test-dir build/windows-mingw64 --output-on-failure
```

## UI 设置

窗口几何、侧栏偏好、舒适/紧凑密度、分隔器、表头和 3D 高级筛选折叠状态由 `QSettings` 持久化。普通模式使用石墨黑导航、银灰工作区和低饱和蓝紫强调色，高对比模式跟随 Windows 系统对比度设置。Qt 安装需包含 Qt SVG 模块，以显示矢量控件图标。

需求页使用参数编辑与实时成像目标双栏布局，视场示意随工件尺寸和装夹余量更新；窄窗口自动折叠导航，长表单和 3D 高级筛选使用局部滚动。界面设计与验证说明见 `docs/ui_redesign.md`。

“参数校算”提供六任务工作台：视场与焦距、分辨率与采样、方案校核、远心倍率、运动与曝光、带宽与存储。支持产品库参数导入、正反算、未知状态、实测视场、单位切换及独立 A/B 快照。使用方式、模型区别和验证范围见 [工作台说明](docs/parameter_workbench.md)。

## 编码约定

所有项目文本文件均使用 UTF-8。修改中文界面、CSV 或文档后执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_text_encoding.ps1
```
