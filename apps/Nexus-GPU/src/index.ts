import { serve } from "bun";

serve({
  fetch(req) {
    const url = new URL(req.url);
    if (req.method === "POST" && url.pathname === "/api/v1/gpu/validate") {
      return new Response(JSON.stringify({ jobId: "demo-gpu-1" }), {
        headers: { "content-type": "application/json" },
        status: 200,
      });
    }
    return new Response("not found", { status: 404 });
  },
  port: 8801,
});

