import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  plugins: [react()],
  server: {
    port: 5175,
    // Same-origin in dev too, so the app never learns to make cross-origin
    // auth calls that would not work in production.
    proxy: { "/api": { target: "http://localhost:3132", changeOrigin: true } },
  },
  build: { target: "esnext" },
});
