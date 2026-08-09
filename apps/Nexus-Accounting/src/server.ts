import { randomUUID } from "node:crypto";

import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { AccountingEngine } from "./accounting-engine";
import { startHeartbeat } from "./cloud";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3061");
  const baseUrl = process.env.NEXUS_ACCOUNTING_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new AccountingEngine(process.env.NEXUS_ACCOUNTING_DB_PATH || ":memory:")
  const phantom = new PhantomApp("nexus-accounting");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-accounting", status: "ok", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/accounting/summary") return json(engine.getSummary());

      if (req.method === "POST" && p === "/api/v1/accounting/transactions") {
        const b = await req.json().catch(() => ({})) as Record<string, unknown>;
        if (!b.accountId || !b.type || !b.amount) return json({ error: "accountId, type, amount required" }, 400);
        return json(engine.addTransaction({ accountId: b.accountId as string, type: b.type as any, amount: b.amount as number, currency: (b.currency as string) || "USD", category: (b.category as string) || "uncategorized", description: (b.description as string) || "", date: (b.date as string) || new Date().toISOString().slice(0, 10), reconciled: false }), 201);
      }
      if (req.method === "GET" && p === "/api/v1/accounting/transactions") {
        const accountId = url.searchParams.get("accountId") || "default";
        return json({ transactions: engine.getTransactions(accountId), balance: engine.getBalance(accountId) });
      }
      if (req.method === "POST" && p === "/api/v1/accounting/invoices") {
        const b = await req.json().catch(() => ({})) as Record<string, unknown>;
        if (!b.clientId || !Array.isArray(b.items)) return json({ error: "clientId, items required" }, 400);
        return json(engine.createInvoice(b.clientId as string, b.items as any[], b.dueDate as string), 201);
      }
      if (req.method === "GET" && p === "/api/v1/accounting/invoices") return json({ invoices: engine.listInvoices() });
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-accounting] Listening on port ${server.port}`);
  const stop = startHeartbeat(baseUrl);
  return { server, close: () => { stop(); engine.close(); phantom.stop(); server.stop(); } };
}
