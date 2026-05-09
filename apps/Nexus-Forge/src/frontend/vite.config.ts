import react from "@vitejs/plugin-react";
import { defineConfig } from "vite";

export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      "/api": {
        target: "http://localhost:8090",
        changeOrigin: true,
      },
      "/.well-known": {
        target: "http://localhost:8090",
        changeOrigin: true,
      },
    },
  },
  build: {
    outDir: "../../dist/frontend",
    emptyOutDir: true,
  },
});
