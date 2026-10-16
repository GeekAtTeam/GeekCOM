import { test, expect, type Page } from "@playwright/test";
import {
  emptyStatus,
  type Batch,
  type Port,
  type SerialEvent,
  type Status,
} from "../src/model";

type Harness = {
  ports: Port[];
  error: string | null;
  connectError: string | null;
  status: Status;
  events: SerialEvent[];
  savedText: string;
  holdPoll: boolean;
  pollCount: number;
  pending?: { result: Batch; resolve: (batch: Batch) => void };
};
declare global {
  interface Window {
    serialTest: Harness;
  }
}
async function setup(page: Page) {
  await page.addInitScript((initial) => {
    localStorage.setItem(
      "connection",
      JSON.stringify({
        port: "/dev/expired",
        baud: 115200,
        dataBits: 8,
        parity: "none",
        stopBits: "1",
      }),
    );
    localStorage.setItem("manualPort", "false");
    const state: Harness = {
      ports: [{ name: "/dev/ttyACM0", description: "IMU" }],
      error: null,
      connectError: null,
      status: structuredClone(initial),
      events: [],
      savedText: "",
      holdPoll: false,
      pollCount: 0,
    };
    window.serialTest = state;
    Object.assign(window, {
      isTauri: true,
      __TAURI_INTERNALS__: {
        invoke: async (
          command: string,
          args: {
            config?: { port: string };
            payload?: { text: string };
            text?: string;
          } = {},
        ) => {
          switch (command) {
            case "list_ports":
              if (state.error) throw state.error;
              return structuredClone(state.ports);
            case "save_receive":
              state.savedText = args.text!;
              return true;
            case "preview":
              return [...new TextEncoder().encode(args.payload?.text || "")];
            case "poll": {
              state.pollCount++;
              const result = {
                status: structuredClone(state.status),
                events: state.events.splice(0),
              };
              if (state.holdPoll) {
                state.holdPoll = false;
                return new Promise<Batch>((resolve) => {
                  state.pending = { result, resolve };
                });
              }
              return result;
            }
            case "connect_serial":
              if (state.connectError) {
                state.status.error = state.connectError;
                throw state.connectError;
              }
              state.status = {
                ...initial,
                session: state.status.session + 1,
                connected: true,
                port: args.config!.port,
              };
              return structuredClone(state.status);
            case "disconnect_serial":
              state.status.connected = false;
              state.events = [];
              return structuredClone(state.status);
            default:
              throw new Error(`Unexpected command: ${command}`);
          }
        },
      },
    });
  }, emptyStatus);
  await page.goto("/");
  await expect(page.getByLabel("串口设备", { exact: true })).toHaveValue(
    "/dev/ttyACM0",
  );
  await expect(page.getByLabel("刷新串口", { exact: true })).toBeEnabled();
}

async function receive(page: Page, id: number, text: string, session?: number) {
  await page.evaluate(
    ({ id, text, session }) => {
      const s = window.serialTest;
      const data = [...new TextEncoder().encode(text)];
      s.status.rx += data.length;
      s.events.push({
        id,
        session: session ?? s.status.session,
        timestamp: Date.now(),
        direction: "RX",
        data,
      });
    },
    { id, text, session },
  );
}

test("editable port picker stays compact, lists names only and preserves typed paths on refresh", async ({
  page,
}) => {
  await setup(page);
  const input = page.getByRole("combobox", { name: "串口设备", exact: true });
  const toggle = page.getByRole("button", {
    name: "展开串口列表",
    exact: true,
  });
  const refresh = page.getByRole("button", { name: "刷新串口", exact: true });
  const original = await input.boundingBox();
  await toggle.click();
  await expect(page.getByRole("listbox").getByRole("option")).toHaveText([
    "/dev/ttyACM0",
  ]);
  await expect(page.getByRole("listbox")).not.toContainText("IMU");
  await page.getByRole("option", { name: "/dev/ttyACM0", exact: true }).click();
  await expect(input).toHaveValue("/dev/ttyACM0");
  await expect(page.getByRole("listbox")).toHaveCount(0);
  await expect(page.getByText(/已发现.*个串口/)).toHaveCount(0);
  await page.evaluate(() => {
    window.serialTest.ports = [
      { name: "/dev/ttyUSB0", description: "USB adapter" },
    ];
  });
  await refresh.click();
  await expect(input).toHaveValue("/dev/ttyUSB0");
  await page.evaluate(() => {
    window.serialTest.error = "enumeration failed";
  });
  await refresh.click();
  await expect(page.getByRole("alert")).toHaveText(
    "刷新失败：enumeration failed",
  );
  await expect(refresh).toBeEnabled();
  await page.evaluate(() => {
    window.serialTest.error = null;
    window.serialTest.ports = [];
  });
  await refresh.click();
  await expect(input).toHaveValue("");
  await expect(page.getByRole("alert")).toHaveCount(0);
  await expect(
    page.getByRole("button", { name: "连接串口", exact: true }),
  ).toBeDisabled();
  await toggle.click();
  await expect(page.getByRole("listbox")).toContainText("未发现串口");
  await input.fill("/dev/pts/42");
  await refresh.click();
  await expect(input).toHaveValue("/dev/pts/42");
  await expect(page.getByLabel("手动串口路径")).toHaveCount(0);
  await input.fill(
    "/dev/serial/by-id/usb-very-long-device-name-that-must-not-resize-the-field",
  );
  const long = await input.boundingBox();
  expect(long!.width).toBe(original!.width);
  expect(long!.height).toBe(original!.height);
  await expect(
    page.getByRole("button", { name: "连接串口", exact: true }),
  ).toBeEnabled();
});

test("editable port picker supports keyboard selection, dismissal and direct typing", async ({
  page,
}) => {
  await setup(page);
  const input = page.getByRole("combobox", { name: "串口设备", exact: true });
  await page.evaluate(() => {
    window.serialTest.ports = [
      { name: "/dev/ttyACM0", description: "IMU" },
      { name: "COM12", description: "Windows port" },
    ];
  });
  await page.getByRole("button", { name: "刷新串口", exact: true }).click();
  await input.press("ArrowDown");
  await expect(input).toHaveAttribute("aria-expanded", "true");
  await input.press("ArrowDown");
  await input.press("Enter");
  await expect(input).toHaveValue("COM12");
  await expect(input).toHaveAttribute("aria-expanded", "false");
  await input.fill("COM");
  await expect(page.getByRole("listbox").getByRole("option")).toHaveText([
    "COM12",
  ]);
  await input.press("Escape");
  await expect(input).toHaveValue("COM");
  await expect(page.getByRole("listbox")).toHaveCount(0);
  await input.fill("COM42");
  await input.press("Tab");
  await expect(input).toHaveValue("COM42");
  await expect(page.getByRole("listbox")).toHaveCount(0);
  await page.getByRole("button", { name: "连接串口", exact: true }).click();
  await expect(input).toBeDisabled();
  await expect(
    page.getByRole("button", { name: "展开串口列表", exact: true }),
  ).toBeDisabled();
});

test("auto-scroll only operates while connected, keeps receiving while paused and resets on disconnect", async ({
  page,
}) => {
  await setup(page);
  await expect(
    page.getByRole("button", { name: "自动滚动", exact: true }),
  ).toBeDisabled();
  await page.getByRole("button", { name: "连接串口", exact: true }).click();
  await page.getByRole("button", { name: "暂停自动滚动", exact: true }).click();
  await expect(
    page.getByRole("button", { name: "恢复自动滚动", exact: true }),
  ).toHaveAttribute("aria-pressed", "true");
  await receive(page, 1, "IMU sample 1");
  await expect(page.locator(".log-row")).toHaveText(/IMU sample 1/);
  await page.getByRole("button", { name: "断开连接", exact: true }).click();
  await expect(
    page.getByRole("button", { name: "自动滚动", exact: true }),
  ).toBeDisabled();
  await expect(
    page.getByRole("button", { name: "自动滚动", exact: true }),
  ).toHaveAttribute("aria-pressed", "false");
  await expect(page.locator(".log-row")).toHaveCount(1);
  await page.getByRole("button", { name: "连接串口", exact: true }).click();
  await expect(
    page.getByRole("button", { name: "暂停自动滚动", exact: true }),
  ).toBeEnabled();
  await receive(page, 2, "old session tail", 1);
  await receive(page, 3, "IMU sample 2");
  await expect(page.locator(".log-row")).toHaveCount(2);
  await expect(page.locator(".log-surface")).not.toContainText(
    "old session tail",
  );
  await page.evaluate(() => {
    window.serialTest.status.connected = false;
    window.serialTest.status.error = "Device unplugged";
  });
  await expect(
    page.getByRole("button", { name: "自动滚动", exact: true }),
  ).toBeDisabled();
  await expect(
    page.getByRole("button", { name: "连接串口", exact: true }),
  ).toBeEnabled();
});

test("a poll response arriving after disconnect cannot append data or restore the old connected state", async ({
  page,
}) => {
  await setup(page);
  await page.getByRole("button", { name: "连接串口", exact: true }).click();
  await expect(
    page.getByRole("button", { name: "断开连接", exact: true }),
  ).toBeEnabled();
  await receive(page, 1, "retained sample");
  await expect(page.locator(".log-row")).toHaveCount(1);
  await page.evaluate(() => {
    const s = window.serialTest;
    s.holdPoll = true;
    s.events.push({
      id: 2,
      session: s.status.session,
      timestamp: Date.now(),
      direction: "RX",
      data: [...new TextEncoder().encode("stale IMU sample")],
    });
  });
  await expect
    .poll(() => page.evaluate(() => !!window.serialTest.pending))
    .toBe(true);
  await page.getByRole("button", { name: "断开连接", exact: true }).click();
  await expect(
    page.getByRole("button", { name: "连接串口", exact: true }),
  ).toBeEnabled();
  const oldCount = await page.evaluate(() => {
    const s = window.serialTest;
    s.pending!.resolve(s.pending!.result);
    return s.pollCount;
  });
  await expect
    .poll(() => page.evaluate(() => window.serialTest.pollCount))
    .toBeGreaterThan(oldCount);
  await expect(page.locator(".log-row")).toHaveCount(1);
  await expect(page.locator(".log-surface")).not.toContainText(
    "stale IMU sample",
  );
  await expect(
    page.getByRole("button", { name: "连接串口", exact: true }),
  ).toBeEnabled();
  await expect(
    page.getByRole("button", { name: "自动滚动", exact: true }),
  ).toBeDisabled();
});

test("failed connection keeps the cause visible and permits editing and retry", async ({
  page,
}) => {
  await setup(page);
  await page.evaluate(() => {
    window.serialTest.connectError = "无法打开 /dev/ttyACM0: Permission denied";
  });
  await page.getByRole("button", { name: "连接串口", exact: true }).click();
  await expect(
    page.getByText("无法打开 /dev/ttyACM0: Permission denied", { exact: true }),
  ).toBeVisible();
  await expect(page.getByLabel("串口设备", { exact: true })).toBeEnabled();
  await expect(
    page.getByRole("button", { name: "自动滚动", exact: true }),
  ).toBeDisabled();
  await expect(
    page.getByRole("button", { name: "周期发送", exact: true }),
  ).toBeDisabled();
  await expect(
    page.getByRole("button", { name: "连接串口", exact: true }),
  ).toBeEnabled();
  await page.getByLabel("串口设备", { exact: true }).fill("/dev/ttyUSB0");
  await page.getByLabel("串口设备", { exact: true }).press("Escape");
  await page.evaluate(() => {
    window.serialTest.connectError = null;
  });
  await page.getByRole("button", { name: "连接串口", exact: true }).click();
  await expect(
    page.getByRole("button", { name: "断开连接", exact: true }),
  ).toBeEnabled();
  await expect(
    page.getByText("无法打开 /dev/ttyACM0: Permission denied", { exact: true }),
  ).toHaveCount(0);
  await expect(page.getByLabel("串口设备", { exact: true })).toHaveValue(
    "/dev/ttyUSB0",
  );
});

test("retained history keeps selection and raw views while paused and receiving", async ({
  page,
}) => {
  await setup(page);
  await page.getByRole("button", { name: "连接串口", exact: true }).click();
  await page.evaluate(() => {
    const s = window.serialTest;
    for (let id = 1; id <= 1000; id++) {
      const data =
        id === 1
          ? [65, 0, 255, 13, 10]
          : [...new TextEncoder().encode(`retained ${id}\n`)];
      s.status.rx += data.length;
      s.events.push({
        id,
        session: s.status.session,
        timestamp: Date.now(),
        direction: "RX",
        data,
      });
    }
  });
  await expect(page.locator(".log-column-head")).toContainText(
    "1000 条读取记录",
  );
  await page.getByRole("button", { name: "暂停自动滚动", exact: true }).click();
  await page.locator(".log-surface").evaluate((e) => {
    e.scrollTop = 0;
  });
  await page.locator('.log-row[data-event-id="1"]').click();
  await expect(page.locator(".byte-detail")).toContainText("41 00 FF 0D 0A");
  const scroll = await page
    .locator(".log-surface")
    .evaluate((e) => e.scrollTop);
  await receive(page, 1001, "arrived while paused\n");
  await expect(page.locator(".log-column-head")).toContainText(
    "1001 条读取记录",
  );
  await expect(page.locator('.log-row[data-event-id="1"]')).toHaveClass(
    /highlighted/,
  );
  expect(await page.locator(".log-surface").evaluate((e) => e.scrollTop)).toBe(
    scroll,
  );
  await page.getByRole("button", { name: "HEX / ASCII", exact: true }).click();
  const first = page.locator('.log-row[data-event-id="1"]');
  await expect(first.locator(".row-content")).toHaveText("41 00 FF 0D 0A");
  await expect(first.locator(".ascii-column")).toHaveText("A....");
  await page.getByRole("button", { name: "文本", exact: true }).click();
  await expect(first.locator(".row-content")).toContainText("A");
  await page.getByRole("button", { name: "恢复自动滚动", exact: true }).click();
  await expect
    .poll(() =>
      page
        .locator(".log-surface")
        .evaluate((e) => e.scrollHeight - e.scrollTop - e.clientHeight),
    )
    .toBeLessThan(3);
  await page.getByRole("button", { name: "断开连接", exact: true }).click();
  await expect(page.locator(".log-column-head")).toContainText(
    "1001 条读取记录",
  );
  await page.getByLabel("清空接收区", { exact: true }).click();
  await expect(page.locator(".log-row")).toHaveCount(0);
  await expect(page.locator(".byte-detail")).toHaveCount(0);
});

test("virtual history bounds DOM without truncating retained data or exports", async ({
  page,
}) => {
  await setup(page);
  await page.getByRole("button", { name: "连接串口", exact: true }).click();
  await page.evaluate(() => {
    const s = window.serialTest;
    s.events = Array.from({ length: 5000 }, (_, i) => ({
      id: i + 1,
      session: s.status.session,
      timestamp: Date.now(),
      direction: "RX" as const,
      data: [
        ...new TextEncoder().encode(
          `history-${String(i + 1).padStart(5, "0")}|`,
        ),
      ],
    }));
    s.status.rx = s.events.reduce((n, e) => n + e.data.length, 0);
  });
  await expect(page.locator(".log-column-head")).toContainText(
    "4000 条读取记录",
  );
  await expect(page.locator('.log-row[data-event-id="5000"]')).toBeVisible();
  expect(await page.locator(".log-row").count()).toBeLessThan(100);
  await page.getByLabel("保存接收数据", { exact: true }).click();
  const exported = await page.evaluate(() => window.serialTest.savedText);
  expect(exported.match(/history-\d{5}\|/g)).toHaveLength(4000);
  expect(exported).toContain("history-01001|");
  expect(exported).toContain("history-05000|");
  expect(exported).not.toContain("history-01000|");
  await page.getByRole("button", { name: "暂停自动滚动", exact: true }).click();
  await page.locator(".log-surface").evaluate((e) => {
    e.scrollTop = 0;
  });
  await page.locator('.log-row[data-event-id="1001"]').click();
  await expect(page.locator(".byte-detail")).toContainText("#1001");
  await receive(page, 5001, "trimmed one record");
  await expect(page.locator(".byte-detail")).toHaveCount(0);
  await expect(page.locator(".log-column-head")).toContainText(
    "4000 条读取记录",
  );
  await expect(page.locator('.log-row[data-event-id="1002"]')).toBeVisible();
  expect(await page.locator(".log-row").count()).toBeLessThan(100);
});

test("virtual rows remeasure multiline content when format and viewport change", async ({
  page,
}) => {
  await setup(page);
  await page.getByRole("button", { name: "连接串口", exact: true }).click();
  await receive(page, 1, "long line ".repeat(50) + "\n" + "中文".repeat(60));
  await receive(page, 2, "visible tail");
  const tail = page.locator('.log-row[data-event-id="2"]');
  for (const view of ["HEX / ASCII", "HEX", "文本"]) {
    await page.getByRole("button", { name: view, exact: true }).click();
    await expect(tail).toBeInViewport();
    await expect
      .poll(() =>
        page.evaluate(() => {
          const first = document
            .querySelector('.log-row[data-event-id="1"]')!
            .getBoundingClientRect();
          const last = document
            .querySelector('.log-row[data-event-id="2"]')!
            .getBoundingClientRect();
          return last.top - first.bottom;
        }),
      )
      .toBeGreaterThanOrEqual(-1);
  }
  await page.setViewportSize({ width: 900, height: 640 });
  await expect(tail).toBeInViewport();
  await page.getByLabel("切换配置区域", { exact: true }).click();
  await expect(tail).toBeInViewport();
  await page.getByRole("button", { name: "断开连接", exact: true }).click();
  await expect(page.locator(".log-column-head")).toContainText("2 条读取记录");
});
