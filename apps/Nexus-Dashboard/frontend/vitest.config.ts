import { defineConfig } from "vitest/config";
import { fileURLToPath } from "node:url";

// Force a single React instance. Tests import nexus-design components straight
// from their .tsx source, and that package can carry its own node_modules/react
// in a separate tree. Without these aliases Vite's Node-style resolution picks
// whichever react is nearest the importing file, and hooks blow up with "Cannot
// read properties of null (reading 'useRef')" — two React copies, not a bug in
// the component. The jsx runtimes are listed before `react` so the more
// specific specifiers win.
const local = (file: string) => fileURLToPath(new URL(`./node_modules/${file}`, import.meta.url));

export default defineConfig({
  resolve: {
    alias: [
      { find: "react/jsx-dev-runtime", replacement: local("react/jsx-dev-runtime.js") },
      { find: "react/jsx-runtime", replacement: local("react/jsx-runtime.js") },
      { find: /^react-dom$/, replacement: local("react-dom/index.js") },
      { find: /^react-dom\/(.*)$/, replacement: local("react-dom/$1") },
      { find: /^react$/, replacement: local("react/index.js") },
    ],
  },
  test: {
    environment: "jsdom",
    environmentOptions: { jsdom: { url: "http://localhost" } },
    globals: true,
    setupFiles: ["./vitest.setup.ts"],
  },
});
