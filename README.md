# VisionSelect 工程工作台

面向工业机器视觉选型的本地桌面应用，使用 **Rust + React/TypeScript + Tauri 2**。围绕设备组合与现场条件工作，修改参数后即时校核二维成像或三维轮廓采样。新工程不再依赖 Qt、C++ 或 CMake。

## 当前能力

- 四套可切换皮肤：石墨、瓷白、暖砂、雾青。点击左下角“外观皮肤”即时切换，重启后自动恢复；页面、图示和原生标题栏同步适配。界面采用柔和层次与短时过渡，并支持系统减少动态效果偏好。
- 四类硬件资料检索：1,393 款相机、1,004 款镜头、2,180 款光源、267 款三维相机，共 4,844 条。
- 按硬件分类组合筛选规格，支持接口、类型、数值区间及“未公开”条件；常用条件直接显示，更多条件展开查看，可单项移除或一键清空。资料库与工作台选设备弹窗共用此功能，详见 [硬件规格筛选方案](docs/硬件规格筛选方案.md)。
- 二维成像：视场、像素当量、接口、像圈、工作距离、景深条件、运动拖影和传输载荷。
- 独立 2D 选型计算：最低分辨率、视场、定焦与远心倍率反算、运动曝光、传输与存储；支持手动输入、读取当前方案和明确应用参数，计算草稿独立保留。使用方法与计算边界见 [二维选型计算方案](docs/二维选型计算方案.md)。
- 三维采样：量程、重复精度、公开截面覆盖、轮廓频率、曝光周期与编码器采样。快照设备不套用轮廓扫描公式。
- 本地 SQLite 方案保存、设备参数快照、参考方案对照、JSON 导入导出。
- CSV、三维 JSON 和旧版 SQLite 产品库的预览合并，提交前自动备份。
- 导出中文 HTML 校核记录，可用浏览器打印为 PDF。

未公开规格显示“待确认”。物方像素当量并不等于测量精度，目录光源尺寸也不代表有效照明范围。方案不控制真实相机或运动设备。

## Windows 开发

本次重建使用 Windows、Rust 1.96 MSVC 工具链、Node.js 24 和 PowerShell 7。需要 Microsoft C++ 构建工具及 Windows SDK 为 Rust 链接本机程序；运行桌面程序需要 WebView2。项目自身源码使用 Rust 与 TypeScript。

```powershell
npm ci
npm run desktop
```

桌面模式由 Tauri 直接调用 Rust 核心，不需要单独运行 API 服务。用于浏览器开发和测试时，在两个终端分别运行：

```powershell
npm run dev:api
```

```powershell
npm run dev
```

浏览器地址为 http://127.0.0.1:1420，开发 API 只绑定 127.0.0.1:4318。浏览器预览也调用真实 Rust 核心。默认开发数据位于 `.codex_tmp/rust-dev-data`，可用环境变量 `VISIONSELECT_DATA_DIR` 指定隔离数据目录。

## 验证与打包

```powershell
cargo fmt --all --check
cargo test --workspace
npm test
npm run build
npx playwright install chromium
npm run test:e2e
pwsh -NoProfile -File tools/check_text_encoding.ps1
npm run desktop:build
```

端到端测试自动启动 Rust API 和 Vite；运行前请停止占用上述两个端口的开发服务。测试数据使用独立的 `.codex_tmp/e2e-data` 目录，不读取桌面用户数据。

桌面程序输出到 `target/release/visionselect-desktop.exe`，Windows 安装包输出到 `target/release/bundle/nsis/`。安装包会在缺少 WebView2 时引导安装运行时；完全离线环境需事先准备 WebView2。

窗口关闭回归可在本机 Windows 桌面会话中运行 `pwsh -NoProfile -File tools/test_desktop_close.ps1`；指定其他构建时使用 `-Executable` 参数。脚本只操作自行启动的隔离验证窗口，检查直接关闭、保存后关闭以及未保存时的确认和取消。

皮肤桌面回归使用 `pwsh -NoProfile -File tools/test_desktop_themes.ps1`，在隔离环境中检查四套皮肤、原生标题栏明暗同步与退出重开恢复。设计与验证范围见 [主题皮肤方案](docs/主题皮肤方案.md)。

## 数据保护与迁移

`resources/data/` 的 5 个原始硬件资料文件完整保留，不参与界面或算法重写。内置数据首次启动时导入 SQLite；用户导入与方案保存不修改这些文件。`coolens_lenses_raw.csv` 作为原始来源资料保留，不重复计入镜头数量。

桌面用户数据默认位于系统应用数据目录下的 `com.visionselect.workbench`，具体路径可在“工作台说明”中查看；桌面模式也支持通过 `VISIONSELECT_DATA_DIR` 环境变量指定隔离数据目录。首次打开旧项目时，请在硬件资料库选择“导入硬件”，导入旧 SQLite 产品库或三维 JSON。先查看新增、更新数量，确认后合并；旧文件保持原样，新库备份写在同一用户目录内。

保存的方案包含设备参数快照，后续更新硬件库不会静默改变已有方案。工作中的有效参数会保存为当前环境的本地草稿；正式方案使用“保存方案”写入 SQLite。

## 工程结构

| 路径 | 职责 |
| --- | --- |
| `crates/vision-core` | 方案模型、目录、校核、导入和 SQLite 持久化 |
| `crates/vision-server` | 浏览器开发与交互测试使用的本机 API |
| `src-tauri` | 桌面启动、原生文件对话框与 Rust 命令 |
| `src` | React 工程工作台与界面样式 |
| `resources/data` | 保留的硬件原始数据 |
| `tests/e2e` | 连接真实 Rust API 的交互回归 |

重建边界见 [重建方案](docs/重建方案.md)，测试范围见 [重建验证记录](docs/重建验证记录.md)。旧源码和重建前未提交改动已在本机 `.codex_tmp/rust-rebuild-backup/` 归档；该目录不提交、不参与新工程构建。
