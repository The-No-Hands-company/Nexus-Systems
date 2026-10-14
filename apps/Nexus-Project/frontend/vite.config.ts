import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";
import { fileURLToPath } from "node:url";

export default defineConfig({
  plugins: [react(), tailwindcss()],
  resolve: {
    alias: [
      { find: "react/jsx-runtime", replacement: fileURLToPath(new URL("./node_modules/react/jsx-runtime.js", import.meta.url)) },
      { find: "react", replacement: fileURLToPath(new URL("./node_modules/react/index.js", import.meta.url)) },
    ],
  },
  server: {
    port: 5173,
    proxy: { "/api": { target: "http://localhost:3152", changeOrigin: true } },
  },
  base: "./",
  build: { outDir: "dist" },
});
