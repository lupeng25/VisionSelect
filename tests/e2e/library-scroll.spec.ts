import { test, expect } from "@playwright/test";

// 验证真实鼠标滚动和控件可见性，避免自动滚入视野掩盖父容器裁切。
for (const viewport of [
  { width: 1520, height: 960 },
  { width: 1280, height: 800 },
  { width: 1000, height: 720 },
]) {
  for (const entry of ["资料库页面", "选设备弹窗"]) {
    test(
      entry +
        "在 " +
        viewport.width +
        " × " +
        viewport.height +
        " 下完整滚动和翻页",
      async ({ page }) => {
        await page.setViewportSize(viewport);
        await page.goto("/");
        await expect(
          page.getByRole("heading", { name: "方案工作台" }),
        ).toBeVisible();
        if (entry === "资料库页面") {
          await page
            .getByRole("button", { name: "硬件资料库", exact: true })
            .click();
        } else {
          await page
            .getByRole("button", { name: "选择面阵相机", exact: true })
            .click();
        }
        const library = page.locator(".library");
        const rows = library.locator(".hardware-table tbody tr");
        const list = library.getByRole("region", { name: "硬件设备列表" });
        const next = library.getByRole("button", {
          name: "下一页",
          exact: true,
        });
        await expect(rows).toHaveCount(60);
        await expect(next).toBeInViewport({ ratio: 1 });
        expect(
          await list.evaluate((el) => el.scrollHeight > el.clientHeight),
        ).toBeTruthy();

        await list.hover();
        await page.mouse.wheel(0, 10000);
        await expect
          .poll(() => list.evaluate((el) => el.scrollTop))
          .toBeGreaterThan(0);
        await expect(rows.last()).toBeInViewport({ ratio: 1 });
        await expect(next).toBeInViewport({ ratio: 1 });
        await rows.last().getByRole("button").click();
        await expect(
          library.getByRole("button", { name: "加入当前方案" }),
        ).toBeInViewport({ ratio: 1 });
        const previousLast = await rows.last().innerText();

        await next.click();
        await expect(library.locator(".pagination")).toContainText("61–120");
        await expect(rows).toHaveCount(60);
        await expect(rows.first()).toBeInViewport({ ratio: 1 });
        expect(await rows.first().innerText()).not.toBe(previousLast);
        await expect(next).toBeInViewport({ ratio: 1 });
        await library
          .getByRole("button", { name: "上一页", exact: true })
          .click();
        await expect(library.locator(".pagination")).toContainText("1–60");
        await expect(rows.first()).toBeInViewport({ ratio: 1 });

        if (viewport.width === 1280) {
          await page.screenshot({
            path: ".codex_tmp/rust-ui/硬件列表修复-" + entry + ".png",
          });
        }
      },
    );
  }
}
