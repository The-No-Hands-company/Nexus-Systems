import { defineConfig } from "vite";
import { resolve } from "path";

export default defineConfig({
  resolve: {
    alias: {
      "@engine": resolve(__dirname, "./src/engine"),
      "@game": resolve(__dirname, "./src/game"),
      "@systems": resolve(__dirname, "./src/systems"),
      "@game-types": resolve(__dirname, "./src/types"),
      "@utils": resolve(__dirname, "./src/utils"),
      "@tools": resolve(__dirname, "./src/tools"),
    },
  },
  server: {
    port: 3000,
    host: "0.0.0.0",
    open: true,
  },
  build: {
    target: "ES2020",
    outDir: "dist",
    sourcemap: false,
    chunkSizeWarningLimit: 5000,
    rollupOptions: {
      output: {
        manualChunks: {
          "babylon-core": ["@babylonjs/core"],
          "babylon-extras": [
            "@babylonjs/loaders",
            "@babylonjs/materials",
            "@babylonjs/inspector",
          ],
        },
      },
    },
  },
});
