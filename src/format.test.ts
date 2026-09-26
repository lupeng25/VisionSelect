import { describe, expect, it } from "vitest";
import { format, inputError, specNumber } from "./format";
import { newProject, type Hardware } from "./types";

describe("参数输入与缺失数据边界", () => {
  it("缺失和非有限数字不显示成零", () => {
    for (const value of [null, undefined, NaN, Infinity])
      expect(format(value)).toBe("—");
    expect(format(0)).toBe("0");
  });
  it("可选距离允许留空，必填参数清空后阻止提交", () => {
    const p = newProject().parameters;
    p.scan_distance = null;
    expect(inputError(p)).toBeNull();
    p.width = NaN;
    expect(inputError(p)).toContain("工件宽度");
  });
  it("编码器脉冲必须为整数，采样裕量系数不能超过一", () => {
    const p = newProject().parameters;
    p.pulses = 2.5;
    expect(inputError(p)).toContain("整数");
    p.pulses = 10000;
    p.safety = 1.01;
    expect(inputError(p)).toContain("采样裕量系数");
  });
  it("新方案之间不共享可变参数和设备列表", () => {
    const a = newProject();
    const b = newProject();
    a.parameters.width = 999;
    a.hardware.push(null);
    expect(b.parameters.width).toBe(20);
    expect(b.hardware).toHaveLength(4);
    expect(a.id).not.toBe(b.id);
  });
  it("目录空字符串与零规格保持未知", () => {
    const item = {
      specs: { empty: "", zero: "0", valid: "3.45", invalid: "未公开" },
    } as unknown as Hardware;
    expect(specNumber(item, "empty")).toBeNull();
    expect(specNumber(item, "zero")).toBeNull();
    expect(specNumber(item, "invalid")).toBeNull();
    expect(specNumber(item, "valid")).toBe(3.45);
  });
});
