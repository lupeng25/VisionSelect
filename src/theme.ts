import { useEffect, useLayoutEffect, useState } from "react";
import { isTauri } from "@tauri-apps/api/core";
import { errorText } from "./api";

export const THEME_KEY = "visionselect.appearance.theme.v1";
export const themes = [
  {
    id: "graphite",
    name: "石墨",
    description: "中灰底色 · 清晰对比",
    scheme: "dark",
  },
  {
    id: "porcelain",
    name: "瓷白",
    description: "清透白色 · 经典蓝调",
    scheme: "light",
  },
  {
    id: "sand",
    name: "暖砂",
    description: "柔和米白 · 赤陶点缀",
    scheme: "light",
  },
  {
    id: "sage",
    name: "雾青",
    description: "浅灰青色 · 沉静松绿",
    scheme: "light",
  },
] as const;
export type ThemeId = (typeof themes)[number]["id"];

function validTheme(value: string | null | undefined): ThemeId {
  return themes.find((theme) => theme.id === value)?.id ?? "graphite";
}

// 在 React 挂载前恢复外观，加载画面也使用已选皮肤。
export function initializeTheme() {
  let theme: ThemeId = "graphite";
  try {
    theme = validTheme(localStorage.getItem(THEME_KEY));
  } catch {
    // 本地存储不可读时仍能启动；用户切换时提示无法保存偏好。
  }
  document.documentElement.dataset.theme = theme;
}

export function useTheme() {
  const [theme, setTheme] = useState<ThemeId>(() =>
    validTheme(document.documentElement.dataset.theme),
  );
  const [storageError, setStorageError] = useState("");
  const [windowError, setWindowError] = useState("");

  useLayoutEffect(() => {
    document.documentElement.dataset.theme = theme;
  }, [theme]);

  useEffect(() => {
    if (!isTauri()) return;
    let current = true;
    const scheme = themes.find((item) => item.id === theme)!.scheme;
    const background = getComputedStyle(document.documentElement)
      .getPropertyValue("--canvas")
      .trim();
    setWindowError("");
    void import("@tauri-apps/api/window")
      .then(async ({ getCurrentWindow }) => {
        if (!current) return;
        const window = getCurrentWindow();
        await Promise.all([
          window.setTheme(scheme),
          window.setBackgroundColor(background),
        ]);
      })
      .catch((error) => {
        if (current) setWindowError("窗口外观同步失败：" + errorText(error));
      });
    return () => {
      current = false;
    };
  }, [theme]);

  function changeTheme(next: ThemeId) {
    setTheme(next);
    try {
      localStorage.setItem(THEME_KEY, next);
      setStorageError("");
    } catch {
      setStorageError("皮肤已切换，但无法保存偏好，下次启动可能需要重新选择。");
    }
  }

  return { theme, changeTheme, storageError, windowError };
}
