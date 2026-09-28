import { test, expect } from "@playwright/test";

test("国产镜头进入真实 Rust 资料库且展示原厂规格", async ({ page }) => {
  await page.goto("/");
  await page.getByRole("button", { name: "硬件资料库", exact: true }).click();
  await page.getByRole("tab", { name: "成像镜头" }).click();

  const search = page.getByRole("textbox", { name: "搜索硬件" });
  const specs = page.getByLabel("设备详细规格");
  await search.fill("MT007-300-150-11-C");
  await page.getByRole("button", { name: "MT007-300-150-11-C 慕藤光" }).click();
  await expect(specs).toContainText("适配靶面对角线 / mm");
  await expect(specs).toContainText("双远心镜头");
  await expect(specs).not.toContainText("BiTelecentric");
  await expect(specs).toContainText("原厂工作距离原文");
  await expect(specs).toContainText("双侧远心镜头系列");
  await page.screenshot({
    path: ".codex_tmp/国产镜头资料库.png",
    fullPage: true,
  });
  await page.setViewportSize({ width: 1000, height: 720 });
  expect(
    await page
      .locator(".hardware-detail-scroll")
      .evaluate((element) => element.scrollWidth <= element.clientWidth),
  ).toBeTruthy();
  await page.screenshot({ path: ".codex_tmp/国产镜头资料库窄窗口.png" });

  await search.fill("XF-PTL03708-C");
  await page.getByRole("button", { name: "XF-PTL03708-C 灿锐光学" }).click();
  await expect(specs).toContainText("原厂产品分类");
  await expect(specs).toContainText("0.218");

  await search.fill("DXT2-WS50T");
  await page
    .getByRole("button", { name: "DXT2-WS50T 桂林桂光仪器有限公司" })
    .click();
  await expect(specs).toContainText("定倍率镜头");
  await expect(specs).toContainText("原厂条件景深 / mm");
  await expect(specs).not.toContainText("目录景深 / mm");
  expect(
    await page
      .locator(".hardware-detail-scroll")
      .evaluate((element) => element.scrollWidth <= element.clientWidth),
  ).toBeTruthy();
  await page.screenshot({ path: ".codex_tmp/桂光镜头资料库窄窗口.png" });
});
