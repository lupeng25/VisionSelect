import type { Hardware, Parameters } from "./types";
export const format = (n: number | null | undefined, digits = 2) =>
  n == null || !Number.isFinite(n)
    ? "—"
    : new Intl.NumberFormat("zh-CN", { maximumFractionDigits: digits }).format(
        n,
      );
export const spec = (h: Hardware | null | undefined, key: string) => {
  const value = h?.specs[key];
  return value == null || value === ""
    ? ""
    : Array.isArray(value)
      ? value.join(" / ")
      : String(value);
};
export const specNumber = (h: Hardware | null | undefined, key: string) => {
  const value = Number(h?.specs[key]);
  return Number.isFinite(value) && value > 0 ? value : null;
};
export function summary(h: Hardware): string {
  if (h.kind === "camera")
    return (
      spec(h, "resolution_x") +
      " × " +
      spec(h, "resolution_y") +
      " px · " +
      spec(h, "pixel_size_um") +
      " μm"
    );
  if (h.kind === "lens")
    return spec(h, "lens_type").toLowerCase().includes("telecentric")
      ? "远心 " + spec(h, "pmag") + "× · WD " + spec(h, "nominal_wd_mm") + " mm"
      : spec(h, "focal_length_mm") +
          " mm · 像圈 " +
          spec(h, "image_circle_mm") +
          " mm";
  if (h.kind === "light")
    return (
      spec(h, "color") +
      " · " +
      spec(h, "active_width_mm") +
      " × " +
      spec(h, "active_height_mm") +
      " mm"
    );
  return spec(h, "technology") || "技术路线待确认";
}
export interface Field {
  key: keyof Parameters;
  title: string;
  unit: string;
  min: number;
  step: number;
  max?: number;
}
export const fields: Record<string, Field[]> = {
  target: [
    { key: "width", title: "工件宽度", unit: "mm", min: 0.01, step: 1 },
    { key: "height", title: "工件高度", unit: "mm", min: 0.01, step: 1 },
    { key: "margin", title: "单边装夹余量", unit: "mm", min: 0, step: 0.5 },
    { key: "pixel", title: "像素当量上限", unit: "μm/px", min: 0.001, step: 1 },
    { key: "distance", title: "镜头工作距离", unit: "mm", min: 0.01, step: 10 },
    {
      key: "variation",
      title: "高度变化（峰峰值）",
      unit: "mm",
      min: 0,
      step: 0.5,
    },
  ],
  acquisition: [
    { key: "fps", title: "目标帧率", unit: "fps", min: 0.01, step: 1 },
    { key: "exposure", title: "曝光时间", unit: "μs", min: 0.01, step: 10 },
    { key: "speed", title: "运动速度", unit: "mm/s", min: 0, step: 10 },
    { key: "blur", title: "允许拖影", unit: "px", min: 0.01, step: 0.1 },
  ],
  measured: [
    {
      key: "measured_width",
      title: "实测视场宽度",
      unit: "mm",
      min: 0.01,
      step: 1,
    },
    {
      key: "measured_height",
      title: "实测视场高度",
      unit: "mm",
      min: 0.01,
      step: 1,
    },
  ],
  scanning: [
    { key: "scan_width", title: "所需 X 覆盖", unit: "mm", min: 0.01, step: 1 },
    { key: "z_range", title: "所需 Z 量程", unit: "mm", min: 0.01, step: 1 },
    {
      key: "z_repeat",
      title: "Z 重复精度上限",
      unit: "μm",
      min: 0.001,
      step: 0.1,
    },
    {
      key: "scan_distance",
      title: "实际工作距离（可留空）",
      unit: "mm",
      min: 0.01,
      step: 1,
    },
    {
      key: "scan_length",
      title: "运动扫描长度",
      unit: "mm",
      min: 0.01,
      step: 10,
    },
    {
      key: "interval",
      title: "目标轮廓间距上限",
      unit: "mm",
      min: 0.001,
      step: 0.01,
    },
    {
      key: "scan_speed",
      title: "目标轴速度",
      unit: "mm/s",
      min: 0.01,
      step: 10,
    },
    {
      key: "safety",
      title: "采样裕量系数",
      unit: "",
      min: 0.01,
      max: 1,
      step: 0.05,
    },
  ],
  trigger: [
    { key: "rate", title: "轮廓采样频率", unit: "Hz", min: 0.01, step: 100 },
    {
      key: "scan_exposure",
      title: "曝光时间",
      unit: "μs",
      min: 0.01,
      step: 10,
    },
    { key: "readout", title: "读出与复位余量", unit: "μs", min: 0, step: 1 },
  ],
  encoder: [
    { key: "travel", title: "标定移动距离", unit: "mm", min: 0.001, step: 1 },
    { key: "pulses", title: "标定脉冲数量", unit: "pulse", min: 1, step: 1 },
    {
      key: "per_profile",
      title: "每轮廓脉冲数",
      unit: "pulse",
      min: 1,
      step: 1,
    },
    {
      key: "pulse_rate",
      title: "编码器脉冲频率",
      unit: "Hz",
      min: 0.01,
      step: 100,
    },
  ],
};
export function inputError(p: Parameters): string | null {
  for (const f of Object.values(fields).flat()) {
    const value = p[f.key];
    if (f.key === "scan_distance" && value === null) continue;
    if (
      typeof value !== "number" ||
      !Number.isFinite(value) ||
      value < f.min ||
      value > (f.max ?? 1e8)
    )
      return "请填写有效的" + f.title + "。";
    if (["pulses", "per_profile"].includes(f.key) && !Number.isInteger(value))
      return f.title + "必须为整数。";
  }
  return null;
}
export const specificationLabels: Record<string, string> = {
  resolution_x: "水平像素 / px",
  resolution_y: "垂直像素 / px",
  pixel_size_um: "像元尺寸 / μm",
  sensor_format: "传感器规格",
  color_mode: "色彩模式",
  pixel_format: "传输像素格式",
  shutter_type: "快门类型",
  max_fps: "标称帧率 / fps",
  interface: "数据接口",
  bandwidth_mbps: "目录带宽 / MB/s",
  bandwidth_source: "带宽来源",
  bit_depth: "位深 / bit",
  dynamic_range_db: "动态范围 / dB",
  lens_mount: "镜头接口",
  lens_type: "镜头类型",
  focal_length_mm: "焦距 / mm",
  min_wd_mm: "最小工作距离 / mm",
  image_circle_mm: "像圈 / mm",
  pmag: "远心倍率",
  nominal_wd_mm: "标称工作距离 / mm",
  wd_tolerance_mm: "距离容差 / mm",
  dof_mm: "目录景深 / mm",
  dof_conditions_confirmed: "景深适用条件已确认",
  f_number: "光圈 F 值",
  megapixel_rating: "标称分辨率 / MP",
  distortion_percent: "畸变 / %",
  telecentricity_deg: "远心度 / °",
  light_type: "光源类型",
  color: "颜色",
  wavelength_nm: "波长 / nm",
  mode: "照明模式",
  active_width_mm: "发光面宽 / mm",
  active_height_mm: "发光面高 / mm",
  best_for: "适用场景",
  technology: "技术路线",
  series: "系列",
  referenceDistanceMm: "参考距离 / mm",
  workingDistanceMinMm: "最近工作距离 / mm",
  workingDistanceMaxMm: "最远工作距离 / mm",
  xFovReferenceMm: "参考截面 X 视场 / mm",
  xFovNearMm: "近端 X 视场 / mm",
  xFovFarMm: "远端 X 视场 / mm",
  yFovReferenceMm: "参考截面 Y 视场 / mm",
  zMeasurementRangeMm: "Z 量程 / mm",
  zRepeatabilityUm: "Z 重复精度 / μm",
  measurementAccuracyUm: "测量精度 / μm",
  profileDataIntervalUm: "轮廓数据间隔 / μm",
  profilePoints: "轮廓点数",
  scanRateMaxHz: "最高轮廓频率 / Hz",
  frameRateHz: "快照帧率 / Hz",
  encoderRateMaxHz: "编码器频率上限 / Hz",
  requiresExternalMotion: "需要外部运动",
  supportsEncoder: "支持编码器",
  supportsExternalTrigger: "支持外部触发",
  exposureTimeMinUs: "最短曝光 / μs",
  exposureTimeMaxUs: "最长曝光 / μs",
  interfaces: "接口",
  accuracyCondition: "精度适用条件",
  geometryCorrectionNote: "几何规格校正说明",
  geometryCorrectionSource: "几何规格校正依据",
  sourceUrl: "来源链接",
  sourceDate: "来源日期",
  source_url: "来源链接",
  source_date: "来源日期",
  notes: "备注",
  ipRating: "防护等级",
  lightSource: "光源",
  materialScenarios: "材质场景",
};
