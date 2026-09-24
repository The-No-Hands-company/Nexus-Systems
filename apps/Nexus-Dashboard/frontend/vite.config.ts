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
  resolve: {
    // @nexus/design declares react as a peerDependency, but a stray copy
    // (currently 19.x) lives in its own node_modules, and this app imports
    // its components from there by relative path — so without deduping,
    // Vite's resolution can pick that copy for some imports and this app's
    // 18.3.1 for others, shipping two React instances and breaking hooks
    // at runtime in dev, build and preview alike.
    dedupe: ["react", "react-dom"],
  },
  build: { target: "esnext" },
});
