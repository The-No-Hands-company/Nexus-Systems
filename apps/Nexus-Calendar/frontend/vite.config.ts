import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";

export default defineConfig({
  plugins: [react(), tailwindcss()],
  server: {
    port: 5170,
    proxy: { "/api": { target: "http://localhost:3068", changeOrigin: true } },
  },
  build: { outDir: "dist" },
});
