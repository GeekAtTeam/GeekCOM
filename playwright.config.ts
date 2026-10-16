import { defineConfig } from "@playwright/test";
export default defineConfig({
  testDir: "./ui/tests",
  use: {
    baseURL: "http://127.0.0.1:1420",
    viewport: { width: 1280, height: 820 },
  },
  webServer: {
    command: "npm run dev",
    url: "http://127.0.0.1:1420",
    reuseExistingServer: !process.env.CI,
  },
});
