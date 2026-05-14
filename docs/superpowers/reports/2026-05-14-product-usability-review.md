# VisionSelect 产品可用性评审报告

## 结论摘要

## 目标用户和评审边界

本评审面向两类用户：机器视觉工程师/系统集成商，以及维护型号库并快速生成候选方案的内部研发或应用工程师。

本评审只判断产品可用性和工程工作流完整性，不验证真实硬件性能，不替代算法正确性审查，也不要求把 VisionSelect 改造成新手销售向导。

## 证据来源

- 本仓库文档：`AGENTS.md`、`README.md`。
- 已批准 spec：`docs/superpowers/specs/2026-05-14-product-usability-review-design.zh-CN.md`。
- UI 页面代码：`src/ui/pages/InputPage.cpp`、`PureCalculationPage.cpp`、`CalculationPage.cpp`、`ResultsPage.cpp`、`ComparisonPage.cpp`、`ReportPage.cpp`、`CatalogPage.cpp`。
- 选型相关代码：`src/selection/SelectionEngine.cpp`、`src/selection/CalculationAssistant.cpp`。
- 外部参考：OpenPnP、Caliscope、Kalibr、Harvester、Lensfun、Optiland，以及公开 FOV/镜头计算器。

## 当前工作流拆解

MainWindow 通过 `QStackedWidget` 注册 7 个工作台页面，侧边栏按钮按索引切换，默认进入“需求输入”；页面间还提供“查看结果”“查看方案对比”“返回需求输入”等快捷跳转。证据：`src/ui/MainWindow.cpp:103`、`src/ui/MainWindow.cpp:121`、`src/ui/MainWindow.cpp:195`、`src/ui/MainWindow.cpp:244`。

### 需求输入

- 用户可做：输入工件宽高、装夹余量、最小特征、允许测量误差、工作距离、高度波动、运动速度和帧率，选择检测类型、表面材质，并勾选反光表面、优先黑白相机、允许远心镜头；可点击“计算推荐”或“查看结果”。
- 应用反馈：控件提供默认值和单位；“计算推荐”把当前表单转成选型请求并刷新推荐、对比、报告和型号库视图；“查看结果”直接跳到推荐结果页。
- 支持决策：把工艺需求沉淀为 FOV、目标物方像素、帧率、工作距离和镜头偏好等选型输入，决定后续相机、镜头、光源筛选口径。
- 证据：`src/ui/pages/InputPage.cpp:35`、`src/ui/pages/InputPage.cpp:49`、`src/ui/pages/InputPage.cpp:84`、`src/ui/pages/InputPage.cpp:102`、`src/ui/MainWindow.cpp:103`、`src/ui/MainWindow.cpp:256`。

### 理论计算

- 用户可做：在“纯计算”页手动输入需求、相机参数、镜头参数和光源约束；镜头可在普通镜头和远心镜头间切换；可点击“计算”或“恢复默认”。
- 应用反馈：结果区输出需求估算、相机估算、镜头估算、光源估算和判断，包含 FOV、目标物方像素、最低分辨率、带宽、曝光上限、传感器尺寸、实际 FOV、DOF、畸变、光源覆盖余量、依据和风险；恢复默认会重置参数并立即刷新。
- 支持决策：在不依赖型号库自动推荐的情况下，验证一组手工硬件假设是否满足采样、带宽、镜头和光源覆盖要求。
- 证据：`src/ui/pages/PureCalculationPage.cpp:43`、`src/ui/pages/PureCalculationPage.cpp:83`、`src/ui/pages/PureCalculationPage.cpp:100`、`src/ui/pages/PureCalculationPage.cpp:129`、`src/ui/pages/PureCalculationPage.cpp:156`、`src/ui/pages/PureCalculationPage.cpp:250`、`src/ui/pages/PureCalculationPage.cpp:325`。

### 计算助手

- 用户可做：刷新计算助手估算（需求摘要、相机估算、镜头候选），返回需求输入页，或点击相机估算表中的一行来刷新对应镜头候选。
- 应用反馈：页面显示需求摘要、相机估算表、镜头候选表和详情文本；相机表给出分辨率、像元、传感器、物方像素、普通焦距、PMAG、带宽和判断；镜头表给出类型、厂家、型号、接口、焦距或 PMAG、FOV、物方像素、WD/DOF、像圈和判断；详情说明最低分辨率、12 bit 带宽、镜头类型倾向、运动曝光上限、当前相机和首选镜头的理由与风险。
- 支持决策：帮助工程师先选相机，再看该相机下普通镜头或远心镜头的可行性，判断采样、接口、WD、DOF、像圈和风险是否可接受。
- 证据：`src/ui/pages/CalculationPage.cpp:37`、`src/ui/pages/CalculationPage.cpp:39`、`src/ui/pages/CalculationPage.cpp:55`、`src/ui/pages/CalculationPage.cpp:78`、`src/ui/pages/CalculationPage.cpp:108`、`src/ui/pages/CalculationPage.cpp:152`、`src/ui/MainWindow.cpp:270`、`src/ui/MainWindow.cpp:291`。

### 推荐结果

- 用户可做：查看自动推荐方案列表，点击行查看详细解释，或点击“查看方案对比”进入对比页。
- 应用反馈：摘要显示需求 FOV、目标物方像素和候选方案数量；表格展示方案类型、得分、相机、镜头、光源、FOV、物方像素、倍率或焦距、WD/DOF 和风险；详情展示公式、有效 FOV、接口带宽、单帧数据、存储、曝光上限、DOF、畸变、光源余量、推荐理由和风险提示。
- 支持决策：查看按得分排序的 Top 候选及其理由/风险；该页不是完整候选全集，也不是按风险排序的列表。
- 证据：`src/ui/pages/ResultsPage.cpp:37`、`src/ui/pages/ResultsPage.cpp:42`、`src/ui/pages/ResultsPage.cpp:50`、`src/ui/pages/ResultsPage.cpp:83`、`src/ui/pages/ResultsPage.cpp:124`、`src/ui/MainWindow.cpp:121`、`src/ui/MainWindow.cpp:256`。

### 候选对比

- 用户可做：对比前 5 个推荐方案，点击行查看明细，重新计算，或导出 BOM CSV。
- 应用反馈：对比表展示方案、得分、相机、镜头、光源、FOV、物方像素、曝光上限、带宽/存储、DOF/畸变、光源余量和主要风险；无方案时提示先计算推荐结果；详情展示 BOM、计算结果、接口与存储、镜头/光源余量、推荐理由和风险；BOM 导出成功后弹出完成提示，失败时显示错误。
- 支持决策：让用户横向比较少量候选方案，并把相机、镜头、光源三类物料导出为采购或方案评审用 BOM。
- 证据：`src/ui/pages/ComparisonPage.cpp:37`、`src/ui/pages/ComparisonPage.cpp:39`、`src/ui/pages/ComparisonPage.cpp:50`、`src/ui/pages/ComparisonPage.cpp:77`、`src/ui/pages/ComparisonPage.cpp:121`、`src/ui/MainWindow.cpp:688`。

### 报告预览

- 用户可做：查看 PDF 报告内容预览，导出 PDF，导出 BOM CSV，或重新计算。
- 应用反馈：预览区说明 PDF 将包含需求输入、目标 FOV、目标物方像素、普通镜头与远心镜头关键公式、Top 推荐、方案对比、BOM、带宽、存储、曝光、DOF 和畸变风险；有结果时展示当前首选方案、相机、镜头、光源、得分、带宽/存储和 BOM 项；导出 PDF 成功后显示生成路径，失败时显示错误。
- 支持决策：把当前选型结论整理成可交付报告，支持内部评审、客户沟通和后续采购流转。
- 证据：`src/ui/pages/ReportPage.cpp:19`、`src/ui/pages/ReportPage.cpp:21`、`src/ui/pages/ReportPage.cpp:36`、`src/ui/pages/ReportPage.cpp:41`、`src/ui/MainWindow.cpp:747`。

### 型号库维护

- 用户可做：在相机、镜头、光源三个标签页中搜索、筛选、清空筛选、新增、编辑、删除、导入 CSV、导出 CSV、导出筛选结果和重置内置数据；双击表格行可编辑。
- 应用反馈：筛选器实时刷新表格并保留选中项；相机表展示型号、厂家、分辨率、像元、传感器、靶面、快门、fps、接口和镜头口；镜头表展示型号、厂家、类型、接口、焦距、PMAG、WD、像圈、远心度、畸变、DOF 和同轴；光源表展示型号、厂家、类型、颜色、波长、模式、有效面积和适用场景；删除和重置有确认弹窗，导入失败显示错误，导出成功显示路径。
- 支持决策：维护自动推荐所依赖的硬件边界，决定哪些真实相机、镜头和光源进入选型池，以及当前筛选视图能否用于局部导出。
- 证据：`src/ui/pages/CatalogPage.cpp:131`、`src/ui/pages/CatalogPage.cpp:139`、`src/ui/pages/CatalogPage.cpp:151`、`src/ui/pages/CatalogPage.cpp:179`、`src/ui/pages/CatalogPage.cpp:225`、`src/ui/pages/CatalogPage.cpp:271`、`src/ui/pages/CatalogPage.cpp:366`、`src/ui/MainWindow.cpp:475`、`src/ui/MainWindow.cpp:646`。

## 外部参考对比

| 参考对象 | 类型 | 对 VisionSelect 的启发 | 不应照搬的部分 |
| --- | --- | --- | --- |
| OpenPnP Vision Solutions | 真实机器视觉工作流 | 强调相机、光源、基准点、Z 高度和标定对最终效果的影响。VisionSelect 应明确“预选型”和“现场验证”的边界。 | OpenPnP 是设备控制和贴片工作流，不是选型软件。 |
| Caliscope | 多相机标定工具 | 强调标定质量、误差反馈和可视化。VisionSelect 可借鉴“可信度/误差提示”的产品表达。 | 不需要做完整多相机标定系统。 |
| Kalibr | 专业相机/IMU 标定工具 | 说明相机模型、内参外参和误差建模对专业可信度很重要。 | 当前用户目标不要求 IMU 或复杂标定链路。 |
| Harvester | GenICam 图像采集库 | 说明真实工业相机会涉及曝光、增益、触发、帧率、带宽和设备节点参数。 | VisionSelect 当前不必变成相机采集软件。 |
| Lensfun | 镜头数据库/校正库 | 说明镜头参数数据库和畸变模型需要可维护、可追溯。 | 不需要先实现完整图像校正。 |
| Optiland | 光学设计/仿真平台 | 提供专业光学可信度上限：MTF、PSF、畸变、公差、优化。 | 过于专业，不适合作为短期产品目标。 |
| 公开 FOV/镜头计算器 | 轻量选型计算工具 | 说明 FOV、sensor size、focal length、working distance、pixel density 是基础输入输出。 | 单页计算器缺少综合评分、目录维护和报告能力。 |

没有发现与 VisionSelect 完全同类的开源软件。VisionSelect 的差异化在于把相机、镜头和光源放在同一个预选型工作流里。它不需要追赶标定工具或光学仿真工具的完整能力，但需要补足工程可信度表达、验证边界和型号库质量控制。

## 可用性优势

- `预选型闭环完整`：工程师可以从推荐结果进入方案对比，再到报告预览和 BOM 导出，支持从技术判断到内部评审/采购流转的连续工作。证据：`src/ui/pages/ResultsPage.cpp` 的推荐结果页、`src/ui/pages/ComparisonPage.cpp` 的对比与 BOM 逻辑、`src/ui/pages/ReportPage.cpp` 的报告预览逻辑。
- `推荐理由和风险可追溯`：选型结果不仅给分数，还把采样、FOV、带宽、镜头和光源判断写入 reasons/risks，便于研发用户复核为什么推荐或为什么有风险。证据：`src/selection/SelectionEngine.cpp` 中 `scoreCamera`、`scoreFixedFocalLens`、`scoreTelecentricLens` 与 `scoreLight` 的评分和风险逻辑。
- `关键工程量直接量化`：结果页和对比页显示有效 FOV、物方像素、带宽、存储、曝光上限、DOF、畸变误差和光源余量，减少工程师手工换算。证据：`src/ui/pages/ResultsPage.cpp` 的详情文本，`src/ui/pages/ComparisonPage.cpp` 的对比表和详情文本。
- `镜头适配判断覆盖面较宽`：普通镜头和远心镜头分别检查 FOV、物方像素、像圈、接口、WD、DOF、畸变、MP/MTF 余量和远心残余误差，符合机器视觉预选型的核心复核点。证据：`src/selection/SelectionEngine.cpp` 中 `scoreFixedFocalLens` 和 `scoreTelecentricLens`。
- `光源没有被当作附属字段`：光源评分考虑测量/缺陷/定位场景、反光表面、远心背光、覆盖余量和高速运动频闪需求，帮助用户提前发现照明风险。证据：`src/selection/SelectionEngine.cpp` 中 `chooseLight` 与 `scoreLight`，以及 `src/selection/CalculationAssistant.cpp` 中光源覆盖和频闪风险判断。
- `计算助手支持假设验证`：用户可先看相机估算，再选择相机刷新镜头候选，适合内部研发在没有最终型号前快速验证 WD、焦距、PMAG、带宽和景深假设。证据：`src/selection/CalculationAssistant.cpp` 的相机/镜头估算排序逻辑，`src/ui/pages/CalculationPage.cpp` 的相机表、镜头表和详情文本。
- `对比页面向评审而不是单点计算`：候选对比把 BOM、计算结果、接口存储、镜头/光源余量、推荐理由和风险放在同一详情区，适合工程评审讨论。证据：`src/ui/pages/ComparisonPage.cpp` 中 `refreshDetails` 的详情拼装逻辑。

## 问题清单

- `P0`：阻止目标用户完成基本预选型。
- `P1`：可能导致错误工程判断或显著降低信任。
- `P2`：影响效率、复核或长期维护，但不阻断使用。
- `P3`：细节可用性或文案改进。

### P0 阻塞问题

未发现 P0。现有代码证据显示，需求输入、推荐结果、候选对比和报告预览路径都存在，用户可以完成基本预选型闭环。

### P1 高优先级问题

- **P1-1 单一得分混合硬约束和软偏好。** 问题：`score.score` 同时承载 FOV、采样、接口带宽、像圈、WD、DOF、畸变、光源和偏好加减分，结果页主要显示“得分”和风险文本，但没有“硬约束通过/失败/需实测确认”的独立状态。用户影响：工程师可能把最高分误读为“工程上已满足”，而不是“在当前权重下排序靠前且仍需看风险”。证据：`src/selection/SelectionEngine.cpp` 中 `evaluate` 初始化并累加分数，`scoreCamera`、`scoreFixedFocalLens`、`scoreTelecentricLens`、`scoreLight` 均向同一分数加减；`src/ui/pages/ResultsPage.cpp` 和 `src/ui/pages/ComparisonPage.cpp` 只以“得分”列和风险文本呈现。建议：新增硬约束状态和可信度标签，例如“硬约束满足”“带风险可评估”“需实测确认”“不建议”，并在排序旁解释主要扣分项。
- **P1-2 型号库数据缺失只沉入风险文本。** 问题：缺少接口带宽、镜头 MP、F/# 或 DOF 等数据时，算法会追加风险或扣分，但结果页/报告页没有单独的数据完整性标识。用户影响：内部用户维护型号库时难以快速区分“方案本身有风险”和“catalog 数据不足导致无法判断”，长期会削弱推荐可信度。证据：`src/selection/SelectionEngine.cpp` 对缺少接口带宽、镜头 MP、F/#/DOF 的风险处理；`src/selection/CalculationAssistant.cpp` 也在带宽、MTF、DOF 缺失时追加风险。建议：在结果、对比和型号库中增加数据质量标记，把缺失字段列为报告假设，并允许按“数据不完整”筛选或降低可信度。

### P2 中优先级问题

- **P2-1 候选对比固定只看前 5 个。** 问题：对比页用 `qMin(5, m_results.size())` 限制候选数量，SelectionEngine 也按得分保留 limit 内候选，用户看不到被排除方案的原因。用户影响：评审时只能比较少量 Top 方案，难以解释为什么某个备选品牌、镜头类型或低风险但低分方案没有进入对比。证据：`src/ui/pages/ComparisonPage.cpp` 的 `setResults` 只渲染前 5 个；`src/selection/SelectionEngine.cpp` 的候选保留和排序逻辑按分数截断。建议：提供“显示更多/全部候选”、可配置对比数量和按风险类型筛选，并在摘要中说明当前只展示 Top N。
- **P2-2 报告预览缺少首选方案的理由和风险。** 问题：报告页预览说明 PDF 会包含 Top 推荐和风险，但实际预览只列当前首选、相机、镜头、光源、得分、带宽/存储和 BOM 项，没有展示推荐理由、风险或验证下一步。用户影响：导出前的复核效率低，用户需要回到结果页或对比页确认风险，报告可信度表达也不够强。证据：`src/ui/pages/ReportPage.cpp` 的 `setResults` 文本拼装只取 `results.first()` 的基础信息和 BOM 摘要。建议：在预览和导出报告中同步展示首选方案的 reasons/risks、关键假设和现场验证清单。
- **P2-3 风险信息密集但缺少结构化复核入口。** 问题：结果页和对比页把多条风险用分号连接到一个表格单元或详情段落，缺少按风险类别、严重性或“需现场确认”过滤。用户影响：候选多或风险多时，工程师复核速度会下降，也不利于把风险分派给光学、机构、电控或采购角色。证据：`src/ui/pages/ResultsPage.cpp` 将 `r.score.risks.join("；")` 放入风险列和详情；`src/ui/pages/ComparisonPage.cpp` 通过 `riskSummary(r)` 显示主要风险。建议：把风险拆成标签或分组，例如“采样”“带宽”“镜头”“光源”“数据缺失”“现场验证”，并提供筛选/排序。

### P3 低优先级问题

- **P3-1 “无主要风险”文案容易显得过度确定。** 问题：`riskSummary` 在风险为空时返回“无主要风险”，结果页表格也直接显示该文案；只有详情里补充仍建议结合厂家 MTF/DOF 与现场光源实测确认。用户影响：快速扫表时可能忽略预选型仍需要实测验证的边界。证据：`src/ui/UiHelpers.cpp` 的 `riskSummary`，`src/ui/pages/ResultsPage.cpp` 的风险列和详情文案。建议：改成“未触发主要风险，仍需实测确认”或在表格中显示“需验证”标签。
- **P3-2 分数含义缺少轻量说明。** 问题：页面显示“得分”，但没有解释分数是相对排序、不是合格证书，也没有展示权重来源或主要加减分构成。用户影响：新加入的内部研发或应用工程师需要读详情甚至代码才能理解分数边界。证据：`src/ui/pages/ResultsPage.cpp` 和 `src/ui/pages/ComparisonPage.cpp` 都展示“得分”列；`src/selection/SelectionEngine.cpp` 中分数由多类工程因素累加。建议：在表头 tooltip 或详情首段增加一句“得分用于候选排序，最终判断以硬约束、风险和实测验证为准”。

## 缺失工作流

## 改进建议

### 短期

### 中期

### 后续

## 不建议优先做的能力

## 最终判断
