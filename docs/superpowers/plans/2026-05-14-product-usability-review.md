# Product Usability Review Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a product usability review that determines whether VisionSelect is effective as an engineering pre-selection tool for machine-vision engineers, integrators, and internal R&D users.

**Architecture:** This is an evidence-first review, not a code change. The work gathers project context, compares adjacent tools, walks the current product workflow, then writes a ranked usability assessment and recommendation report.

**Tech Stack:** Markdown documentation, Git, GitHub/web research, local Qt Widgets app inspection, optional Qt GUI walkthrough screenshots.

---

## Files And Responsibilities

- Create: `docs/superpowers/reports/2026-05-14-product-usability-review.md`
  - Final review report with evidence, severity-ranked findings, and recommendations.
- Read: `docs/superpowers/specs/2026-05-14-product-usability-review-design.zh-CN.md`
  - Approved Chinese review design and scope.
- Read: `AGENTS.md`
  - Project overview, build/test commands, and working rules.
- Read: `README.md`
  - Toolchain and run instructions.
- Read: `src/ui/pages/InputPage.cpp`
  - Requirement-input workflow.
- Read: `src/ui/pages/PureCalculationPage.cpp`
  - Pure calculation workflow.
- Read: `src/ui/pages/CalculationPage.cpp`
  - Estimation/assistant workflow.
- Read: `src/ui/pages/ResultsPage.cpp`
  - Recommendation result workflow.
- Read: `src/ui/pages/ComparisonPage.cpp`
  - Candidate comparison workflow.
- Read: `src/ui/pages/ReportPage.cpp`
  - Report-preview workflow.
- Read: `src/ui/pages/CatalogPage.cpp`
  - Catalog maintenance workflow.
- Read: `src/selection/SelectionEngine.cpp`
  - Recommendation factors exposed to users.
- Read: `src/selection/CalculationAssistant.cpp`
  - Engineering estimation factors exposed to users.
- Optional output directory: `.deps/product-usability-review/`
  - Screenshots and walkthrough evidence if the app is launched.

## Task 1: Create Review Report Skeleton

**Files:**
- Create: `docs/superpowers/reports/2026-05-14-product-usability-review.md`
- Read: `docs/superpowers/specs/2026-05-14-product-usability-review-design.zh-CN.md`

- [ ] **Step 1: Create the report with fixed sections**

Create `docs/superpowers/reports/2026-05-14-product-usability-review.md` with this exact structure:

```markdown
# VisionSelect 产品可用性评审报告

## 结论摘要

## 目标用户和评审边界

## 证据来源

## 当前工作流拆解

## 外部参考对比

## 可用性优势

## 问题清单

### P0 阻塞问题

### P1 高优先级问题

### P2 中优先级问题

### P3 低优先级问题

## 缺失工作流

## 改进建议

### 短期

### 中期

### 后续

## 不建议优先做的能力

## 最终判断
```

- [ ] **Step 2: Add target-user and boundary text**

Add this text under `目标用户和评审边界`:

```markdown
本评审面向两类用户：机器视觉工程师/系统集成商，以及维护型号库并快速生成候选方案的内部研发或应用工程师。

本评审只判断产品可用性和工程工作流完整性，不验证真实硬件性能，不替代算法正确性审查，也不要求把 VisionSelect 改造成新手销售向导。
```

- [ ] **Step 3: Add initial evidence-source list**

Add this list under `证据来源`:

```markdown
- 本仓库文档：`AGENTS.md`、`README.md`。
- 已批准 spec：`docs/superpowers/specs/2026-05-14-product-usability-review-design.zh-CN.md`。
- UI 页面代码：`src/ui/pages/InputPage.cpp`、`PureCalculationPage.cpp`、`CalculationPage.cpp`、`ResultsPage.cpp`、`ComparisonPage.cpp`、`ReportPage.cpp`、`CatalogPage.cpp`。
- 选型相关代码：`src/selection/SelectionEngine.cpp`、`src/selection/CalculationAssistant.cpp`。
- 外部参考：OpenPnP、Caliscope、Kalibr、Harvester、Lensfun、Optiland，以及公开 FOV/镜头计算器。
```

- [ ] **Step 4: Commit the skeleton**

Run:

```powershell
git add docs/superpowers/reports/2026-05-14-product-usability-review.md
git commit -m "docs: start product usability review report"
```

Expected: one commit that creates the report skeleton.

## Task 2: Map Current VisionSelect Workflow

**Files:**
- Modify: `docs/superpowers/reports/2026-05-14-product-usability-review.md`
- Read: `src/ui/pages/InputPage.cpp`
- Read: `src/ui/pages/PureCalculationPage.cpp`
- Read: `src/ui/pages/CalculationPage.cpp`
- Read: `src/ui/pages/ResultsPage.cpp`
- Read: `src/ui/pages/ComparisonPage.cpp`
- Read: `src/ui/pages/ReportPage.cpp`
- Read: `src/ui/pages/CatalogPage.cpp`
- Read: `src/ui/MainWindow.cpp`

- [ ] **Step 1: Identify navigation pages**

Search:

```powershell
rg -n "addWidget\\(|setActivePage|nav" src/ui/MainWindow.cpp
```

Expected: enough lines to confirm the app navigation order and signal wiring.

- [ ] **Step 2: Inspect each page implementation once**

Read the listed UI files and capture only product behavior visible to the user:

```powershell
Get-Content src/ui/pages/InputPage.cpp
Get-Content src/ui/pages/PureCalculationPage.cpp
Get-Content src/ui/pages/CalculationPage.cpp
Get-Content src/ui/pages/ResultsPage.cpp
Get-Content src/ui/pages/ComparisonPage.cpp
Get-Content src/ui/pages/ReportPage.cpp
Get-Content src/ui/pages/CatalogPage.cpp
```

Expected: notes for input fields, tables, actions, result displays, report controls, and catalog maintenance controls.

- [ ] **Step 3: Write workflow map**

Under `当前工作流拆解`, add these subsections and fill them with concrete observations from the code:

```markdown
### 需求输入

### 理论计算

### 计算助手

### 推荐结果

### 候选对比

### 报告预览

### 型号库维护
```

Each subsection must contain:

- What the user can do.
- What feedback the app gives.
- What engineering decision the page supports.
- What evidence was used, including file paths.

- [ ] **Step 4: Commit the workflow map**

Run:

```powershell
git add docs/superpowers/reports/2026-05-14-product-usability-review.md
git commit -m "docs: map VisionSelect usability workflow"
```

Expected: one commit that adds the current workflow breakdown.

## Task 3: Build External Benchmark Matrix

**Files:**
- Modify: `docs/superpowers/reports/2026-05-14-product-usability-review.md`

- [ ] **Step 1: Capture benchmark categories**

Under `外部参考对比`, add a table with these columns:

```markdown
| 参考对象 | 类型 | 对 VisionSelect 的启发 | 不应照搬的部分 |
| --- | --- | --- | --- |
```

- [ ] **Step 2: Add benchmark rows**

Add these rows:

```markdown
| OpenPnP Vision Solutions | 真实机器视觉工作流 | 强调相机、光源、基准点、Z 高度和标定对最终效果的影响。VisionSelect 应明确“预选型”和“现场验证”的边界。 | OpenPnP 是设备控制和贴片工作流，不是选型软件。 |
| Caliscope | 多相机标定工具 | 强调标定质量、误差反馈和可视化。VisionSelect 可借鉴“可信度/误差提示”的产品表达。 | 不需要做完整多相机标定系统。 |
| Kalibr | 专业相机/IMU 标定工具 | 说明相机模型、内参外参和误差建模对专业可信度很重要。 | 当前用户目标不要求 IMU 或复杂标定链路。 |
| Harvester | GenICam 图像采集库 | 说明真实工业相机会涉及曝光、增益、触发、帧率、带宽和设备节点参数。 | VisionSelect 当前不必变成相机采集软件。 |
| Lensfun | 镜头数据库/校正库 | 说明镜头参数数据库和畸变模型需要可维护、可追溯。 | 不需要先实现完整图像校正。 |
| Optiland | 光学设计/仿真平台 | 提供专业光学可信度上限：MTF、PSF、畸变、公差、优化。 | 过于专业，不适合作为短期产品目标。 |
| 公开 FOV/镜头计算器 | 轻量选型计算工具 | 说明 FOV、sensor size、focal length、working distance、pixel density 是基础输入输出。 | 单页计算器缺少综合评分、目录维护和报告能力。 |
```

- [ ] **Step 3: Write benchmark summary**

Below the table, add this summary and adapt only if evidence contradicts it:

```markdown
没有发现与 VisionSelect 完全同类的开源软件。VisionSelect 的差异化在于把相机、镜头和光源放在同一个预选型工作流里。它不需要追赶标定工具或光学仿真工具的完整能力，但需要补足工程可信度表达、验证边界和型号库质量控制。
```

- [ ] **Step 4: Commit the benchmark matrix**

Run:

```powershell
git add docs/superpowers/reports/2026-05-14-product-usability-review.md
git commit -m "docs: compare VisionSelect with adjacent vision tools"
```

Expected: one commit that adds external benchmark evidence.

## Task 4: Evaluate Usability Strengths And Issues

**Files:**
- Modify: `docs/superpowers/reports/2026-05-14-product-usability-review.md`
- Read: `src/selection/SelectionEngine.cpp`
- Read: `src/selection/CalculationAssistant.cpp`
- Read: `src/ui/pages/ResultsPage.cpp`
- Read: `src/ui/pages/ComparisonPage.cpp`
- Read: `src/ui/pages/ReportPage.cpp`

- [ ] **Step 1: Inspect recommendation and explanation code**

Run:

```powershell
rg -n "risk|reason|score|density|bandwidth|distortion|dof|working|focal|light|exposure" src/selection src/ui/pages
```

Expected: lines that show what recommendation reasons, risks, and engineering factors are exposed.

- [ ] **Step 2: Write usability strengths**

Under `可用性优势`, add at least five concrete strengths. Each item must name the user value and evidence. Use this format:

```markdown
- `优势名称`：说明它如何帮助工程师或内部研发用户。证据：`path/to/file.cpp` 中的具体页面或逻辑。
```

- [ ] **Step 3: Write severity-ranked issues**

Under `问题清单`, use this severity model:

```markdown
- `P0`：阻止目标用户完成基本预选型。
- `P1`：可能导致错误工程判断或显著降低信任。
- `P2`：影响效率、复核或长期维护，但不阻断使用。
- `P3`：细节可用性或文案改进。
```

Add findings under the matching severity headings. Each finding must include:

- Problem.
- User impact.
- Evidence.
- Recommendation.

- [ ] **Step 4: Commit findings**

Run:

```powershell
git add docs/superpowers/reports/2026-05-14-product-usability-review.md
git commit -m "docs: identify product usability findings"
```

Expected: one commit with strengths and ranked issues.

## Task 5: Optional Local UI Walkthrough Evidence

**Files:**
- Modify: `docs/superpowers/reports/2026-05-14-product-usability-review.md`
- Optional create: `.deps/product-usability-review/`

- [ ] **Step 1: Decide whether GUI evidence is needed**

Use GUI walkthrough if the report has claims about layout, navigation, visual density, or text fit. Skip it if the report only evaluates product workflow from code and existing screenshots.

- [ ] **Step 2: Launch the app with Qt PATH**

Run:

```powershell
$env:PATH = "C:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin;$env:PATH"
Start-Process -FilePath "bin\VisionSelect.exe" -WindowStyle Hidden
```

Expected: the app launches.

- [ ] **Step 3: Capture walkthrough notes**

Visit each sidebar page manually or with the `qt-gui-walkthrough` skill:

```text
输入参数
纯计算
计算助手
推荐结果
候选对比
参数库
报告
```

Record whether each page has visible primary action, understandable table/preview content, and no obvious horizontal overflow.

- [ ] **Step 4: Add evidence note**

Under `证据来源`, add:

```markdown
- 本地 UI 走查：访问所有侧边栏页面，记录主要操作、布局密度和明显溢出问题。
```

Under related findings, cite the walkthrough notes.

- [ ] **Step 5: Commit walkthrough evidence**

Run:

```powershell
git add docs/superpowers/reports/2026-05-14-product-usability-review.md .deps/product-usability-review
git commit -m "docs: add product usability walkthrough evidence"
```

Expected: one commit if walkthrough evidence was added. If no screenshots or notes were added, do not create this commit.

## Task 6: Write Recommendations And Final Judgment

**Files:**
- Modify: `docs/superpowers/reports/2026-05-14-product-usability-review.md`

- [ ] **Step 1: Fill missing workflows**

Under `缺失工作流`, include only workflows that matter to the approved audience. Consider these candidates and keep only those supported by evidence:

```markdown
- 工程验证清单：把预选型结果转为现场验证步骤。
- 型号库数据质量检查：标记缺失、过期或可信度不足的 catalog 字段。
- 结果可信度标签：区分硬约束满足、估算满足、需要实测确认。
- 选型假设导出：在报告里列出关键假设和用户输入边界。
```

- [ ] **Step 2: Fill recommendations**

Under `改进建议`, group recommendations:

```markdown
### 短期

- 在结果页和报告中强化“推荐理由/风险/验证下一步”的展示。
- 增加工程验证清单，明确哪些结论需要现场测试或标定确认。
- 为 catalog 维护增加缺失字段和异常值提示。

### 中期

- 增加结果可信度标签，例如“硬约束满足”“估算满足”“需实测确认”。
- 增强报告输出，让工程师能直接复用为预选型说明。
- 增加典型场景模板，减少重复输入。

### 后续

- 评估真实相机参数接入或 GenICam 数据导入。
- 评估镜头数据库质量模型，例如畸变、像圆、工作距离和分辨率等级的可信度来源。
- 评估标定数据与预选型结果的回写关系。
```

- [ ] **Step 3: Fill non-priority capabilities**

Under `不建议优先做的能力`, include:

```markdown
- 完整光学仿真平台。
- 完整相机采集和设备控制软件。
- 面向新手销售的长流程向导。
- 多相机/IMU 标定系统。
```

- [ ] **Step 4: Fill final judgment**

Under `最终判断`, write a clear decision using this shape:

```markdown
VisionSelect 的方向成立，适合作为工程预选型工具继续推进。当前最主要问题不是页面结构，而是工程复核层还不够强：结果需要更明确地区分理论估算、catalog 评分和现场验证。下一阶段应优先补强推荐可信度表达、验证清单和型号库质量提示，而不是扩展到完整采集、标定或光学仿真平台。
```

- [ ] **Step 5: Commit the final report**

Run:

```powershell
git add docs/superpowers/reports/2026-05-14-product-usability-review.md
git commit -m "docs: complete product usability review"
```

Expected: one commit with final recommendations and judgment.

## Task 7: Final Review And Handoff

**Files:**
- Read: `docs/superpowers/reports/2026-05-14-product-usability-review.md`

- [ ] **Step 1: Placeholder scan**

Run:

```powershell
rg -n "未完成|后续填写|此处填写|稍后完善" docs/superpowers/reports/2026-05-14-product-usability-review.md
```

Expected: no matches.

- [ ] **Step 2: Scope check**

Confirm the report does not propose implementation as completed work. It may recommend future work, but it must not claim features were built.

- [ ] **Step 3: Evidence check**

Confirm each P1/P2 issue includes product impact and evidence.

- [ ] **Step 4: Final status**

Run:

```powershell
git status --short
```

Expected: clean worktree.

- [ ] **Step 5: Handoff to user**

Report:

```text
产品可用性评审已完成，报告位于 docs/superpowers/reports/2026-05-14-product-usability-review.md。核心判断：VisionSelect 方向成立，但下一阶段应优先补强工程复核层、验证清单和型号库质量提示。
```

## Self-Review

- Spec coverage: the plan covers target audience, external references, workflow completeness, recommendation credibility, internal tool efficiency, error prevention, verification boundary language, and final report output.
- Placeholder scan: this plan uses no incomplete placeholders.
- Type consistency: this is a documentation/research plan; paths and command names are consistent across tasks.
