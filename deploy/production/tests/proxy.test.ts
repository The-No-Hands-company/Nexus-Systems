import { describe, it, expect } from "bun:test";

describe("proxy module", () => {
  it("can be imported without binding a port", async () => {
    // If proxy.ts still calls Bun.serve at module scope this either throws
    // EADDRINUSE against the running production proxy or, worse, silently
    // steals port 8080 from it.
    const mod = await import("../proxy");
    expect(typeof mod.handleRequest).toBe("function");
    expect(typeof mod.startProxy).toBe("function");
  });

  it("404s an unknown host", async () => {
    const { handleRequest } = await import("../proxy");
    const res = await handleRequest(new Request("http://nope.example.com/"));
    expect(res.status).toBe(404);
  });
});

describe("route policy", () => {
  it("defaults to public when Cloud says nothing about auth", async () => {
    const { buildRouteMap } = await import("../proxy");
    const map = buildRouteMap([{ domain: "draw.tnhc.dev", upstream: "http://127.0.0.1:9" }] as never);
    expect(map["draw.tnhc.dev"]).toEqual({ upstream: "http://127.0.0.1:9", requiresAuth: false });
  });

  it("honours requiresAuth when Cloud sets it", async () => {
    const { buildRouteMap } = await import("../proxy");
    const map = buildRouteMap([
      { domain: "chat.tnhc.dev", upstream: "http://127.0.0.1:9", requiresAuth: true },
    ] as never);
    expect(map["chat.tnhc.dev"]!.requiresAuth).toBe(true);
  });

  it("lowercases the host so policy cannot be dodged by casing", async () => {
    const { buildRouteMap } = await import("../proxy");
    const map = buildRouteMap([
      { domain: "CHAT.tnhc.dev", upstream: "http://127.0.0.1:9", requiresAuth: true },
    ] as never);
    expect(map["chat.tnhc.dev"]!.requiresAuth).toBe(true);
  });
});
