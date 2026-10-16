import { defineConfig } from "vitest/config";
import react from "@vitejs/plugin-react";
export default defineConfig({
  plugins: [react()],
  clearScreen: false,
  server: { port: 1420, strictPort: true },
  envPrefix: ["VITE_", "TAURI_ENV_"],
  build: {
    target: "es2022",
    rollupOptions: {
      output: {
        manualChunks: { terminal: ["@xterm/xterm", "@xterm/addon-fit"] },
      },
    },
  },
  test: { include: ["ui/src/**/*.test.ts"] },
});
