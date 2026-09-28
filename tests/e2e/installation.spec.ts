import { test, expect, type APIRequestContext } from "@playwright/test";
import AxeBuilder from "@axe-core/playwright";

async function api(request: APIRequestContext, operation: string, data = {}) {
  const response = await request.post(
    `http://127.0.0.1:4318/api/${operation}`,
    { data },
  );
  expect(response.ok(), await response.text()).toBeTruthy();
  return response.json();
}

test("安装图距离、公差随 Rust 方案保存，切换视图不修改方案", async ({
  page,
  request,
}) => {
  const { default_project: project } = await api(request, "bootstrap");
  project.name = "安装图持久化验证";
  for (const [index, kind] of ["camera", "lens", "light"].entries()) {
    const result = await api(request, "catalog", { kind, limit: 200 });
    project.hardware[index] =
      kind === "light"
        ? (result.items.find(
            (h: { specs: { light_type: string } }) =>
              h.specs.light_type === "Coaxial",
          ) ?? result.items[0])
        : result.items[0];
  }
  project.parameters.distance = 180;
  await page.addInitScript((p) => {
    if (!localStorage.getItem("visionselect.rust-workbench.draft.v1"))
      localStorage.setItem(
        "visionselect.rust-workbench.draft.v1",
        JSON.stringify(p),
      );
  }, project);
  await page.goto("/");
  await expect(page.locator(".live-indicator")).toHaveText("即时联动");
  await page.getByRole("button", { name: "安装主视图", exact: true }).click();
  await expect(page.getByTestId("installation-distance")).toHaveText("180 mm");
  await expect(page.getByTestId("installation-light-distance")).toHaveText(
    "待确认",
  );
  for (const h of project.hardware.slice(0, 3))
    await expect(page.locator(".installation-devices")).toContainText(h.model);
  await page.getByLabel("照明布置预览").selectOption("coaxial");
  await page
    .getByRole("spinbutton", { name: "光源工作距离", exact: true })
    .fill("60");
  await page
    .getByRole("spinbutton", { name: "镜头安装公差 ±", exact: true })
    .fill("10");
  await page
    .getByRole("spinbutton", { name: "光源安装公差 ±", exact: true })
    .fill("10");
  await expect(page.getByTestId("installation-distance")).toHaveText(
    "180 ± 10 mm",
  );
  await expect(page.getByTestId("installation-light-distance")).toHaveText(
    "60 ± 10 mm",
  );
  await page.getByRole("button", { name: "保存方案", exact: true }).click();
  await expect(page.locator(".save-state")).toHaveText("已保存");
  const saved = await api(request, "load_project", { id: project.id });
  expect(saved.parameters.light_distance).toBe(60);
  expect(saved.parameters.distance_tolerance).toBe(10);
  expect(saved.parameters.light_distance_tolerance).toBe(10);
  await page.reload();
  await expect(page.locator(".save-state")).toHaveText("已保存");
  await page.getByRole("button", { name: "安装主视图", exact: true }).click();
  await expect(page.getByTestId("installation-light-distance")).toHaveText(
    "60 ± 10 mm",
  );
  await page.getByRole("button", { name: "成像平面", exact: true }).click();
  const front = page.getByRole("button", { name: "安装主视图", exact: true });
  await front.focus();
  await page.keyboard.press("Enter");
  await expect(front).toHaveAttribute("aria-pressed", "true");
  await expect(page.locator(".save-state")).toHaveText("已保存");
  for (const layout of ["ring", "side", "coaxial", "backlight", "dome"]) {
    await page.getByLabel("照明布置预览").selectOption(layout);
    await expect(page.locator(".installation-drawing")).toHaveAttribute(
      "data-layout",
      layout,
    );
    await page.locator(".installation-view").screenshot({
      path: `.codex_tmp/installation-view/${layout}.png`,
      animations: "disabled",
    });
  }
  await page.getByLabel("照明布置预览").selectOption("coaxial");
  for (const theme of ["porcelain", "graphite", "sand", "sage"]) {
    await page.evaluate(
      (theme) => document.documentElement.setAttribute("data-theme", theme),
      theme,
    );
    await page.locator(".installation-view").screenshot({
      path: `.codex_tmp/installation-view/${theme}.png`,
      animations: "disabled",
    });
  }
  await page.evaluate(() =>
    document.documentElement.setAttribute("data-theme", "porcelain"),
  );
  const accessibility = await new AxeBuilder({ page })
    .include(".installation-view")
    .analyze();
  expect(accessibility.violations).toEqual([]);
  await page.setViewportSize({ width: 1024, height: 768 });
  expect(
    await page
      .locator(".installation-canvas")
      .evaluate((el) => el.scrollWidth <= el.clientWidth),
  ).toBeTruthy();
  await page.locator(".installation-view").screenshot({
    path: ".codex_tmp/installation-view/窄窗口.png",
    animations: "disabled",
  });
  expect(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= innerWidth,
    ),
  ).toBeTruthy();
  await page
    .getByRole("spinbutton", { name: "光源工作距离", exact: true })
    .fill("-1");
  await expect(
    page.getByRole("button", { name: "保存方案", exact: true }),
  ).toBeDisabled();
  expect(await page.locator(".installation-drawing").innerHTML()).not.toMatch(
    /NaN|Infinity/,
  );
  await api(request, "delete_project", { id: project.id });
});

test("空硬件、距离变化与三维模式隔离", async ({ page }) => {
  const errors: string[] = [];
  page.on("pageerror", (e) => errors.push(e.message));
  await page.goto("/");
  await page.getByRole("button", { name: "安装主视图", exact: true }).click();
  await expect(page.locator(".installation-drawing")).toHaveAttribute(
    "data-layout",
    "unknown",
  );
  await expect(page.locator(".installation-devices")).toContainText("尚未选择");
  await page
    .getByRole("spinbutton", { name: "镜头工作距离", exact: true })
    .fill("250");
  await expect(page.getByTestId("installation-distance")).toHaveText("250 mm");
  await page
    .getByRole("spinbutton", { name: "镜头安装公差 ±", exact: true })
    .fill("0");
  await expect(page.getByTestId("installation-distance")).toHaveText(
    "250 ± 0 mm",
  );
  await page
    .getByRole("spinbutton", { name: "镜头安装公差 ±", exact: true })
    .fill("");
  await expect(page.getByTestId("installation-distance")).toHaveText("250 mm");
  await page.getByRole("button", { name: "三维采样", exact: true }).click();
  await expect(
    page.getByRole("button", { name: "安装主视图", exact: true }),
  ).toHaveCount(0);
  expect(errors).toEqual([]);
});

test("线扫镜头保留实测视场，微小安装公差保存后仍准确显示", async ({
  page,
  request,
}) => {
  const { default_project: project } = await api(request, "bootstrap");
  project.name = "安装标注精度回归";
  project.hardware[0] = (
    await api(request, "catalog", { kind: "camera", limit: 1 })
  ).items[0];
  project.hardware[1] = (
    await api(request, "catalog", { kind: "lens", search: "XF-ZFL3296-1.82X" })
  ).items[0];
  expect(project.hardware[1]).toBeTruthy();
  project.parameters.measured = true;
  project.parameters.measured_width = 24;
  project.parameters.measured_height = 12;
  await page.addInitScript((p) => {
    if (!localStorage.getItem("visionselect.rust-workbench.draft.v1"))
      localStorage.setItem(
        "visionselect.rust-workbench.draft.v1",
        JSON.stringify(p),
      );
  }, project);
  await page.goto("/");
  await expect(page.getByTestId("metric-fov_x")).toHaveText("24 × 12");
  await expect(page.getByTestId("check-lens_application")).toContainText(
    "待确认",
  );
  await expect(page.getByTestId("check-fov_y")).toContainText("冲突");
  await page.getByRole("button", { name: "安装主视图", exact: true }).click();
  await page
    .getByRole("spinbutton", { name: "镜头安装公差 ±", exact: true })
    .fill("0.001");
  await page
    .getByRole("spinbutton", { name: "光源工作距离", exact: true })
    .fill("60");
  await page
    .getByRole("spinbutton", { name: "光源安装公差 ±", exact: true })
    .fill("0.0001");
  await expect(page.getByTestId("installation-distance")).toHaveText(
    "110 ± 0.001 mm",
  );
  await expect(page.getByTestId("installation-light-distance")).toHaveText(
    "60 ± 0.0001 mm",
  );
  try {
    await page.getByRole("button", { name: "保存方案", exact: true }).click();
    await expect(page.locator(".save-state")).toHaveText("已保存");
    const saved = await api(request, "load_project", { id: project.id });
    expect(saved.parameters.distance_tolerance).toBe(0.001);
    expect(saved.parameters.light_distance_tolerance).toBe(0.0001);
    await page.reload();
    await expect(page.getByTestId("metric-fov_x")).toHaveText("24 × 12");
    await page.getByRole("button", { name: "安装主视图", exact: true }).click();
    await expect(page.getByTestId("installation-distance")).toHaveText(
      "110 ± 0.001 mm",
    );
    await expect(page.getByTestId("installation-light-distance")).toHaveText(
      "60 ± 0.0001 mm",
    );
    await page.setViewportSize({ width: 1000, height: 720 });
    for (const theme of ["graphite", "porcelain"]) {
      await page.evaluate(
        (value) => document.documentElement.setAttribute("data-theme", value),
        theme,
      );
      await page.locator(".installation-view").screenshot({
        path: `.codex_tmp/installation-view/公差精度-${theme}.png`,
        animations: "disabled",
      });
    }
    await page.getByRole("button", { name: "采集与校准", exact: true }).click();
    await page.getByLabel("使用实测视场覆盖估算").uncheck();
    await expect(page.getByTestId("metric-fov_x")).toHaveText("— × —");
    await expect(page.getByTestId("check-lens_application")).toContainText(
      "待确认",
    );
  } finally {
    await api(request, "delete_project", { id: project.id });
  }
});
