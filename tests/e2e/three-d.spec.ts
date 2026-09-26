import {
  test,
  expect,
  type APIRequestContext,
  type Page,
} from "@playwright/test";
import { readFile } from "node:fs/promises";
import type { Bootstrap, Project } from "../../src/types";

async function api(
  request: APIRequestContext,
  operation: string,
  payload = {},
) {
  const response = await request.post(
    `http://127.0.0.1:4318/api/${operation}`,
    { data: payload },
  );
  expect(response.ok(), await response.text()).toBeTruthy();
  return response.json();
}
async function ready(page: Page) {
  page.on("dialog", (dialog) => dialog.accept());
  await page.goto("/");
  await expect(page.getByRole("heading", { name: "方案工作台" })).toBeVisible();
}
async function chooseScanner(page: Page) {
  await page.getByRole("button", { name: "三维采样", exact: true }).click();
  await page.getByRole("button", { name: "选择3D 相机", exact: true }).click();
  const dialog = page.getByRole("dialog");
  await dialog.getByRole("textbox", { name: "搜索硬件" }).fill("Gocator 2320");
  await dialog
    .getByRole("button", { name: "Gocator 2320 LMI", exact: true })
    .click();
  return dialog;
}
async function encoded(page: Page) {
  await page.getByRole("button", { name: "触发与曝光", exact: true }).click();
  await page.getByLabel("触发方式").selectOption("encoder");
  await page
    .getByRole("spinbutton", { name: "标定移动距离", exact: true })
    .fill("100");
  await page
    .getByRole("spinbutton", { name: "标定脉冲数量", exact: true })
    .fill("4000");
  await page
    .getByRole("spinbutton", { name: "每轮廓脉冲数", exact: true })
    .fill("1");
  await page
    .getByRole("spinbutton", { name: "编码器脉冲频率", exact: true })
    .fill("1600");
}

test("原厂截面修正进入资料库、工作台和窄窗口指标", async ({ page }) => {
  await ready(page);
  const dialog = await chooseScanner(page);
  const specs = dialog.getByLabel("设备详细规格");
  await expect(
    specs.locator("div").filter({ hasText: "最近工作距离 / mm" }),
  ).toContainText("40");
  await expect(
    specs.locator("div").filter({ hasText: "最远工作距离 / mm" }),
  ).toContainText("65");
  await expect(specs).not.toContainText("参考截面 X 视场");
  await expect(specs).toContainText("几何规格校正说明");
  await dialog.getByRole("button", { name: "加入当前方案" }).click();
  await page
    .getByRole("spinbutton", { name: "实际工作距离（可留空）" })
    .fill("40");
  await page
    .getByRole("spinbutton", { name: "所需 X 覆盖", exact: true })
    .fill("20");
  await expect(page.getByTestId("check-scan_fov")).toHaveClass(/failed/);
  await expect(
    page.getByTestId("check-scan_fov").locator(".check-values"),
  ).toContainText("当前 18");
  await expect(page.getByTestId("metric-profiles")).toHaveText("7,500");
  await expect(page.getByTestId("metric-required_profiles")).toHaveText(
    "6,000",
  );
  await expect(page.getByTestId("metric-actual_interval")).toHaveText("0.04");
  await expect(page.locator(".metric-row .metric")).toHaveCount(6);
  await page.screenshot({
    path: ".codex_tmp/three-d-fix/三维工作台-近端覆盖.png",
    fullPage: true,
  });
  await page.setViewportSize({ width: 1024, height: 768 });
  expect(
    await page
      .locator(".metric-row")
      .evaluate((element) => element.scrollWidth <= element.clientWidth),
  ).toBeTruthy();
  for (const card of await page.locator(".metric-row .metric").all()) {
    expect(
      await card.evaluate(
        (element) => element.scrollWidth <= element.clientWidth,
      ),
    ).toBeTruthy();
  }
  await page.screenshot({
    path: ".codex_tmp/three-d-fix/三维工作台-窄窗口.png",
    fullPage: true,
  });
  const spacing = page.getByRole("spinbutton", {
    name: "目标轮廓间距上限",
    exact: true,
  });
  await spacing.scrollIntoViewIfNeeded();
  await expect(spacing).toBeInViewport();
  await spacing.fill("0.06");
  await expect(page.getByTestId("metric-required_profiles")).toHaveText(
    "5,000",
  );
  await expect(page.getByTestId("metric-profiles")).toHaveText("7,500");
  await page.screenshot({
    path: ".codex_tmp/three-d-fix/三维工作台-窄窗口参数.png",
    fullPage: true,
  });
});

test("实际采样数量、建议裕量与缓存风险实时联动", async ({ page }) => {
  await ready(page);
  const dialog = await chooseScanner(page);
  await dialog.getByRole("button", { name: "加入当前方案" }).click();
  await encoded(page);
  await expect(page.getByTestId("metric-profiles")).toHaveText("12,000");
  await expect(page.getByTestId("metric-actual_interval")).toHaveText("0.025");
  await page
    .getByRole("spinbutton", { name: "标定脉冲数量", exact: true })
    .fill("2000");
  await page
    .getByRole("spinbutton", { name: "编码器脉冲频率", exact: true })
    .fill("800");
  await page.getByLabel("仅查看冲突和待确认").uncheck();
  await expect(page.getByTestId("check-scan_speed")).toHaveClass(/passed/);
  await expect(page.getByTestId("check-encoder_speed")).toHaveClass(/passed/);
  await expect(page.getByTestId("check-sampling_margin")).toHaveClass(
    /unknown/,
  );
  await page
    .getByRole("spinbutton", { name: "标定脉冲数量", exact: true })
    .fill("4000");
  await page
    .getByRole("spinbutton", { name: "编码器脉冲频率", exact: true })
    .fill("1600");
  await page.getByRole("button", { name: "覆盖与采样", exact: true }).click();
  await page
    .getByRole("spinbutton", { name: "运动扫描长度", exact: true })
    .fill("3000");
  await page.getByLabel("仅查看冲突和待确认").check();
  await expect(page.getByTestId("metric-profiles")).toHaveText("120,000");
  await expect(page.getByTestId("metric-required_profiles")).toHaveText(
    "60,000",
  );
  await expect(page.getByTestId("check-buffer")).toHaveClass(/unknown/);
  await page.getByTestId("check-buffer").locator("summary").click();
  await expect(page.getByTestId("check-buffer")).toContainText("120,000");
  await page.screenshot({
    path: ".codex_tmp/three-d-fix/三维工作台-采集容量.png",
    fullPage: true,
    animations: "disabled",
  });
});

test("旧方案草稿和导出报告采用修正，保存的原始快照可追溯", async ({
  page,
  request,
}) => {
  const bootstrap: Bootstrap = await api(request, "bootstrap");
  const data = JSON.parse(
    await readFile("resources/data/three_d_cameras.json", "utf8"),
  );
  const specs = data.cameras.find(
    (item: { model: string }) => item.model === "Gocator 2320",
  );
  const project: Project = {
    ...bootstrap.default_project,
    mode: "scanning",
    name: "VSTEST-三维旧快照修复",
    hardware: [
      null,
      null,
      null,
      {
        id: "three_d:lmi:gocator 2320",
        kind: "three_d",
        model: "Gocator 2320",
        manufacturer: "LMI",
        origin: "内置资料",
        specs,
      },
    ],
    parameters: {
      ...bootstrap.default_project.parameters,
      scan_distance: 40,
      scan_width: 20,
    },
  };
  await api(request, "save_project", { project });
  try {
    await page.addInitScript(
      (p) =>
        localStorage.setItem(
          "visionselect.rust-workbench.draft.v1",
          JSON.stringify(p),
        ),
      project,
    );
    await ready(page);
    await expect(page.getByTestId("check-scan_fov")).toHaveClass(/failed/);
    await expect(
      page.getByTestId("check-scan_fov").locator(".check-values"),
    ).toContainText("当前 18");
    await expect(page.locator(".save-state")).toHaveText("已保存");
    await page.getByRole("button", { name: "导出", exact: true }).click();
    const download = page.waitForEvent("download");
    await page
      .getByRole("button", { name: "工程校核记录", exact: true })
      .click();
    const exported = await download;
    const report = await readFile((await exported.path())!, "utf8");
    expect(report).toContain("当前 18.000 / 要求 20.000");
    expect(report).toContain("7500.000000");
    expect(report).toContain("已按原厂规格表校正");
    const saved = await api(request, "load_project", { id: project.id });
    expect(saved.hardware[3].specs.xFovReferenceMm).toBe(22);
  } finally {
    await api(request, "delete_project", { id: project.id });
  }
});
