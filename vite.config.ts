import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  plugins: [react()],
  server: {
    proxy: { "/api": "http://127.0.0.1:4318" },
    // Windows 链接程序会短暂独占可执行文件，前端无需监听 Rust 与本地归档。
    watch: {
      ignored: [
        "**/target/**",
        "**/src-tauri/**",
        "**/crates/**",
        "**/resources/**",
        "**/.codex_tmp/**",
        "**/build/**",
        "**/.deps/**",
      ],
    },
  },
  clearScreen: false,
});
