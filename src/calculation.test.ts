import { describe, expect, it } from "vitest";
import {
  defaultCalculation,
  restoreCalculation,
  formatCalculation,
} from "./calculation";

describe("独立计算草稿恢复", () => {
  it("微小非零结果不会显示成零", () => {
    expect(formatCalculation(1e-8, 4)).toBe("1.00e-8");
    expect(formatCalculation(0, 4)).toBe("0");
    expect(formatCalculation(null, 4)).toBe("—");
  });
  it("损坏或未来版本草稿不会影响打开页面", () => {
    expect(restoreCalculation("{")).toEqual(defaultCalculation);
    expect(restoreCalculation('{"version":2,"active":"motion"}')).toEqual(
      defaultCalculation,
    );
  });
  it("保留静止速度、用户空值，丢弃错误类型和未知字段", () => {
    const restored = restoreCalculation(
      JSON.stringify({
        version: 1,
        active: "motion",
        motion: { speed: 0, sampling_um: null, fps: "破损值", injected: 9 },
        optics: { method: "未知模型" },
        data: { pixel_format: "未知格式" },
      }),
    );
    expect(restored.active).toBe("motion");
    expect(restored.motion.speed).toBe(0);
    expect(restored.motion.sampling_um).toBeNull();
    expect(restored.motion.fps).toBe(30);
    expect(restored.motion).not.toHaveProperty("injected");
    expect(restored.optics.method).toBe("fov");
    expect(restored.data.pixel_format).toBe("Mono8");
  });
});
