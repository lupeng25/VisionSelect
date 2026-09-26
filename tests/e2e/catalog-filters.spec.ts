import {
  test,
  expect,
  type Page,
  type APIRequestContext,
} from "@playwright/test";
import AxeBuilder from "@axe-core/playwright";

const brand = "筛选回归厂";
async function api(request: APIRequestContext, operation: string, data = {}) {
  const response = await request.post(
    "http://127.0.0.1:4318/api/" + operation,
    { data },
  );
  expect(response.ok(), await response.text()).toBeTruthy();
  return response.json();
}
test.beforeAll(async ({ request }) => {
  const cameras = [
    "model,manufacturer,resolution_x,resolution_y,pixel_size_um,interface,color_mode,max_fps,shutter_type,lens_mount",
  ];
  for (let i = 1; i <= 61; i++)
    cameras.push(
      `VSFILTER-CAM-${String(i).padStart(3, "0")},${brand},2000,1000,5,USB3,Mono,100,Global,C`,
    );
  cameras.push(
    `VSFILTER-CAM-GIGE,${brand},2000,1000,5,GigE,Mono,100,Global,C`,
    `VSFILTER-CAM-LARGE,${brand},4000,1000,5,USB3,Mono,100,Global,C`,
    `VSFILTER-CAM-SLOW,${brand},2000,1000,5,USB3,Mono,30,Rolling,C`,
    `VSFILTER-CAM-COLOR,${brand},2000,1000,5,USB3,Color,100,Global,C`,
    `VSFILTER-CAM-UNKNOWN,${brand},2000,1000,5,USB3,Mono,,Unknown,C`,
  );
  for (const file of [
    { kind: "camera", content: cameras.join("\n") },
    {
      kind: "lens",
      content: `model,manufacturer,lens_type,lens_mount,focal_length_mm,image_circle_mm,pmag,nominal_wd_mm\nVSFILTER-LENS-25,${brand},FixedFocal,C,25,11,0,0\nVSFILTER-LENS-50,${brand},FixedFocal,C,50,22,0,0\nVSFILTER-LENS-TELE,${brand},ObjectTelecentric,C,0,11,0.3,110`,
    },
    {
      kind: "light",
      content: `model,manufacturer,light_type,color,active_width_mm,active_height_mm,mode,wavelength_nm\nVSFILTER-LIGHT-MULTI,${brand},Bar,White/Red/Blue,100,20,Continuous,0\nVSFILTER-LIGHT-RING,${brand},Ring,Red,50,50,Continuous,630\nVSFILTER-LIGHT-GREEN,${brand},Bar,Green,150,15,Continuous,530`,
    },
  ]) {
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
        manufacturer: brand,
        model: "VSFILTER-3D-A",
        technology: "线激光轮廓",
        interfaces: ["GigE", "I/O"],
        scanRateMaxHz: 2000,
        zMeasurementRangeMm: 20,
        supportsEncoder: true,
      },
      {
        manufacturer: brand,
        model: "VSFILTER-3D-B",
        technology: "结构光快照",
        interfaces: ["USB3"],
        zMeasurementRangeMm: 100,
        supportsEncoder: false,
      },
      {
        manufacturer: brand,
        model: "VSFILTER-3D-C",
        technology: "线激光轮廓",
        interfaces: ["10GigE"],
        scanRateMaxHz: 1000,
        zMeasurementRangeMm: 10,
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

async function open(page: Page, modal = false) {
  await page.goto("/");
  await page
    .getByRole("button", {
      name: modal ? "选择面阵相机" : "硬件资料库",
      exact: true,
    })
    .click();
  await expect(page.locator(".hardware-table tbody tr")).toHaveCount(60);
}
async function cameraFilters(page: Page) {
  await page.getByRole("combobox", { name: "筛选厂家" }).selectOption(brand);
  await page
    .getByRole("combobox", { name: "筛选相机接口" })
    .selectOption("value:usb3");
  await page
    .getByRole("combobox", { name: "筛选色彩类型" })
    .selectOption("value:mono");
  await page.getByRole("spinbutton", { name: "有效像素下限" }).fill("2");
  await page.getByRole("spinbutton", { name: "有效像素上限" }).fill("2");
  await page.getByRole("spinbutton", { name: "最高帧率下限" }).fill("60");
  await expect(page.locator(".pagination")).toContainText("1–60 / 61");
}

test("相机组合筛选按全库计数和翻页，范围变化重置分页", async ({ page }) => {
  await open(page);
  await cameraFilters(page);
  await page.getByRole("button", { name: "下一页", exact: true }).click();
  await expect(page.locator(".pagination")).toContainText("61–61 / 61");
  await expect(page.locator(".model-link")).toHaveCount(1);
  await expect(page.locator(".model-link")).toContainText("VSFILTER-CAM-061");
  await page.getByRole("spinbutton", { name: "最高帧率上限" }).fill("90");
  await expect(
    page.getByRole("heading", { name: "没有找到匹配设备" }),
  ).toBeVisible();
  await expect(
    page.getByRole("button", { name: "上一页", exact: true }),
  ).toBeDisabled();
  await page.getByRole("spinbutton", { name: "最高帧率上限" }).fill("50");
  await expect(page.getByRole("alert")).toContainText("下限不能大于上限");
  await expect(page.locator(".model-link")).toHaveCount(0);
  await page.getByRole("button", { name: "移除最高帧率筛选" }).click();
  await expect(page.locator(".pagination")).toContainText("1–60 / 63");
  await page.getByRole("button", { name: "清空全部筛选", exact: true }).click();
  await expect(
    page.getByRole("combobox", { name: "筛选相机接口" }),
  ).toHaveValue("");
  await expect(
    page.getByRole("spinbutton", { name: "有效像素下限" }),
  ).toHaveValue("");
  await expect(page.locator(".model-link")).toHaveCount(60);
});

test("四类规格与多值字段可筛选，分类之间保留独立条件", async ({ page }) => {
  await open(page);
  await cameraFilters(page);
  await page.getByRole("tab", { name: "成像镜头", exact: true }).click();
  await page.getByRole("combobox", { name: "筛选厂家" }).selectOption(brand);
  await page
    .getByRole("combobox", { name: "筛选镜头类型" })
    .selectOption("value:fixed_focal");
  await page.getByRole("spinbutton", { name: "焦距下限" }).fill("20");
  await page.getByRole("spinbutton", { name: "焦距上限" }).fill("30");
  await expect(page.locator(".model-link")).toHaveCount(1);
  await expect(page.locator(".model-link")).toContainText("VSFILTER-LENS-25");
  await page.getByRole("tab", { name: "照明光源", exact: true }).click();
  await page.getByRole("combobox", { name: "筛选厂家" }).selectOption(brand);
  await page
    .getByRole("combobox", { name: "筛选光源颜色" })
    .selectOption("value:blue");
  await page.getByRole("spinbutton", { name: "发光宽度下限" }).fill("100");
  await expect(page.locator(".model-link")).toHaveCount(1);
  await expect(page.locator(".model-link")).toContainText(
    "VSFILTER-LIGHT-MULTI",
  );
  await page.getByRole("tab", { name: "3D 相机", exact: true }).click();
  await page.getByRole("combobox", { name: "筛选厂家" }).selectOption(brand);
  await page
    .getByRole("combobox", { name: "筛选技术路线" })
    .selectOption("value:线激光轮廓");
  await page
    .getByRole("combobox", { name: "筛选设备接口" })
    .selectOption("value:gige");
  await expect(page.locator(".model-link")).toHaveCount(1);
  await expect(page.locator(".model-link")).toContainText("VSFILTER-3D-A");
  await page.getByRole("combobox", { name: "筛选设备接口" }).selectOption("");
  await page.getByRole("button", { name: "更多筛选", exact: true }).click();
  await page
    .getByRole("combobox", { name: "筛选编码器支持" })
    .selectOption("missing");
  await page.getByRole("button", { name: "完成", exact: true }).click();
  await expect(page.locator(".model-link")).toHaveCount(1);
  await expect(page.locator(".model-link")).toContainText("VSFILTER-3D-C");
  await page.getByRole("tab", { name: "面阵相机", exact: true }).click();
  await expect(page.getByRole("combobox", { name: "筛选厂家" })).toHaveValue(
    brand,
  );
  await expect(
    page.getByRole("spinbutton", { name: "最高帧率下限" }),
  ).toHaveValue("60");
  await expect(page.locator(".pagination")).toContainText("1–60 / 61");
});

for (const modal of [false, true]) {
  for (const viewport of [
    { width: 1520, height: 960 },
    { width: 1000, height: 720 },
  ]) {
    test(`${modal ? "选设备弹窗" : "资料库"} ${viewport.width} 下高级筛选、键盘与滚动`, async ({
      page,
    }) => {
      await page.setViewportSize(viewport);
      await open(page, modal);
      await cameraFilters(page);
      const more = page.getByRole("button", { name: "更多筛选", exact: true });
      await more.click();
      const panel = page.getByRole("region", {
        name: "更多规格筛选",
        exact: true,
      });
      await expect(
        panel.getByRole("button", { name: "完成", exact: true }),
      ).toBeInViewport({ ratio: 1 });
      await page
        .getByRole("combobox", { name: "筛选快门类型" })
        .selectOption("value:global");
      const accessibility = await new AxeBuilder({ page })
        .withTags(["wcag2a", "wcag2aa"])
        .analyze();
      expect(
        accessibility.violations.map((v) => ({
          id: v.id,
          nodes: v.nodes.map((n) => n.target),
        })),
      ).toEqual([]);
      await page.keyboard.press("Escape");
      await expect(panel).not.toBeVisible();
      if (modal) await expect(page.getByRole("dialog")).toBeVisible();
      await expect(more).toBeFocused();
      const list = page.getByRole("region", { name: "硬件设备列表" });
      await expect(page.locator(".hardware-table tbody tr")).toHaveCount(60);
      await list.hover();
      await page.mouse.wheel(0, 10000);
      await expect(
        page.locator(".hardware-table tbody tr").last(),
      ).toBeInViewport({ ratio: 1 });
      await expect(
        page.getByRole("button", { name: "下一页", exact: true }),
      ).toBeInViewport({ ratio: 1 });
      await page.locator(".model-link").last().click();
      await expect(
        page.getByRole("button", { name: "加入当前方案", exact: true }),
      ).toBeInViewport({ ratio: 1 });
      const detail = page.getByRole("region", {
        name: "设备详细信息",
        exact: true,
      });
      await detail.hover();
      await page.mouse.wheel(0, 10000);
      await expect(
        page.locator(".specification-list > div").last(),
      ).toBeInViewport({ ratio: 1 });
      await expect(
        page.getByRole("button", { name: "加入当前方案", exact: true }),
      ).toBeInViewport({ ratio: 1 });
      expect(
        await page
          .locator(".library")
          .evaluate((el) => el.scrollWidth <= el.clientWidth),
      ).toBe(true);
      if (viewport.width === 1000)
        await page.screenshot({
          path: `.codex_tmp/rust-ui/规格筛选-${modal ? "弹窗" : "页面"}-窄窗口.png`,
        });
      if (modal) {
        await page
          .getByRole("button", { name: "加入当前方案", exact: true })
          .click();
        await expect(page.getByRole("dialog")).not.toBeVisible();
        await expect(page.locator(".equipment-card").first()).toContainText(
          "VSFILTER-CAM-060",
        );
      }
    });
  }
}
