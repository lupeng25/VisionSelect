import { test, expect } from "@playwright/test";

test("官网核对结果进入真实资料库详情与筛选视图", async ({ page }) => {
  await page.goto("/");
  await page.getByRole("button", { name: "硬件资料库", exact: true }).click();
  const search = page.getByRole("textbox", { name: "搜索硬件" });
  const specs = page.getByLabel("设备详细规格");

  await search.fill("MER3-515-131X2M");
  await page
    .getByRole("button", { name: "MER3-515-131X2M Daheng Imaging" })
    .click();
  await expect(specs).toContainText("信噪比 / dB");
  await expect(specs).toContainText("39.8");
  await expect(specs).not.toContainText("动态范围 / dB");
  await expect(specs).toContainText("历史目录带宽 / MB/s（口径待核）");

  await page.getByRole("tab", { name: "成像镜头" }).click();
  await search.fill("APO-DTJCA25M-2C-110-G");
  await page
    .getByRole("button", { name: "APO-DTJCA25M-2C-110-G COOLENS" })
    .click();
  await expect(
    specs.locator("div").filter({ hasText: "远心度 / °" }),
  ).toContainText("0.06");
  await expect(specs).toContainText("景深 0.17–0.44 mm");

  await page.getByRole("tab", { name: "3D 相机" }).click();
  await search.fill("Gocator 2320");
  await page.getByRole("button", { name: "Gocator 2320 LMI" }).click();
  await expect(specs).toContainText("红色激光（标准配置）");
  await expect(specs).toContainText("激光波长 / nm");
  await expect(specs).toContainText("660");
  await page.screenshot({
    path: ".codex_tmp/hardware-data-corrections/官网校正后的资料库.png",
    fullPage: true,
  });

  await page.setViewportSize({ width: 1000, height: 720 });
  await search.fill("Gocator 2390");
  await page.getByRole("button", { name: "Gocator 2390 LMI" }).click();
  await expect(specs).toContainText("未能官网确认");
  await expect(specs).not.toContainText("激光波长 / nm");
  await expect(specs).toContainText("原 405 nm 蓝光声明没有可核实依据");
  expect(
    await page
      .locator(".hardware-detail-scroll")
      .evaluate((element) => element.scrollWidth <= element.clientWidth),
  ).toBeTruthy();
  const correction = specs
    .locator(":scope > div")
    .filter({ hasText: "激光规格核对说明" });
  await correction.scrollIntoViewIfNeeded();
  await expect(correction).toBeInViewport({ ratio: 0.1 });
  await page.screenshot({
    path: ".codex_tmp/hardware-data-corrections/待核型号窄窗口.png",
  });
});
