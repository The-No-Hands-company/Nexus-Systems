import { serve, file, type Server } from "bun";
import path from "path";
import fs from "fs";

const MIME: Record<string, string> = {
  ".html": "text/html",
  ".js": "application/javascript",
  ".mjs": "application/javascript",
  ".css": "text/css",
  ".json": "application/json",
  ".svg": "image/svg+xml",
  ".png": "image/png",
  ".woff2": "font/woff2",
  ".woff": "font/woff",
  ".ttf": "font/ttf",
  ".ico": "image/x-icon",
  ".webp": "image/webp",
};

export interface StaticServer {
  port: number;
  stop(): void;
}

export function serveDir(distDir: string): StaticServer {
  const server: Server = serve({
    port: 0,
    async fetch(req) {
      const url = new URL(req.url);
      let pathname = url.pathname;

      // API paths not handled here — let Playwright intercept them
      if (pathname.startsWith("/api/")) {
        return new Response(JSON.stringify({ error: "not mocked" }), {
          status: 404,
          headers: { "content-type": "application/json" },
        });
      }

      let filePath = path.join(distDir, pathname);

      // SPA fallback: no extension or directory → serve index.html
      const ext = path.extname(filePath);
      if (!ext || !fs.existsSync(filePath) || fs.statSync(filePath).isDirectory()) {
        filePath = path.join(distDir, "index.html");
      }

      if (!fs.existsSync(filePath)) {
        return new Response("Not found", { status: 404 });
      }

      const contentType = MIME[path.extname(filePath)] ?? "application/octet-stream";
      return new Response(file(filePath), { headers: { "content-type": contentType } });
    },
  });

  return {
    port: server.port,
    stop() { server.stop(); },
  };
}
