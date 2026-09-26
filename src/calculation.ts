import type { Parameters } from "./types";
import { fields, format } from "./format";

export function formatCalculation(value: number | null, digits: number) {
  if (
    value !== null &&
    Number.isFinite(value) &&
    value !== 0 &&
    Math.abs(value) < 10 ** -digits
  )
    return value.toExponential(2);
  return format(value, digits);
}

export function calculationApplyError(patch: CalculationPatch): string | null {
  for (const [key, value] of Object.entries(patch)) {
    const field = Object.values(fields)
      .flat()
      .find((f) => f.key === key);
    if (
      field &&
      (typeof value !== "number" ||
        !Number.isFinite(value) ||
        value < field.min ||
        value > (field.max ?? 1e8))
    )
      return `工作台的${field.title}需在 ${format(field.min, 6)} 到 ${format(field.max ?? 1e8, 0)} ${field.unit} 之间；此结果仍可用于独立计算。`;
  }
  return null;
}

export type CalculationKind = "resolution" | "optics" | "motion" | "data";
export type OpticalMethod =
  "fov" | "focal" | "distance" | "telecentric_fov" | "magnification";
export const calculationKinds: CalculationKind[] = [
  "resolution",
  "optics",
  "motion",
  "data",
];
export const opticalMethods: { value: OpticalMethod; label: string }[] = [
  { value: "fov", label: "已知焦距、工作距离 → 求视场" },
  { value: "focal", label: "已知视场、工作距离 → 求焦距" },
  { value: "distance", label: "已知视场、焦距 → 求工作距离" },
  { value: "telecentric_fov", label: "已知远心倍率 → 求视场" },
  { value: "magnification", label: "已知视场 → 求远心倍率" },
];
export const pixelFormats = [
  "Mono8",
  "Mono10",
  "Mono12",
  "Mono16",
  "Mono10p",
  "Mono12p",
  "BayerRG8",
  "BayerRG10p",
  "BayerRG12p",
  "RGB8",
  "BGR8",
];
type Numeric = number | null;
interface SensorDraft {
  resolution_x: Numeric;
  resolution_y: Numeric;
  pixel_size_um: Numeric;
}
export interface CalculationDraft {
  version: 1;
  active: CalculationKind;
  resolution: SensorDraft & {
    object_width: Numeric;
    object_height: Numeric;
    margin: Numeric;
    pixel_limit: Numeric;
  };
  optics: SensorDraft & {
    method: OpticalMethod;
    focal_mm: Numeric;
    distance_mm: Numeric;
    fov_width: Numeric;
    fov_height: Numeric;
    magnification: Numeric;
  };
  motion: {
    sampling_um: Numeric;
    speed: Numeric;
    exposure: Numeric;
    blur: Numeric;
    fps: Numeric;
  };
  data: {
    resolution_x: Numeric;
    resolution_y: Numeric;
    pixel_format: string;
    fps: Numeric;
    duration_seconds: Numeric;
    bandwidth_mbps: Numeric;
    utilization_percent: Numeric;
  };
}
export type CalculationPatch = Partial<
  Pick<
    Parameters,
    | "width"
    | "height"
    | "margin"
    | "pixel"
    | "distance"
    | "speed"
    | "exposure"
    | "blur"
    | "fps"
    | "pixel_format"
  >
>;
export interface CalculationMetric {
  key: string;
  label: string;
  value: number | null;
  other: number | null;
  unit: string;
  digits: number;
}
export interface CalculationGeometry {
  width: number;
  height: number;
  required_width: number | null;
  required_height: number | null;
}
export interface CalculationResult {
  status: "passed" | "failed" | "unknown" | "neutral";
  summary: string;
  metrics: CalculationMetric[];
  formulas: string[];
  notes: string[];
  geometry: CalculationGeometry | null;
  apply: CalculationPatch;
  apply_description: string;
}
export const CALCULATION_KEY = "visionselect.calculator-2d.v1";
export const defaultCalculation: CalculationDraft = {
  version: 1,
  active: "resolution",
  resolution: {
    object_width: 20,
    object_height: 15,
    margin: 2,
    pixel_limit: 15,
    resolution_x: 2448,
    resolution_y: 2048,
    pixel_size_um: 3.45,
  },
  optics: {
    method: "fov",
    resolution_x: 2448,
    resolution_y: 2048,
    pixel_size_um: 3.45,
    focal_mm: 25,
    distance_mm: 300,
    fov_width: 100,
    fov_height: 80,
    magnification: 0.3,
  },
  motion: { sampling_um: 10, speed: 100, exposure: 50, blur: 1, fps: 30 },
  data: {
    resolution_x: 2448,
    resolution_y: 2048,
    pixel_format: "Mono8",
    fps: 30,
    duration_seconds: 60,
    bandwidth_mbps: 125,
    utilization_percent: 80,
  },
};

// 仅恢复既有字段；无效草稿不会阻止主程序启动。
export function restoreCalculation(text: string | null): CalculationDraft {
  const result = structuredClone(defaultCalculation);
  if (!text) return result;
  try {
    const parsed = JSON.parse(text);
    if (parsed?.version !== 1) return result;
    if (calculationKinds.includes(parsed.active)) result.active = parsed.active;
    for (const kind of calculationKinds) {
      const section = result[kind] as unknown as Record<
        string,
        Numeric | string
      >;
      for (const key of Object.keys(section)) {
        const value: unknown = parsed[kind]?.[key];
        if (
          typeof section[key] === "number" &&
          (value === null ||
            (typeof value === "number" && Number.isFinite(value)))
        )
          section[key] = value;
      }
    }
    if (opticalMethods.some((m) => m.value === parsed.optics?.method))
      result.optics.method = parsed.optics.method;
    if (pixelFormats.includes(parsed.data?.pixel_format))
      result.data.pixel_format = parsed.data.pixel_format;
  } catch {
    /* 保留可编辑的默认参数。 */
  }
  return result;
}
