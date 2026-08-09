import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createSearchServer } from "../src/server";

describe("nexus-search v2", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createSearchServer>>;

  beforeAll(async () => {
    handle = await createSearchServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });
  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const r = await fetch(`${base}/health`);
    expect(r.status).toBe(200);
  });

  it("index and search with facets", async () => {
    await fetch(`${base}/api/v1/search/index`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ title: "Guardian Policy", content: "Guardian enforces safety and trust decisions across the ecosystem", source: "guardian" }),
    });
    await fetch(`${base}/api/v1/search/index`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ title: "Tunnel Routing", content: "Tunnel manages HTTPS reachability and route health for sovereign exposure", source: "tunnel" }),
    });

    const r = await fetch(`${base}/api/v1/search/query`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ query: "safety trust", facets: true }),
    });
    expect(r.status).toBe(200);
    const body = await r.json() as { results: Array<{ title: string }>; facets: Record<string, Array<{ value: string }>>; total: number };
    expect(body.total).toBeGreaterThan(0);
    expect(body.facets).toBeDefined();
    expect(body.facets!.source).toBeDefined();
  });

  it("add federated source", async () => {
    const r = await fetch(`${base}/api/v1/search/sources`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: "nexus-files-local", url: "http://localhost:3033", type: "files" }),
    });
    expect(r.status).toBe(201);

    const list = await fetch(`${base}/api/v1/search/sources`);
    expect(list.status).toBe(200);
    const body = await list.json() as { sources: Array<{ name: string }> };
    expect(body.sources.some((s) => s.name === "nexus-files-local")).toBe(true);
  });

  it("batch index documents", async () => {
    const r = await fetch(`${base}/api/v1/search/index/batch`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ documents: [
        { title: "Auth Service", content: "Handles identity, tokens, and API keys", source: "docs" },
        { title: "Edge Gateway", content: "Ingress routing with rate limiting and threat detection", source: "docs" },
      ]}),
    });
    expect(r.status).toBe(201);
    const body = await r.json() as { indexed: number };
    expect(body.indexed).toBe(2);
  });

  it("search by source filter", async () => {
    const r = await fetch(`${base}/api/v1/search/query`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ query: "routing", source: "docs" }),
    });
    expect(r.status).toBe(200);
    const body = await r.json() as { results: Array<{ source: string }> };
    expect(body.results.every((r) => r.source === "docs")).toBe(true);
  });

  it("GET /api/v1/search/status shows sources", async () => {
    const r = await fetch(`${base}/api/v1/search/status`);
    expect(r.status).toBe(200);
    const body = await r.json() as { sources: Array<{ name: string }> };
    expect(Array.isArray(body.sources)).toBe(true);
  });
});
