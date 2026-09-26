import { test, expect, type Page } from "@playwright/test";
import AxeBuilder from "@axe-core/playwright";

async function open(page: Page) {
  await page.goto("/");
  await page.getByRole("button", { name: "2D 选型计算", exact: true }).click();
  await expect(page.getByTestId("calc-minimum_resolution")).toBeVisible();
}
async function fill(page: Page, fields: Record<string, string>) {
  for (const [name, value] of Object.entries(fields))
    await page.getByRole("spinbutton", { name, exact: true }).fill(value);
}
test("分辨率双轴推算、继续计算镜头与应用参数", async ({ page }) => {
  await open(page);
  await fill(page, {
    工件宽度: "100",
    工件高度: "80",
    单边余量: "0",
    像素当量上限: "25",
    水平有效像素: "4000",
    垂直有效像素: "2000",
    像元尺寸: "5",
  });
  await expect(page.getByTestId("calc-minimum_resolution")).toHaveText(
    "4,000 × 3,200",
  );
  await expect(page.getByTestId("calc-sampling")).toHaveText("40");
  await expect(page.locator(".calculation-verdict")).toContainText(
    "分辨率不足",
  );
  await page.getByRole("spinbutton", { name: "垂直有效像素" }).fill("4000");
  await expect(page.locator(".calculation-verdict")).toContainText("满足两轴");
  await page.getByRole("button", { name: "用这个视场继续算镜头" }).click();
  await expect(page.getByRole("combobox", { name: "计算目标" })).toHaveValue(
    "focal",
  );
  await expect(
    page.getByRole("spinbutton", { name: "目标视场宽度" }),
  ).toHaveValue("100");
  await page
    .getByRole("spinbutton", { name: "工作距离", exact: true })
    .fill("400");
  await expect(page.getByTestId("calc-focal")).toHaveText("80");
  await page.getByRole("button", { name: "应用参数", exact: true }).click();
  await expect(page.getByRole("heading", { name: "方案工作台" })).toBeVisible();
  await expect(
    page.getByRole("spinbutton", { name: "镜头工作距离" }),
  ).toHaveValue("400");
  await expect(page.getByRole("spinbutton", { name: "工件宽度" })).toHaveValue(
    "20",
  );
});

test("空值与不适用光学条件清除旧结果，草稿可恢复", async ({ page }) => {
  await open(page);
  await page.getByRole("spinbutton", { name: "工件宽度" }).fill("");
  await expect(
    page.getByRole("button", { name: "应用参数", exact: true }),
  ).toBeDisabled();
  await expect(page.getByTestId("calc-minimum_resolution")).not.toBeVisible();
  await expect(page.getByRole("alert")).toContainText("工件宽度");
  await page.getByRole("spinbutton", { name: "工件宽度" }).fill("0.001");
  await expect(page.getByTestId("calc-minimum_resolution")).toBeVisible();
  await expect(
    page.getByRole("button", { name: "应用参数", exact: true }),
  ).toBeDisabled();
  await expect(page.locator(".calculation-actions")).toContainText(
    "工作台的工件宽度",
  );
  await page.getByRole("spinbutton", { name: "工件宽度" }).fill("72");
  await expect(page.getByTestId("calc-minimum_resolution")).toBeVisible();
  await page.getByRole("button", { name: "硬件资料库", exact: true }).click();
  await page.getByRole("button", { name: "2D 选型计算", exact: true }).click();
  await expect(page.getByRole("spinbutton", { name: "工件宽度" })).toHaveValue(
    "72",
  );
  await page.reload();
  await page.getByRole("button", { name: "2D 选型计算", exact: true }).click();
  await expect(page.getByRole("spinbutton", { name: "工件宽度" })).toHaveValue(
    "72",
  );
  await page.getByRole("tab", { name: /镜头与工作距离/ }).click();
  await page
    .getByRole("spinbutton", { name: "工作距离", exact: true })
    .fill("10");
  await expect(page.getByRole("alert")).toContainText("工作距离必须大于焦距");
  await expect(
    page.getByRole("button", { name: "应用参数", exact: true }),
  ).toBeDisabled();
  await page
    .getByRole("spinbutton", { name: "工作距离", exact: true })
    .fill("300");
  await expect(page.getByTestId("calc-fov")).toBeVisible();
});

test("运动静止边界和打包格式通过真实 API 计算", async ({ page }) => {
  await open(page);
  await page.getByRole("tab", { name: /运动与曝光/ }).click();
  await fill(page, {
    物方像素当量: "10",
    运动速度: "100",
    曝光时间: "100",
    允许拖影: "1",
    目标帧率: "30",
  });
  await expect(page.getByTestId("calc-motion_blur")).toHaveText("1");
  await expect(page.getByTestId("calc-exposure_limit")).toHaveText("100");
  await page.getByRole("spinbutton", { name: "运动速度" }).fill("0");
  await expect(page.getByTestId("calc-motion_limit")).toHaveText("—");
  await expect(page.getByTestId("calc-exposure_limit")).toHaveText(
    "33,333.333",
  );
  await page.getByRole("tab", { name: /传输与存储/ }).click();
  await fill(page, {
    水平有效像素: "2000",
    垂直有效像素: "1000",
    目标帧率: "20",
    记录时长: "60",
  });
  await page
    .getByRole("combobox", { name: "传输像素格式" })
    .selectOption("Mono12");
  await expect(page.getByTestId("calc-payload")).toHaveText("80");
  await expect(page.getByTestId("calc-storage")).toHaveText("4.8");
  await page
    .getByRole("combobox", { name: "传输像素格式" })
    .selectOption("Mono12p");
  await expect(page.getByTestId("calc-payload")).toHaveText("60");
  await expect(page.getByTestId("calc-storage")).toHaveText("3.6");
});

test("读取真实设备与实测像素，应用时保留硬件和三维参数", async ({
  page,
  request,
}) => {
  const api = async (operation: string, data = {}) => {
    const response = await request.post(
      "http://127.0.0.1:4318/api/" + operation,
      { data },
    );
    expect(response.ok()).toBeTruthy();
    return response.json();
  };
  const data = await api("bootstrap");
  const camera = (
    await api("catalog", { kind: "camera", search: "COE-050-M-USB-070-IR-C" })
  ).items[0];
  const lens = (
    await api("catalog", { kind: "lens", search: "CSWH-03X110V3TF" })
  ).items[0];
  const project = data.default_project;
  project.hardware = [camera, lens, null, null];
  project.name = "计算读取验证";
  Object.assign(project.parameters, {
    width: 40,
    height: 20,
    measured: true,
    measured_width: 40,
    measured_height: 20,
    scan_length: 777,
    pixel_format: "Mono12p",
  });
  await page.addInitScript(
    (project) =>
      localStorage.setItem(
        "visionselect.rust-workbench.draft.v1",
        JSON.stringify(project),
      ),
    project,
  );
  await open(page);
  await page.getByRole("button", { name: "读取当前方案" }).click();
  await expect(
    page.getByRole("spinbutton", { name: "水平有效像素" }),
  ).toHaveValue("2448");
  await expect(page.getByRole("spinbutton", { name: "像元尺寸" })).toHaveValue(
    "3.45",
  );
  await page.getByRole("tab", { name: /运动与曝光/ }).click();
  await expect(
    page.getByRole("spinbutton", { name: "物方像素当量" }),
  ).toHaveValue("9.765625");
  await page.getByRole("tab", { name: /镜头与工作距离/ }).click();
  await expect(page.getByRole("combobox", { name: "计算目标" })).toHaveValue(
    "telecentric_fov",
  );
  await expect(page.getByRole("spinbutton", { name: "远心倍率" })).toHaveValue(
    "0.3",
  );
  await expect(
    page.getByRole("button", { name: "应用参数", exact: true }),
  ).toBeDisabled();
  await page.getByRole("tab", { name: /视场与分辨率/ }).click();
  await page.getByRole("spinbutton", { name: "工件宽度" }).fill("48");
  await expect(
    page.getByRole("button", { name: "应用参数", exact: true }),
  ).toBeEnabled();
  await page.getByRole("button", { name: "应用参数", exact: true }).click();
  await expect(page.getByRole("spinbutton", { name: "工件宽度" })).toHaveValue(
    "48",
  );
  const current = await page.evaluate(() =>
    JSON.parse(localStorage.getItem("visionselect.rust-workbench.draft.v1")!),
  );
  expect(current.hardware).toEqual(project.hardware);
  expect(current.parameters.scan_length).toBe(777);
  expect(current.parameters.measured).toBe(true);
  expect(current.parameters.measured_width).toBe(40);
});

test("四组计算的键盘导航、对比度与窄窗口布局", async ({ page }) => {
  await open(page);
  await page.getByRole("tab", { name: /视场与分辨率/ }).focus();
  await page.keyboard.press("ArrowRight");
  await expect(page.getByRole("tab", { name: /镜头与工作距离/ })).toBeFocused();
  await expect(page.getByRole("combobox", { name: "计算目标" })).toBeVisible();
  for (const name of [
    /视场与分辨率/,
    /镜头与工作距离/,
    /运动与曝光/,
    /传输与存储/,
  ]) {
    await page.getByRole("tab", { name }).click();
    await expect(
      page.locator(".calculation-results .live-indicator"),
    ).toHaveText("已更新");
    const result = await new AxeBuilder({ page })
      .withTags(["wcag2a", "wcag2aa"])
      .analyze();
    expect(
      result.violations.map((v) => ({
        id: v.id,
        nodes: v.nodes.map((n) => n.target),
      })),
    ).toEqual([]);
  }
  await page.getByRole("tab", { name: /视场与分辨率/ }).click();
  await expect(page.getByTestId("calc-minimum_resolution")).toBeVisible();
  await expect(
    page.getByRole("button", { name: "应用参数", exact: true }),
  ).toBeInViewport();
  await page.screenshot({ path: ".codex_tmp/rust-ui/2D选型计算.png" });
  await page.setViewportSize({ width: 1000, height: 720 });
  expect(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= innerWidth,
    ),
  ).toBe(true);
  expect(
    await page
      .locator(".calculator-page")
      .evaluate((el) => el.scrollWidth <= el.clientWidth),
  ).toBe(true);
  await page.screenshot({ path: ".codex_tmp/rust-ui/2D选型计算-窄窗口.png" });
  await page.locator(".calculator-page").evaluate((el) => {
    el.scrollTop = el.scrollHeight;
  });
  await expect(
    page.getByRole("button", { name: "应用参数", exact: true }),
  ).toBeInViewport();
});
