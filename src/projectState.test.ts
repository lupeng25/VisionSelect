import { expect, it } from "vitest";
import { projectSignature } from "./projectState";
import { newProject, type Project } from "./types";

it("对象键重排不导致已保存方案显示未保存", () => {
  const project = newProject();
  const reordered = Object.fromEntries(
    Object.entries(project).reverse(),
  ) as Project;
  expect(projectSignature(reordered)).toBe(projectSignature(project));
  reordered.parameters = { ...project.parameters, distance: 300 };
  expect(projectSignature(reordered)).not.toBe(projectSignature(project));
});
