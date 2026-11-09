import { randomUUID } from "node:crypto";

import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { FinanceEngine } from "./finance-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3076");
  const startedAt = Date.now();
  const engine = new FinanceEngine("data/nexus-finance.sqlite")
  const phantom = new PhantomApp("nexus-finance");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") {
        return json({ service: "nexus-finance", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), phantom: phantom.status() });
      }
      if (req.method === "GET" && p === "/api/v1/status") {
        return json({ service: "nexus-finance", status: "ready", capabilities: ["expenses", "budgets", "accounting"], phantom: phantom.status() });
      }
      if (req.method === "POST" && p === "/api/v1/finance/expenses") { const b = await req.json().catch(()=>({})) as any; if (!b.userId || !b.amount) return json({ error: "userId and amount required" }, 400); return json(engine.addExpense({ userId: b.userId, amount: b.amount, category: b.category, description: b.description, date: b.date }), 201); }
if (req.method === "GET" && p === "/api/v1/finance/expenses") { const uid = url.searchParams.get("userId") || "default"; return json({ expenses: engine.listExpenses(uid), summary: engine.getSummary(uid) }); }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-finance] Listening on port ${server.port}`);
  return { server, close: () => { phantom.stop(); server.stop(); } };
}
