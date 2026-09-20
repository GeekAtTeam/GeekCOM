import { test, expect } from "@playwright/test";
test("browser preview exposes layout without pretending to connect hardware", async ({
  page,
}) => {
  const errors: string[] = [];
  page.on("pageerror", (e) => errors.push(e.message));
  await page.goto("/");
  await expect(page.getByText("浏览器预览", { exact: false })).toBeVisible();
  await expect(page.getByRole("button", { name: "连接串口" })).toBeDisabled();
  await page.getByRole("button", { name: "HEX / ASCII", exact: true }).click();
  await page.getByLabel("外观").selectOption("dark");
  await expect(page.locator("html")).toHaveAttribute("data-theme", "dark");
  await page.getByLabel("切换配置区域").click();
  await expect(page.locator(".sidebar")).toHaveCount(0);
  await page.reload();
  await expect(page.locator(".sidebar")).toHaveCount(0);
  await expect(page.locator("html")).toHaveAttribute("data-theme", "dark");
  await page.getByLabel("切换配置区域").click();
  await page.getByRole("button", { name: "串口终端", exact: true }).click();
  await expect(page.locator(".terminal-pane")).toBeVisible();
  await expect(page.getByText("本地回显", { exact: true })).toBeVisible();
  await page.getByRole("button", { name: "串口调试", exact: true }).click();
  await page.screenshot({ path: "test-results/workbench-dark.png" });
  expect(errors).toEqual([]);
});
test("minimum window size keeps primary controls in the viewport", async ({
  page,
}) => {
  await page.setViewportSize({ width: 900, height: 640 });
  await page.goto("/");
  await page.getByLabel("外观").selectOption("light");
  for (const selector of [".primary", ".statusbar", ".connection-toolbar"]) {
    const box = await page.locator(selector).first().boundingBox();
    expect(box).not.toBeNull();
    expect(box!.x).toBeGreaterThanOrEqual(0);
    expect(box!.x + box!.width).toBeLessThanOrEqual(901);
    expect(box!.y + box!.height).toBeLessThanOrEqual(641);
  }
  await page.screenshot({ path: "test-results/workbench-light-900.png" });
  await page.getByRole("button", { name: "帮助", exact: true }).first().click();
  await expect(page.getByRole("dialog")).toBeVisible();
  await page.getByRole("button", { name: "知道了" }).click();
  await expect(page.getByRole("dialog")).toHaveCount(0);
});
