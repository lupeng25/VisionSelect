import { expect, it } from "vitest";
import {
  installationDimension,
  lightingLayout,
  positiveDimension,
} from "./installation";
import { inputError } from "./format";
import { newProject, type Hardware } from "./types";
import { projectSignature } from "./projectState";

it("安装尺寸保留有效精度，微小公差不会显示为零", () => {
  expect(installationDimension(110, 0.001)).toBe("110 ± 0.001 mm");
  expect(installationDimension(60.12345, 0.0001)).toBe("60.12345 ± 0.0001 mm");
  expect(installationDimension(123456.789, 1e-8)).toBe(
    "1.23456789e5 ± 1e-8 mm",
  );
  expect(installationDimension(110, 0)).toBe("110 ± 0 mm");
  expect(installationDimension(110, null)).toBe("110 mm");
  expect(installationDimension(null, 0.001)).toBe("待确认");
});

it("识别目录照明类型，未知类型不推断成有效布置", () => {
  for (const [type, expected] of [
    ["TelecentricBacklight", "backlight"],
    ["Coaxial", "coaxial"],
    ["环形光源", "ring"],
    ["Bar", "side"],
    ["Dome", "dome"],
    ["custom", "unknown"],
  ]) {
    expect(
      lightingLayout({
        id: type,
        kind: "light",
        model: type,
        manufacturer: "测试",
        origin: "测试",
        specs: { light_type: type },
      } satisfies Hardware),
    ).toBe(expected);
  }
  expect(lightingLayout(null)).toBe("unknown");
});
it("旧草稿与空标注等价，零公差有效，非法安装参数被拒绝", () => {
  const project = newProject();
  const legacy = structuredClone(project);
  delete legacy.parameters.light_distance;
  delete legacy.parameters.distance_tolerance;
  delete legacy.parameters.light_distance_tolerance;
  expect(inputError(legacy.parameters)).toBeNull();
  expect(projectSignature(project)).toBe(projectSignature(legacy));
  project.parameters.distance_tolerance = 0;
  expect(inputError(project.parameters)).toBeNull();
  expect(projectSignature(project)).not.toBe(projectSignature(legacy));
  for (const value of [-1, NaN, Infinity, 1e9]) {
    project.parameters.light_distance = value;
    expect(inputError(project.parameters)).toContain("光源工作距离");
  }
  for (const value of [undefined, null, NaN, Infinity, -1, 0])
    expect(positiveDimension(value)).toBeNull();
});
