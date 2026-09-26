# VisionSelect Agent 指南

## 工程基线

- 使用中文沟通，所有项目文档使用中文，文本编码为 UTF-8。
- PowerShell 优先使用 7。
- 新架构为 Rust + React/TypeScript + Tauri 2；不再引入 C++、Qt 或 CMake 工程。
- Rust 核心位于 crates/vision-core，开发 API 位于 crates/vision-server，桌面封装位于 src-tauri，前端位于 src。
- resources/data 是保留的硬件原始资料，未经用户要求不得改写。用户产品库必须与内置数据分离。

## 工作与验证

- 修改前检查 git status --short，保留用户未提交改动。
- 简单改动简短计划；跨模块、数据、公共接口和发布相关任务写明目标、依赖、验证与退出条件。
- Rust 校核与持久化测试写在 vision-core；交互测试使用 tests/e2e，并连接真实 Rust API。
- 常规检查：cargo fmt --all --check；cargo test --workspace；npm run build；npm test；npm run test:e2e。
- 端到端测试要求 1420、4318 端口空闲；测试数据位于 .codex_tmp/e2e-data，不复用桌面用户库。
- 桌面构建：npm run desktop:build。Windows 需要 Rust MSVC 工具链、Microsoft 构建工具及 WebView2；无需 Qt SDK。
- 修改中文、CSV 或文档后运行 pwsh -NoProfile -File tools/check_text_encoding.ps1。
- UI 改动最终记录导航、布局、文字适配和视觉验证情况。
- 构建产物位于 target 和 dist；旧工程本地归档在 .codex_tmp/rust-rebuild-backup，不参与构建。
