import type { Hardware } from "./types";
import { spec } from "./format";

// 尺寸标注保留已存数值的有效精度，不能把非零安装公差舍入为零。
export function installationDimension(
  value: number | null,
  tolerance?: number | null,
): string {
  if (value == null || positiveDimension(value) == null) return "待确认";
  const exact = (v: number) =>
    (v >= 1e5 ? v.toExponential() : String(v)).replace("e+", "e");
  const suffix =
    tolerance != null && Number.isFinite(tolerance) && tolerance >= 0
      ? ` ± ${exact(tolerance)}`
      : "";
  return `${exact(value)}${suffix} mm`;
}

export type LightingLayout =
  "ring" | "side" | "coaxial" | "backlight" | "dome" | "unknown";
export const lightingLabels: Record<LightingLayout, string> = {
  ring: "环形照明",
  side: "侧向照明",
  coaxial: "同轴照明",
  backlight: "背光照明",
  dome: "穹顶照明",
  unknown: "布置待确认",
};
export function lightingLayout(light: Hardware | null): LightingLayout {
  const value = spec(light, "light_type").trim().toLowerCase();
  if (/backlight|背光/.test(value)) return "backlight";
  if (/coaxial|同轴/.test(value)) return "coaxial";
  if (/dome|穹顶|球积分/.test(value)) return "dome";
  if (/ring|环形/.test(value)) return "ring";
  if (/bar|条形|条状/.test(value)) return "side";
  return "unknown";
}
export function positiveDimension(
  value: number | null | undefined,
): number | null {
  return value != null && Number.isFinite(value) && value > 0 ? value : null;
}
