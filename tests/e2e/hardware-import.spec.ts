import { test, expect } from "@playwright/test";
import AxeBuilder from "@axe-core/playwright";
import { resolve } from "node:path";
import { readFile } from "node:fs/promises";

test("Excel 多工作表、表头行及中文字段对应", async ({ page }) => {
  await page.goto("/");
  await page.getByRole("button", { name: "硬件资料库", exact: true }).click();
  let chooserCount = 0;
  page.on("filechooser", () => chooserCount++);
  await page.getByRole("button", { name: "导入硬件", exact: true }).click();
  const entry = page.getByRole("dialog", { name: "导入硬件", exact: true });
  await expect(
    entry.getByRole("heading", { name: "拖入硬件参数文件" }),
  ).toBeVisible();
  expect(chooserCount).toBe(0);
  const downloading = page.waitForEvent("download");
  await entry.getByRole("button", { name: "下载模板", exact: true }).click();
  const downloaded = await downloading;
  expect(downloaded.suggestedFilename()).toBe("面阵相机导入模板.csv");
  expect(await readFile((await downloaded.path())!, "utf8")).toContain(
    "model,manufacturer,resolution_x",
  );
  await page.setViewportSize({ width: 1000, height: 720 });
  for (const theme of ["graphite", "porcelain"]) {
    await page.evaluate(
      (value) => document.documentElement.setAttribute("data-theme", value),
      theme,
    );
    await entry.screenshot({
      path: `.codex_tmp/import-ui/导入入口-${theme}.png`,
      animations: "disabled",
    });
  }
  expect(
    (await new AxeBuilder({ page }).include(".import-entry").analyze())
      .violations,
  ).toEqual([]);
  const choosing = page.waitForEvent("filechooser");
  await entry.getByRole("button", { name: "选择文件", exact: true }).click();
  await (
    await choosing
  ).setFiles(resolve("crates/vision-core/tests/fixtures/硬件导入样例.xlsx"));
  expect(chooserCount).toBe(1);
  const wizard = page.getByRole("dialog", { name: "导入硬件参数表" });
  await expect(wizard.getByLabel("工作表")).toHaveValue("说明");
  await wizard.getByLabel("工作表").selectOption("相机参数");
  await wizard.getByLabel("表头所在行").fill("2");
  await expect(wizard.getByLabel("第 1 列字段对应")).toHaveValue("model");
  await expect(wizard.getByLabel("第 5 列字段对应")).toHaveValue(
    "pixel_size_um",
  );
  await expect(wizard).toContainText("2 条记录");
  await page.setViewportSize({ width: 1000, height: 720 });
  await page.evaluate(() =>
    document.documentElement.setAttribute("data-theme", "porcelain"),
  );
  await wizard.screenshot({
    path: ".codex_tmp/import-ui/Excel字段对应.png",
    animations: "disabled",
  });
  expect(
    await wizard.evaluate((el) => el.scrollWidth <= el.clientWidth),
  ).toBeTruthy();
  expect(
    (await new AxeBuilder({ page }).include(".hardware-import").analyze())
      .violations,
  ).toEqual([]);
  await wizard.getByRole("button", { name: "检查导入差异" }).click();
  await expect(wizard.getByRole("alert")).toContainText("第 4 行");
  await expect(wizard.getByRole("button", { name: /确认导入/ })).toBeDisabled();
  await wizard.screenshot({
    path: ".codex_tmp/import-ui/Excel错误行.png",
    animations: "disabled",
  });
  await page.keyboard.press("Escape");
  await expect(wizard).toHaveCount(0);
});

test("CSV 手动对应、错误行跳过、差异预览和安全补充参数", async ({
  page,
  request,
}) => {
  const model = "VSTABLE-" + Date.now();
  async function api(operation: string, data = {}) {
    const r = await request.post("http://127.0.0.1:4318/api/" + operation, {
      data,
    });
    expect(r.ok(), await r.text()).toBeTruthy();
    return r.json();
  }
  const original = await api("import_preview", {
    kind: "camera",
    format: "csv",
    content: `model,manufacturer,resolution_x,notes\n${model},表格测试厂家,1024,原有备注`,
  });
  await api("import_commit", { token: original.token });
  await page.goto("/");
  await page.getByRole("button", { name: "硬件资料库", exact: true }).click();
  await page.getByLabel("导入硬件文件").setInputFiles({
    name: "中文参数表.csv",
    mimeType: "text/csv",
    buffer: Buffer.from(
      `产品型号,厂商名称,水平像素(px),像元尺寸(μm),备注\n${model},表格测试厂家,2048,3.45,\n错误型号,表格测试厂家,-1,3.45,`,
    ),
  });
  const wizard = page.getByRole("dialog", { name: "导入硬件参数表" });
  await expect(wizard.getByLabel("第 2 列字段对应")).toHaveValue("");
  await expect(
    wizard.getByRole("button", { name: "检查导入差异" }),
  ).toBeDisabled();
  await wizard.getByLabel("第 2 列字段对应").selectOption("manufacturer");
  await wizard.getByRole("button", { name: "检查导入差异" }).click();
  await expect(wizard.getByRole("alert")).toContainText("第 3 行");
  await expect(
    wizard.getByRole("button", { name: "确认导入 1 条" }),
  ).toBeDisabled();
  await wizard.getByRole("button", { name: "返回调整" }).click();
  await wizard.getByLabel("跳过错误行，仅导入通过校验的记录").check();
  await wizard.getByRole("button", { name: "检查导入差异" }).click();
  await wizard.locator("summary").click();
  await expect(wizard.locator(".import-diff")).toContainText("像元尺寸 / μm");
  await expect(wizard.locator(".import-diff")).toContainText("3.45");
  await wizard.screenshot({
    path: ".codex_tmp/import-ui/CSV差异预览.png",
    animations: "disabled",
  });
  await wizard.getByRole("button", { name: "确认导入 1 条" }).click();
  await expect(wizard).toHaveCount(0);
  await expect(
    page.getByText("已导入 1 条设备。原库已自动备份。", { exact: true }),
  ).toBeVisible();
  const item = (await api("catalog", { kind: "camera", search: model }))
    .items[0];
  expect(item.specs.resolution_x).toBe("1024");
  expect(item.specs.pixel_size_um).toBe(3.45);
  expect(item.specs.notes).toBe("原有备注");
});

test("GBK CSV 自动识别及重复字段阻止预览", async ({ page }) => {
  await page.goto("/");
  await page.getByRole("button", { name: "硬件资料库", exact: true }).click();
  await page.getByLabel("导入硬件文件").setInputFiles({
    name: "GBK参数.csv",
    mimeType: "text/csv",
    buffer: Buffer.from(
      "D0CDBAC52CB3A7BCD22CCBAEC6BDCFF1CBD80AB1E0C2EBD1E9D6A42CB2E2CAD4B3A7BCD22C31303234",
      "hex",
    ),
  });
  const wizard = page.getByRole("dialog", { name: "导入硬件参数表" });
  await expect(wizard).toContainText("GB18030");
  await expect(wizard.getByLabel("第 1 列字段对应")).toHaveValue("model");
  await expect(wizard.getByLabel("第 2 列字段对应")).toHaveValue(
    "manufacturer",
  );
  await wizard.getByLabel("第 2 列字段对应").selectOption("model");
  await expect(
    wizard.getByRole("button", { name: "检查导入差异" }),
  ).toBeDisabled();
});

test("导入入口支持拖入文件、切换分类和键盘关闭", async ({ page }) => {
  await page.goto("/");
  await page.getByRole("button", { name: "硬件资料库", exact: true }).click();
  const button = page.getByRole("button", { name: "导入硬件", exact: true });
  await button.click();
  const entry = page.getByRole("dialog", { name: "导入硬件", exact: true });
  await page.keyboard.press("Escape");
  await expect(entry).toHaveCount(0);
  await expect(button).toBeFocused();
  await button.click();
  await entry.getByLabel("导入到分类").selectOption("lens");
  const file = await page.evaluateHandle(() => {
    const data = new DataTransfer();
    data.items.add(
      new File(["型号,厂家,焦距(mm)\n拖入测试,测试厂家,25"], "镜头.csv", {
        type: "text/csv",
      }),
    );
    return data;
  });
  await entry
    .locator(".import-entry")
    .dispatchEvent("drop", { dataTransfer: file });
  await file.dispose();
  const wizard = page.getByRole("dialog", {
    name: "导入硬件参数表",
    exact: true,
  });
  await expect(wizard.getByLabel("导入硬件分类")).toHaveValue("lens");
  await expect(wizard.getByLabel("第 3 列字段对应")).toHaveValue(
    "focal_length_mm",
  );
});
