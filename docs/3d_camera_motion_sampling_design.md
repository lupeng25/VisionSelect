# 3D 相机参数设定与运动采样计算功能设计

## 设计来源

参考文件：`C:/Users/73200/Desktop/Data/3DData/相机参数设定.xlsx`

该 Excel 以基恩士 3D 相机参数设定为例，核心不是产品目录浏览，而是围绕线扫/轮廓式 3D 相机的运动采样关系进行计算。表格中的绿色单元格是可修改输入，红色单元格是计算结果，可直接转化为应用内的参数设定计算器。

## 功能定位

新增功能建议命名为：**3D 参数设定**

它应归属于现有 3D 相机模块，作为当前“3D 产品筛选/查看”的扩展，不影响现有 2D 相机 + 镜头 + 光源选型流程。

目标是让用户在选择某款 3D 相机后，进一步计算：

- 固定扫描距离下需要采集多少条轮廓/帧。
- 编码器脉冲对应的轴向采样间隔。
- Y 向像素当量，即运动方向点距。
- X 向像素当量，即相机公开的 X 轮廓数据间隔。
- 指定采样周期和安全系数下允许的轴运动速度。
- 当前相机规格是否满足用户输入的速度、点距和距离要求。

## Excel 公式映射

| Excel 位置 | 表格含义 | 应用内字段 | 公式 |
| --- | --- | --- | --- |
| B2 | 采集帧数/轮廓数 | `profileCount` | `scanDistanceMm / profileIntervalMm` |
| B5 | 脉冲采样间隔 | `pulseIntervalMm` | `axisTravelMm / pulseCount` |
| B8 | Y 像素当量 | `yPixelPitchMm` | `pulseIntervalMm * refinementPoints` |
| B9 | X 像素当量 | `xPixelPitchMm` | 优先来自相机 `profileDataIntervalUm / 1000` |
| B12 | 轴运动速度 | `maxAxisSpeedMmS` | `samplingRateHz * profileIntervalMm * safetyFactor` |

说明：

- Excel 中 `0.005 mm` 的 X 像素当量示例，对应 `5 um` 的 X 轮廓数据间隔。
- Excel 中 `取 80%` 应设计为可配置安全系数，默认 `0.8`。
- Excel 中“采样周期（Hz/S）”建议在应用里命名为“采样频率/轮廓频率（Hz）”，避免单位歧义。

## 用户流程

1. 用户进入 3D 相机页面。
2. 用户通过现有筛选器筛出候选 3D 相机。
3. 用户选中某个相机型号。
4. 参数设定区域自动带入相机公开参数：
   - X 轮廓数据间隔。
   - 最大扫描频率或帧率。
   - 工作距离或参考距离。
   - X/Y 覆盖范围。
   - Z 量程。
5. 用户输入现场工况：
   - 扫描距离。
   - 期望轮廓采样间隔。
   - 轴移动距离。
   - 编码器脉冲数量。
   - 细化点数。
   - 采样频率。
   - 安全系数。
6. 系统输出计算结果和风险提示。
7. 后续可将结果加入 3D 选型报告或 BOM。

## 页面设计

建议在现有 3D 页面中增加一个页签或分区：

- `产品筛选`
- `参数设定`

参数设定页面分为四块：

### 1. 当前相机摘要

展示只读字段：

- 品牌 / 系列 / 型号。
- 技术路线。
- X 轮廓数据间隔。
- 最大扫描频率或帧率。
- 工作距离范围或参考距离。
- X/Y 覆盖范围。
- Z 量程。

若相机缺少 X 轮廓数据间隔或最大扫描频率，则提示“官方数据缺失，需要人工确认”，并允许用户手动覆盖。

### 2. 现场输入

| 字段 | 默认值 | 单位 | 校验 |
| --- | --- | --- | --- |
| 扫描距离 | 300 | mm | 大于 0 |
| 轮廓采样间隔 | 0.05 | mm/轮廓 | 大于 0 |
| 轴移动距离 | 10 | mm | 大于 0 |
| 脉冲数量 | 10000 | pulse | 大于 0 |
| 细化点数 | 50 | 点 | 大于 0 |
| 采样频率 | 1000 | Hz | 大于 0，不应超过相机公开上限 |
| 安全系数 | 0.8 | - | 建议 0.1 到 1.0 |

### 3. 计算结果

| 结果 | 单位 | 示例 |
| --- | --- | --- |
| 采集轮廓数 | 条 | 6000 |
| 脉冲采样间隔 | mm/pulse | 0.001 |
| Y 像素当量 | mm | 0.05 |
| X 像素当量 | mm | 0.005 |
| 允许轴速度 | mm/s | 40 |
| X/Y 点距比例 | - | 10:1 |

### 4. 状态与风险

输出示例：

- 满足：采样频率未超过相机公开最大扫描频率。
- 需确认：该型号未公开 X 轮廓数据间隔。
- 风险：Y 像素当量明显大于 X 像素当量，缺陷检测可能出现运动方向采样不足。
- 风险：扫描距离下需要采集的轮廓数较多，需要确认控制器缓存和传输能力。

## 数据模型建议

新增计算输入结构：

```cpp
struct ThreeDMotionSamplingInput {
    double scanDistanceMm = 300.0;
    double profileIntervalMm = 0.05;
    double axisTravelMm = 10.0;
    int pulseCount = 10000;
    int refinementPoints = 50;
    double samplingRateHz = 1000.0;
    double safetyFactor = 0.8;
    double overrideXPixelPitchMm = -1.0;
};
```

新增计算结果结构：

```cpp
struct ThreeDMotionSamplingResult {
    double profileCount = 0.0;
    double pulseIntervalMm = 0.0;
    double yPixelPitchMm = 0.0;
    double xPixelPitchMm = -1.0;
    double maxAxisSpeedMmS = 0.0;
    double xyPitchRatio = -1.0;
    bool samplingRateKnown = false;
    bool samplingRateWithinCameraLimit = true;
    QStringList reasons;
    QStringList risks;
};
```

建议新增文件：

- `src/three_d/ThreeDCalculation.h`
- `src/three_d/ThreeDCalculation.cpp`

这样可以把计算逻辑从 UI 中隔离出来，便于写 QtTest。

## 与现有 3D 数据字段的映射

| 当前字段 | 用途 |
| --- | --- |
| `ThreeDCameraSpec::profileDataIntervalUm` | 优先作为 X 像素当量来源 |
| `ThreeDCameraSpec::xyResolutionUm` | 当 X 轮廓数据间隔缺失时作为备选提示 |
| `ThreeDCameraSpec::scanRateMaxHz` | 采样频率上限校验 |
| `ThreeDCameraSpec::frameRateHz` | 面阵/快照式 3D 的速度参考 |
| `ThreeDCameraSpec::workingDistanceMinMm/MaxMm` | 工作距离范围校验 |
| `ThreeDCameraSpec::referenceDistanceMm` | 无范围数据时的参考距离提示 |
| `ThreeDCameraSpec::xFovNearMm/xFovReferenceMm/xFovFarMm` | X 覆盖校验 |
| `ThreeDCameraSpec::zMeasurementRangeMm` | Z 量程校验 |

## 对现有选型能力的扩展

当前 `ThreeDCameraMatcher` 已支持需求过滤和“不满足原因”。建议在后续阶段追加运动采样条件：

```cpp
struct ThreeDCameraRequirement {
    ...
    double targetYPixelPitchMm = -1.0;
    double requiredAxisSpeedMmS = -1.0;
    double scanDistanceMm = -1.0;
};
```

匹配器可以新增判断：

- 若用户要求 `requiredAxisSpeedMmS`，根据相机最大扫描频率和目标轮廓间隔估算可支持速度。
- 若用户要求 `targetYPixelPitchMm`，检查输入的轴编码器参数是否能达到。
- 若相机缺少最大扫描频率或 X 轮廓数据间隔，则进入“需确认”状态，而不是直接淘汰。

## 分阶段实现建议

### 第一阶段：独立计算器

- 新增 `ThreeDCalculation` 计算模块。
- 在 3D 页面添加“参数设定”区域。
- 用户选中相机后自动带入 X 轮廓数据间隔和最大扫描频率。
- 输出 Excel 对应的 5 个核心结果。
- 新增 QtTest 覆盖 Excel 示例值：
  - `300 / 0.05 = 6000`
  - `10 / 10000 = 0.001`
  - `0.001 * 50 = 0.05`
  - `1000 * 0.05 * 0.8 = 40`

### 第二阶段：匹配器联动

- 将运动速度、Y 像素当量、采样频率加入 3D 匹配条件。
- 结果表增加“运动采样状态”列。
- 详情区展示计算依据和风险。

### 第三阶段：报告与导出

- 3D 参数设定结果进入 PDF 报告。
- 支持导出 3D 参数设定 CSV。
- 支持将选中的 3D 相机加入 BOM。

## 测试点

新增测试建议放在 `tests/test_selection.cpp`：

- Excel 示例公式回归测试。
- 输入 0 或负数时不崩溃，并给出风险/错误提示。
- 相机有 `profileDataIntervalUm` 时自动计算 X 像素当量。
- 相机缺少 `profileDataIntervalUm` 时允许手动覆盖。
- 采样频率超过相机 `scanRateMaxHz` 时输出风险。
- 当前 3D 参数计算不改变 2D 选型结果。

## 验收标准

- 用户不打开 Excel，也能完成同样的 3D 参数设定计算。
- Excel 中的示例值在应用里得到一致结果。
- 选择不同 3D 相机时，X 像素当量和速度上限能随产品数据变化。
- 数据缺失时明确提示“需确认”，不误判为满足。
- 该功能不影响 2D 相机、镜头、光源的现有推荐结果。

## 设计结论

这份 Excel 最适合作为 3D 模块的“运动采样计算器”需求来源。建议先做独立参数设定页，再逐步接入 3D 产品匹配、报告和 BOM。这样既能保留当前 3D 产品库的查看能力，又能让 3D 相机模块具备接近实际项目选型的计算价值。
