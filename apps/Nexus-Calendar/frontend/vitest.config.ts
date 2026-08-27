import { defineConfig } from "vitest/config";
import { fileURLToPath } from "node:url";

export default defineConfig({
  esbuild: { jsx: "automatic" },
  resolve: {
    alias: [
      { find: "react/jsx-dev-runtime", replacement: fileURLToPath(new URL("./node_modules/react/jsx-dev-runtime.js", import.meta.url)) },
      { find: "react/jsx-runtime", replacement: fileURLToPath(new URL("./node_modules/react/jsx-runtime.js", import.meta.url)) },
      { find: "react", replacement: fileURLToPath(new URL("./node_modules/react/index.js", import.meta.url)) },
    ],
  },
  test: {
    environment: "jsdom",
    globals: true,
    setupFiles: ["./vitest.setup.ts"],
  },
});
