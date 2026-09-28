import {
  test,
  expect,
  type APIRequestContext,
  type Page,
} from "@playwright/test";
import { readFile } from "node:fs/promises";
import AxeBuilder from "@axe-core/playwright";

const prefix = "VSTEST-";
async function api(
  request: APIRequestContext,
  operation: string,
  payload = {},
) {
  const response = await request.post(
    "http://127.0.0.1:4318/api/" + operation,
    { data: payload },
  );
  expect(response.ok(), await response.text()).toBeTruthy();
  return response.json();
}
test.beforeAll(async ({ request }) => {
  const files = [
    {
      kind: "camera",
      content:
        "model,manufacturer,resolution_x,resolution_y,pixel_size_um,lens_mount,max_fps,shutter_type,bandwidth_mbps,bandwidth_source\nVSTEST-CAM-2000,测试厂家,2000,1000,5,C,100,Global,125,specified",
    },
    {
      kind: "lens",
      content:
        "model,manufacturer,lens_type,focal_length_mm,image_circle_mm,min_wd_mm,lens_mount,dof_mm\nVSTEST-LENS-25,测试厂家,fixed,25,12,100,C,6",
    },
    {
      kind: "light",
      content:
        "model,manufacturer,light_type,color,active_width_mm,active_height_mm\nVSTEST-LIGHT-100,测试厂家,背光,白色,100,100",
    },
  ];
  for (const file of files) {
    const preview = await api(request, "import_preview", {
      ...file,
      format: "csv",
    });
    await api(request, "import_commit", { token: preview.token });
  }
  const content = JSON.stringify({
    schemaVersion: 1,
    cameras: [
      {
        manufacturer: "测试厂家",
        model: "VSTEST-3D-轮廓扫描与超长中文型号-精密检测系列",
        technology: "激光轮廓",
        scanRateMaxHz: 2000,
        profileDataIntervalUm: 5,
        zMeasurementRangeMm: 20,
        zRepeatabilityUm: 2,
        workingDistanceMinMm: 100,
        referenceDistanceMm: 150,
        workingDistanceMaxMm: 200,
        xFovNearMm: 30,
        xFovReferenceMm: 50,
        xFovFarMm: 70,
        supportsEncoder: true,
        supportsExternalTrigger: false,
        encoderRateMaxHz: 100000,
        exposureTimeMinUs: 10,
        exposureTimeMaxUs: 500,
      },
    ],
  });
  const preview = await api(request, "import_preview", {
    kind: "three_d",
    format: "json",
    content,
  });
  await api(request, "import_commit", { token: preview.token });
});
test.afterAll(async ({ request }) => {
  const data = await api(request, "bootstrap");
  for (const p of data.projects)
    if (p.name.startsWith(prefix))
      await api(request, "delete_project", { id: p.id });
});
test.beforeEach(async ({ page }) => {
  const errors: string[] = [];
  page.on("pageerror", (e) => errors.push(e.message));
  page.on("dialog", (dialog) => dialog.accept());
  await page.goto("/");
  await expect(page.getByRole("heading", { name: "方案工作台" })).toBeVisible();
  await expect(page.locator(".live-indicator")).toHaveText("即时联动");
  (page as Page & { errors: string[] }).errors = errors;
});
test.afterEach(async ({ page }) => {
  expect((page as Page & { errors: string[] }).errors).toEqual([]);
});
async function choose(page: Page, category: string, model: string) {
  await page
    .getByRole("button", { name: "选择" + category, exact: true })
    .click();
  const dialog = page.getByRole("dialog");
  await dialog.getByRole("textbox", { name: "搜索硬件" }).fill(model);
  await dialog
    .getByRole("button", { name: model + " 测试厂家", exact: true })
    .click();
  await dialog.getByRole("button", { name: "加入当前方案" }).click();
  await expect(dialog).not.toBeVisible();
}
async function settle(page: Page) {
  await expect(page.locator(".live-indicator")).toHaveText("即时联动");
}
test("二维组合、实时 Rust 校核、固定参考与保存重开", async ({
  page,
  request,
}) => {
  expect(
    (await (await request.get("http://127.0.0.1:4318/api/health")).json())
      .engine,
  ).toBe("rust");
  await page
    .getByRole("textbox", { name: "方案名称" })
    .fill(prefix + "二维方案");
  await choose(page, "面阵相机", "VSTEST-CAM-2000");
  await choose(page, "成像镜头", "VSTEST-LENS-25");
  await choose(page, "照明光源", "VSTEST-LIGHT-100");
  await page.getByRole("spinbutton", { name: "镜头工作距离" }).fill("200");
  await expect(page.getByTestId("metric-fov_x")).toHaveText("80 × 40");
  await expect(page.getByTestId("metric-pixel")).toHaveText("40");
  await expect(page.getByTestId("metric-bandwidth")).toHaveText("—");
  await page.getByRole("button", { name: "采集与校准" }).click();
  await page.getByLabel("实际传输像素格式").selectOption("Mono8");
  await expect(page.getByTestId("metric-bandwidth")).toHaveText("40");
  await page.getByRole("button", { name: "成像目标", exact: true }).click();
  await page.getByRole("spinbutton", { name: "像素当量上限" }).fill("50");
  await page.getByRole("button", { name: "设为参考", exact: true }).click();
  await settle(page);
  await page.getByRole("button", { name: "关闭提示" }).click();
  await page.locator(".workbench").evaluate((el) => {
    el.scrollTop = 0;
  });
  await page.screenshot({
    path: ".codex_tmp/rust-ui/二维工作台.png",
    fullPage: true,
  });
  await page.getByRole("spinbutton", { name: "镜头工作距离" }).fill("400");
  await expect(page.getByTestId("metric-fov_x")).toHaveText("160 × 80");
  await page.getByRole("button", { name: "对比方案" }).click();
  const row = page
    .getByRole("dialog")
    .getByRole("row")
    .filter({
      has: page.getByRole("rowheader", { name: "水平视场", exact: true }),
    });
  await expect(row).toContainText("80 mm");
  await expect(row).toContainText("160 mm");
  await page.screenshot({
    path: ".codex_tmp/rust-ui/方案对照.png",
    fullPage: true,
  });
  await page.keyboard.press("Escape");
  await page.getByRole("button", { name: "保存方案", exact: true }).click();
  await expect(page.locator(".save-state")).toHaveText("已保存");
  await page.reload();
  await expect(page.locator(".save-state")).toHaveText("已保存");
  await page.getByRole("button", { name: "新建方案", exact: true }).click();
  await expect(page.locator(".save-state")).toHaveText("草稿");
  await page.getByRole("button", { name: "我的方案" }).click();
  await page
    .locator(".project-card")
    .filter({ hasText: prefix + "二维方案" })
    .getByRole("button", { name: "继续编辑" })
    .click();
  await expect(
    page.getByRole("spinbutton", { name: "镜头工作距离" }),
  ).toHaveValue("400");
  await expect(page.getByTestId("metric-fov_x")).toHaveText("160 × 80");
  await page.reload();
  await expect(page.getByRole("textbox", { name: "方案名称" })).toHaveValue(
    prefix + "二维方案",
  );
  await expect(page.locator(".save-state")).toHaveText("已保存");
});
test("无效参数阻止保存导出，取消离开保留当前方案", async ({ page }) => {
  await page.getByRole("spinbutton", { name: "工件宽度" }).fill("");
  await expect(
    page.getByRole("heading", { name: "请完成参数输入" }),
  ).toBeVisible();
  await expect(
    page.getByRole("button", { name: "保存方案", exact: true }),
  ).toBeDisabled();
  await expect(
    page.getByRole("button", { name: "导出", exact: true }),
  ).toBeDisabled();
  await page.getByRole("button", { name: "新建方案", exact: true }).click();
  await expect(page.getByRole("dialog")).toContainText("当前有未保存修改");
  await page.getByRole("button", { name: "返回", exact: true }).click();
  await expect(page.getByRole("spinbutton", { name: "工件宽度" })).toHaveValue(
    "",
  );
  await page.getByRole("spinbutton", { name: "工件宽度" }).fill("25");
  await expect(
    page.getByRole("button", { name: "保存方案", exact: true }),
  ).toBeEnabled();
});
test("资料库分页、无结果、导入错误与预览提交", async ({ page }) => {
  await page.getByRole("button", { name: "选择面阵相机", exact: true }).click();
  const dialog = page.getByRole("dialog");
  await expect(dialog.locator(".hardware-table tbody tr")).toHaveCount(60);
  await dialog.getByRole("button", { name: "下一页" }).click();
  await expect(dialog.locator(".pagination")).toContainText("61–120");
  await dialog.getByLabel("搜索硬件").fill("不存在的型号ZZZ");
  await expect(
    dialog.getByRole("heading", { name: "没有找到匹配设备" }),
  ).toBeVisible();
  await expect(
    dialog.getByRole("button", { name: "加入当前方案" }),
  ).toHaveCount(0);
  await dialog.getByLabel("导入硬件文件").setInputFiles({
    name: "错误.csv",
    mimeType: "text/csv",
    buffer: Buffer.from("错误表头\n内容"),
  });
  const wizard = page.getByRole("dialog", {
    name: "导入硬件参数表",
    exact: true,
  });
  await expect(
    wizard.getByRole("button", { name: "检查导入差异" }),
  ).toBeDisabled();
  await expect(wizard).toContainText("请保证型号与厂家已对应");
  await wizard.getByRole("button", { name: "取消", exact: true }).click();
  const importedModel = "VSTEST-IMPORT-" + Date.now();
  const csv = `model,manufacturer,resolution_x\n${importedModel},测试厂家,1024`;
  await dialog.getByLabel("导入硬件文件").setInputFiles({
    name: "回归.csv",
    mimeType: "text/csv",
    buffer: Buffer.from(csv),
  });
  await wizard.getByRole("button", { name: "检查导入差异" }).click();
  await wizard
    .getByRole("button", { name: "确认导入 1 条", exact: true })
    .click();
  await expect(dialog.getByRole("status")).toContainText("已导入 1 条设备");
  await dialog.getByLabel("搜索硬件").fill(importedModel);
  await expect(dialog.locator(".hardware-table tbody tr")).toHaveCount(1);
  await dialog
    .getByRole("button", { name: importedModel + " 测试厂家" })
    .click();
  await page.screenshot({
    path: ".codex_tmp/rust-ui/硬件资料库.png",
    fullPage: true,
  });
});
test("三维轮廓、编码器冲突与二维参数保留", async ({ page }) => {
  await page.getByRole("spinbutton", { name: "工件宽度" }).fill("32");
  await page.getByRole("button", { name: "三维采样", exact: true }).click();
  await choose(
    page,
    "3D 相机",
    "VSTEST-3D-轮廓扫描与超长中文型号-精密检测系列",
  );
  await expect(page.getByTestId("metric-profiles")).toHaveText("7,500");
  await expect(page.getByTestId("metric-required_profiles")).toHaveText(
    "6,000",
  );
  await expect(page.getByTestId("metric-speed")).toHaveText("50");
  await page
    .getByRole("spinbutton", { name: "实际工作距离（可留空）" })
    .fill("150");
  await page.getByRole("button", { name: "触发与曝光" }).click();
  await page.getByLabel("触发方式").selectOption("encoder");
  await page.getByRole("spinbutton", { name: "编码器脉冲频率" }).fill("40000");
  await settle(page);
  await expect(
    page.locator(".check-item.unknown").filter({ hasText: "建议采样裕量" }),
  ).toBeVisible();
  await page.getByRole("spinbutton", { name: "每轮廓脉冲数" }).fill("100");
  await expect(
    page
      .locator(".check-item.failed")
      .filter({ hasText: "编码器实际轮廓间距" }),
  ).toBeVisible();
  await page.screenshot({
    path: ".codex_tmp/rust-ui/三维采样.png",
    fullPage: true,
  });
  await page.getByRole("button", { name: "二维成像", exact: true }).click();
  await page.getByRole("button", { name: "成像目标", exact: true }).click();
  await expect(page.getByRole("spinbutton", { name: "工件宽度" })).toHaveValue(
    "32",
  );
});
test("导出可重新导入的方案与可打印校核记录", async ({ page }) => {
  await page
    .getByRole("textbox", { name: "方案名称" })
    .fill(prefix + "导出方案");
  await settle(page);
  await page.getByRole("button", { name: "导出", exact: true }).click();
  const download = page.waitForEvent("download");
  await page.getByRole("button", { name: "工程方案 JSON" }).click();
  const file = await download;
  const path = await file.path();
  const data = JSON.parse(await readFile(path!, "utf8"));
  expect(data.name).toBe(prefix + "导出方案");
  await page.getByRole("button", { name: "我的方案" }).click();
  await page.getByLabel("导入方案文件").setInputFiles(path!);
  await expect(page.locator(".save-state")).toHaveText("已保存");
  await expect(page.getByRole("textbox", { name: "方案名称" })).toHaveValue(
    prefix + "导出方案",
  );
  await settle(page);
  await page.getByRole("button", { name: "导出", exact: true }).click();
  const reportDownload = page.waitForEvent("download");
  await page.getByRole("button", { name: "工程校核记录", exact: true }).click();
  const report = await reportDownload;
  expect(await readFile((await report.path())!, "utf8")).toContain("逐项校核");
});
test("窄窗口、键盘关闭和损坏草稿恢复", async ({ page }) => {
  await page.setViewportSize({ width: 1024, height: 768 });
  await expect(
    page.getByRole("button", { name: "保存方案", exact: true }),
  ).toBeInViewport();
  await expect(
    page.getByRole("spinbutton", { name: "工件宽度" }),
  ).toBeInViewport();
  expect(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= innerWidth,
    ),
  ).toBeTruthy();
  await page.screenshot({
    path: ".codex_tmp/rust-ui/窄窗口.png",
    fullPage: true,
  });
  await page.getByRole("button", { name: "选择面阵相机", exact: true }).click();
  await page.keyboard.press("Escape");
  await expect(page.getByRole("dialog")).not.toBeVisible();
  await page.evaluate(() =>
    localStorage.setItem(
      "visionselect.rust-workbench.draft.v1",
      '{"version":1,"parameters":{},"hardware":[null,null,null,null]}',
    ),
  );
  await page.reload();
  await expect(page.getByRole("textbox", { name: "方案名称" })).toHaveValue(
    "未命名方案",
  );
});

test("工作台与硬件弹窗的无障碍检查", async ({ page }) => {
  const workspace = await new AxeBuilder({ page })
    .withTags(["wcag2a", "wcag2aa"])
    .analyze();
  expect(
    workspace.violations.map((v) => ({
      id: v.id,
      nodes: v.nodes.map((n) => ({
        target: n.target,
        summary: n.failureSummary,
      })),
    })),
  ).toEqual([]);
  await page.getByRole("button", { name: "选择面阵相机", exact: true }).click();
  await expect(
    page.getByRole("dialog").locator(".hardware-table tbody tr"),
  ).toHaveCount(60);
  await page.getByRole("dialog").locator(".model-link").first().click();
  const library = await new AxeBuilder({ page })
    .withTags(["wcag2a", "wcag2aa"])
    .analyze();
  expect(
    library.violations.map((v) => ({
      id: v.id,
      nodes: v.nodes.map((n) => ({
        target: n.target,
        summary: n.failureSummary,
      })),
    })),
  ).toEqual([]);
});
