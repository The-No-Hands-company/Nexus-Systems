import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-finance", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;

  beforeAll(async () => {
    handle = await createServer(0, ":memory:");
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-finance");
    expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-finance");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("GET / serves Financial Dashboard UI", async () => {
    const res = await fetch(`${base}/`);
    expect(res.status).toBe(200);
    expect(res.headers.get("content-type")).toContain("text/html");
    const html = await res.text();
    expect(html).toContain("Nexus Finance");
  });

  it("manages expenses and summary", async () => {
    const add = await fetch(`${base}/api/v1/finance/expenses`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        userId: "test-user",
        amount: 450,
        type: "expense",
        category: "infrastructure",
        description: "Cloud server instances",
      }),
    });
    expect(add.status).toBe(201);

    const list = await fetch(`${base}/api/v1/finance/expenses?userId=test-user`);
    expect(list.status).toBe(200);
    const body = await list.json() as any;
    expect(body.expenses.length).toBeGreaterThan(0);
    expect(body.summary.totalExpense).toBe(450);
  });

  it("creates and updates invoices", async () => {
    const create = await fetch(`${base}/api/v1/finance/invoices`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        userId: "test-user",
        clientName: "Enterprise Client Corp",
        amount: 3200,
        currency: "USD",
      }),
    });
    expect(create.status).toBe(201);
    const inv = await create.json() as any;
    expect(inv.clientName).toBe("Enterprise Client Corp");

    const patch = await fetch(`${base}/api/v1/finance/invoices/${inv.id}/status`, {
      method: "PATCH",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ status: "paid" }),
    });
    expect(patch.status).toBe(200);
  });
});
