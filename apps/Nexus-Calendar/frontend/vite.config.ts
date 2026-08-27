import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";

export default defineConfig({
  plugins: [react(), tailwindcss()],
  server: {
    port: 5170,
    proxy: { "/api": { target: "http://localhost:3068", changeOrigin: true } },
  },
  // The same artifact is served at the standalone root and under Dashboard's
  // /calendar mount. Relative URLs remain within whichever document owns it.
  base: "./",
  build: { outDir: "dist" },
});
