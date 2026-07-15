# VisionSelect Agent 指南

## 项目概览

- VisionSelect 是使用 C++20 和 Qt 6.10.1 Widgets 开发的工业机器视觉元器件选型应用。
- 项目仅使用 CMake；根项目入口为 `CMakeLists.txt`，标准配置位于 `CMakePresets.json`。
- 支持的 Windows 工具链为 MinGW 13.1 和 MSVC 2022 x64。
- 开发产物位于 `build/windows-*/bin/`，发布产物通过安装步骤输出到 `dist/VisionSelect/`；不要在根目录创建 `bin/`。

## 构建与测试

- MinGW Debug：`.\build.ps1 -Toolchain MinGW -Configuration Debug -Test`。
- MinGW Release：`.\build.ps1 -Toolchain MinGW -Configuration Release -Test`。
- MSVC Release：在 Visual Studio 2022 x64 开发者 PowerShell 中执行 `.\build.ps1 -Toolchain MSVC -Configuration Release -Test`。
- 发布：在任一 Release 命令后增加 `-Install`，产物写入 `dist/VisionSelect/`。
- 算法测试程序为 `VisionSelectTests`，UI 测试程序为 `VisionSelectUiTests`；统一通过 CTest 运行。
- 本地 Qt 6.10.1 MinGW 示例路径：`D:\Qt6\6.10.1\mingw_64`；MinGW 示例路径：`D:\Qt6\Tools\mingw1310_64`。

## 模块说明

- `src/catalog/`：相机、镜头和光源目录的加载、查询与持久化。
- `src/core/`：共享选型数据类型、标签、解析和兼容性辅助函数。
- `src/selection/`：选型引擎、评分、需求估算和计算辅助逻辑。
- `src/ui/`：Qt Widgets 页面、导航、主题、状态持久化和用户工作流。
- `src/three_d/`：3D 相机目录、匹配和采样计算。
- `src/report/`：PDF 报告生成。
- `tests/test_selection.cpp`：算法与导出回归测试。
- `tests/test_ui.cpp`：UI 表现层、状态和无障碍测试。

## 工作规则

- 修改前执行 `git status --short`，保留用户已有改动，不回退无关内容。
- 不编辑生成的 Ninja、Makefile 或其他构建目录文件。
- 所有项目文本使用 UTF-8。修改中文 UI、CSV 数据或文档后运行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_text_encoding.ps1`。
- 选型逻辑和计算行为优先在 `tests/test_selection.cpp` 中补测试；UI 状态与交互行为放在 `tests/test_ui.cpp`。
- UI 改动需在最终说明中记录导航、布局、文字适配和手工视觉验证情况。
- 保持改动范围聚焦，并沿用现有 Qt 6/CMake/C++20 风格。
