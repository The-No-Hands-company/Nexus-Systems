import path from "node:path";
import { defineConfig } from "vitest/config";

export default defineConfig({
  // Force a single React instance. Tests import nexus-design components
  // straight from their .tsx source (see overlay.test.tsx), and that package
  // carries its own node_modules/react (installed to satisfy its
  // peerDependency, pinned to 18.3.1 same as this app) in a separate
  // node_modules tree. Without this alias, Vite's Node-style resolution
  // picks whichever node_modules/react is nearest the importing file, so the
  // design package's copy wins and hooks blow up with "Cannot read
  // properties of null (reading 'useRef')" — two React copies, not a bug in
  // the component itself.
  resolve: {
    alias: {
      react: path.resolve(__dirname, "node_modules/react"),
      "react-dom": path.resolve(__dirname, "node_modules/react-dom"),
    },
  },
  test: {
    environment: "jsdom",
    environmentOptions: { jsdom: { url: "http://localhost" } },
    globals: true,
    setupFiles: ["./vitest.setup.ts"],
  },
});
