import { defineConfig } from "@playwright/test";
export default defineConfig({
  testDir: "./tests/e2e",
  fullyParallel: false,
  workers: 1,
  timeout: 30000,
  expect: { timeout: 8000 },
  use: {
    baseURL: "http://127.0.0.1:1420",
    viewport: { width: 1520, height: 960 },
    trace: "retain-on-failure",
    screenshot: "only-on-failure",
  },
  reporter: [["list"]],
  webServer: [
    {
      command: "cargo run -p vision-server",
      url: "http://127.0.0.1:4318/api/health",
      reuseExistingServer: false,
      timeout: 120000,
      env: { VISIONSELECT_DATA_DIR: ".codex_tmp/e2e-data" },
    },
    {
      command: "npm run dev",
      url: "http://127.0.0.1:1420",
      reuseExistingServer: false,
    },
  ],
});
