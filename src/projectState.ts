import type { Project } from "./types";

// Rust 和浏览器的对象键顺序可能不同，不将序列化顺序当作用户修改。
export function projectSignature(project: Project): string {
  return JSON.stringify(project, (_key, value: unknown) => {
    // 旧草稿未包含安装标注字段，与 Rust 补齐的空值视为同一方案。
    if (
      [
        "light_distance",
        "distance_tolerance",
        "light_distance_tolerance",
      ].includes(_key) &&
      value == null
    )
      return undefined;
    if (value !== null && typeof value === "object" && !Array.isArray(value)) {
      return Object.fromEntries(
        Object.entries(value).sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0)),
      );
    }
    return value;
  });
}
