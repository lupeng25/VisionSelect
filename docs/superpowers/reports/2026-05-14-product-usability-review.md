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
