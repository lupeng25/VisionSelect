import { test, expect, type Page } from "@playwright/test";
import AxeBuilder from "@axe-core/playwright";

const themeKey = "visionselect.appearance.theme.v1";
const skins = [
  { id: "graphite", name: "石墨", scheme: "dark" },
  { id: "porcelain", name: "瓷白", scheme: "light" },
  { id: "sand", name: "暖砂", scheme: "light" },
  { id: "sage", name: "雾青", scheme: "light" },
];

async function ready(page: Page) {
  await page.goto("/");
  await expect(page.getByRole("heading", { name: "方案工作台" })).toBeVisible();
  await expect(page.locator(".live-indicator")).toHaveText("即时联动");
}
async function chooseTheme(page: Page, name: string) {
  await page.getByRole("button", { name: "外观皮肤", exact: true }).click();
  // 点击整张卡片，确保视觉隐藏的原生单选框仍可用。
  await page
    .locator(".theme-card")
    .filter({ has: page.getByRole("radio", { name, exact: true }) })
    .click();
  await expect(page.getByRole("radio", { name, exact: true })).toBeChecked();
}
async function accessible(page: Page) {
  // 淡入期间的透明度不是最终配色，完成有限动画后检查实际可读性。
  await page.evaluate(async () => {
    const animations = document
      .getAnimations()
      .filter(
        (animation) => animation.effect?.getTiming().iterations !== Infinity,
      );
    await Promise.allSettled(animations.map((animation) => animation.finished));
  });
  const audit = await new AxeBuilder({ page })
    .withTags(["wcag2a", "wcag2aa"])
    .analyze();
  expect(audit.violations).toEqual([]);
}
async function noHorizontalOverflow(page: Page) {
  expect(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= innerWidth,
    ),
  ).toBe(true);
}

test.beforeEach(async ({ page }) => {
  page.on("dialog", (dialog) => void dialog.accept());
});

for (const skin of skins) {
  test(`${skin.name}皮肤覆盖主要页面和弹窗，重载后恢复`, async ({
    page,
    request,
  }) => {
    test.setTimeout(60000);
    const errors: string[] = [];
    page.on("pageerror", (error) => errors.push(error.message));
    expect(
      (await (await request.get("http://127.0.0.1:4318/api/health")).json())
        .engine,
    ).toBe("rust");
    await ready(page);
    await chooseTheme(page, skin.name);
    await expect(page.locator("html")).toHaveAttribute("data-theme", skin.id);
    await expect(page.locator("html")).toHaveCSS("color-scheme", skin.scheme);
    await expect(page.getByRole("status")).toContainText(
      "当前使用：" + skin.name,
    );
    await accessible(page);
    await page.getByRole("button", { name: "完成", exact: true }).click();
    await expect(
      page.getByRole("button", { name: "外观皮肤", exact: true }),
    ).toBeFocused();
    await accessible(page);

    await page
      .getByRole("button", { name: "选择面阵相机", exact: true })
      .click();
    await expect(page.locator(".model-link")).toHaveCount(60);
    await page.locator(".model-link").first().click();
    await expect(
      page.getByRole("button", { name: "加入当前方案", exact: true }),
    ).toBeEnabled();
    await accessible(page);
    await page.keyboard.press("Escape");

    await page.getByRole("button", { name: "硬件资料库", exact: true }).click();
    await expect(page.locator(".model-link")).toHaveCount(60);
    await page.locator(".model-link").first().click();
    await page.getByRole("button", { name: "更多筛选", exact: true }).click();
    await expect(
      page.getByRole("combobox", { name: "筛选快门类型" }),
    ).toBeVisible();
    await accessible(page);
    await page.keyboard.press("Escape");
    await noHorizontalOverflow(page);

    await page
      .getByRole("button", { name: "2D 选型计算", exact: true })
      .click();
    await expect(page.locator(".calculation-results")).toBeVisible();
    await expect(
      page.locator(".calculation-results .live-indicator"),
    ).toHaveText("已更新");
    await accessible(page);
    await page.getByRole("button", { name: "我的方案" }).click();
    await expect(page.getByRole("heading", { name: "我的方案" })).toBeVisible();
    await accessible(page);
    await page.reload();
    await expect(
      page.getByRole("heading", { name: "方案工作台" }),
    ).toBeVisible();
    await expect(page.locator("html")).toHaveAttribute("data-theme", skin.id);
    await expect(
      page.getByRole("button", { name: "外观皮肤", exact: true }),
    ).toHaveAttribute("title", "外观皮肤 · " + skin.name);
    expect(errors).toEqual([]);
  });
}

test("切换皮肤保留当前方案输入和正在使用的筛选", async ({ page }) => {
  await ready(page);
  await page
    .getByRole("textbox", { name: "方案名称" })
    .fill("皮肤切换中的草稿");
  await page
    .getByRole("spinbutton", { name: "工件宽度", exact: true })
    .fill("123");
  await page.getByRole("button", { name: "硬件资料库", exact: true }).click();
  await page
    .getByRole("combobox", { name: "筛选相机接口" })
    .selectOption("value:usb3");
  await page.getByRole("spinbutton", { name: "最高帧率下限" }).fill("100");
  await expect(page.locator(".library-results")).toHaveAttribute(
    "aria-busy",
    "false",
  );
  await page.locator(".model-link").first().click();
  const chosen =
    (await page.locator(".hardware-table tr.selected").textContent()) ?? "";
  const total = (await page.locator(".pagination").textContent()) ?? "";
  await chooseTheme(page, "暖砂");
  await page.keyboard.press("Escape");
  await expect(
    page.getByRole("combobox", { name: "筛选相机接口" }),
  ).toHaveValue("value:usb3");
  await expect(
    page.getByRole("spinbutton", { name: "最高帧率下限" }),
  ).toHaveValue("100");
  await expect(page.locator(".hardware-table tr.selected")).toHaveText(chosen);
  await expect(page.locator(".pagination")).toHaveText(total);
  await page.getByRole("button", { name: "方案工作台", exact: true }).click();
  await expect(page.getByRole("textbox", { name: "方案名称" })).toHaveValue(
    "皮肤切换中的草稿",
  );
  await expect(
    page.getByRole("spinbutton", { name: "工件宽度", exact: true }),
  ).toHaveValue("123");
  await expect(page.locator(".save-state")).toHaveText("未保存");
});

test("窄窗口可用键盘切换皮肤，关闭后焦点回到入口", async ({ page }) => {
  await page.setViewportSize({ width: 1000, height: 720 });
  await ready(page);
  const trigger = page.getByRole("button", { name: "外观皮肤", exact: true });
  await expect(trigger).toBeInViewport({ ratio: 1 });
  await trigger.focus();
  await page.keyboard.press("Enter");
  await page.getByRole("radio", { name: "石墨", exact: true }).focus();
  await page.keyboard.press("ArrowRight");
  await expect(
    page.getByRole("radio", { name: "瓷白", exact: true }),
  ).toBeChecked();
  await page.keyboard.press("ArrowRight");
  await page.keyboard.press("ArrowRight");
  await expect(page.locator("html")).toHaveAttribute("data-theme", "sage");
  await expect(
    page.getByRole("button", { name: "完成", exact: true }),
  ).toBeInViewport({ ratio: 1 });
  await noHorizontalOverflow(page);
  await accessible(page);
  await page.keyboard.press("Escape");
  await expect(trigger).toBeFocused();
  await expect(page.getByRole("dialog")).toHaveCount(0);
});

test("无效的旧外观偏好回退到默认石墨", async ({ page }) => {
  await page.addInitScript(
    (key) => localStorage.setItem(key, "removed-theme"),
    themeKey,
  );
  await ready(page);
  await expect(page.locator("html")).toHaveAttribute("data-theme", "graphite");
  await chooseTheme(page, "石墨");
  await expect(
    page.getByRole("radio", { name: "石墨", exact: true }),
  ).toBeChecked();
});

test("外观存储不可用时仍可切换并说明无法保存", async ({ page }) => {
  await page.addInitScript((key) => {
    const getItem = Storage.prototype.getItem;
    const setItem = Storage.prototype.setItem;
    Storage.prototype.getItem = function (name) {
      if (name === key) throw new DOMException("blocked", "SecurityError");
      return getItem.call(this, name);
    };
    Storage.prototype.setItem = function (name, value) {
      if (name === key) throw new DOMException("blocked", "QuotaExceededError");
      return setItem.call(this, name, value);
    };
  }, themeKey);
  await ready(page);
  await chooseTheme(page, "瓷白");
  await expect(page.locator("html")).toHaveAttribute("data-theme", "porcelain");
  await expect(page.getByRole("alert")).toContainText("无法保存偏好");
  await page.getByRole("button", { name: "完成", exact: true }).click();
  await expect(page.getByRole("heading", { name: "方案工作台" })).toBeVisible();
});

test("减少动态效果时仍可切换分组和皮肤，浮层定位与焦点正常", async ({
  page,
}) => {
  await page.emulateMedia({ reducedMotion: "reduce" });
  await page.setViewportSize({ width: 1000, height: 720 });
  await ready(page);
  await page
    .getByRole("spinbutton", { name: "工件宽度", exact: true })
    .fill("123");
  await page.getByRole("button", { name: "采集与校准", exact: true }).click();
  await page.getByRole("button", { name: "成像目标", exact: true }).click();
  await expect(
    page.getByRole("spinbutton", { name: "工件宽度", exact: true }),
  ).toHaveValue("123");
  await expect(page.locator(".parameter-body")).toHaveCSS(
    "animation-name",
    "none",
  );
  await chooseTheme(page, "雾青");
  await expect(page.getByRole("dialog")).toHaveCSS("animation-name", "none");
  await expect(
    page.getByRole("button", { name: "完成", exact: true }),
  ).toHaveCSS("transition-duration", "0s");
  await page.keyboard.press("Escape");
  await expect(
    page.getByRole("button", { name: "外观皮肤", exact: true }),
  ).toBeFocused();
  await page.getByRole("button", { name: "选择面阵相机", exact: true }).click();
  await page.getByRole("button", { name: "更多筛选", exact: true }).click();
  await expect(
    page.getByRole("combobox", { name: "筛选快门类型" }),
  ).toBeInViewport({ ratio: 1 });
  await expect(page.locator(".catalog-filter-panel")).toHaveCSS(
    "animation-name",
    "none",
  );
  await expect(page.locator(".modal")).toHaveCSS("transform", "none");
  await page.keyboard.press("Escape");
  await expect(
    page.getByRole("dialog", { name: "选择面阵相机" }),
  ).toBeVisible();
  await page.keyboard.press("Escape");
  await expect(
    page.getByRole("button", { name: "选择面阵相机", exact: true }),
  ).toBeFocused();
});
