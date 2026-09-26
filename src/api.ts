import { invoke, isTauri } from "@tauri-apps/api/core";
import type { Project } from "./types";
export async function request<T>(
  operation: string,
  payload: unknown = {},
): Promise<T> {
  if (isTauri()) return invoke<T>("request", { operation, payload });
  const response = await fetch("/api/" + operation, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });
  const result = await response.json();
  if (!response.ok) throw new Error(result.error || "请求未完成");
  return result as T;
}
export function errorText(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}
export async function exportProject(
  project: Project,
  format: "project" | "report",
): Promise<boolean> {
  const extension = format === "project" ? "vision.json" : "html";
  const safeName = project.name
    .replace(/[<>:"/\\|?*\u0000-\u001f]/g, "_")
    .slice(0, 80);
  if (isTauri()) {
    const { save } = await import("@tauri-apps/plugin-dialog");
    const path = await save({
      defaultPath: safeName + "." + extension,
      filters: [
        {
          name: format === "project" ? "工程方案" : "校核记录",
          extensions: [format === "project" ? "json" : "html"],
        },
      ],
    });
    if (!path) return false;
    await invoke("export_file", { path, project, format });
  } else {
    const { content } = await request<{ content: string }>(
      format === "project" ? "export_project" : "report",
      { project },
    );
    const url = URL.createObjectURL(
      new Blob([content], {
        type:
          format === "project"
            ? "application/json;charset=utf-8"
            : "text/html;charset=utf-8",
      }),
    );
    const link = document.createElement("a");
    link.href = url;
    link.download = safeName + "." + extension;
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }
  return true;
}
