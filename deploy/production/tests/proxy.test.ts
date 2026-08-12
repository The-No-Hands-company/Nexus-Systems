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

describe("identity header is proxy-controlled", () => {
  it("strips a client-supplied x-nexus-identity before forwarding", async () => {
    const { handleRequest, __setRoutesForTest } = await import("../proxy");

    // A real upstream that reports back exactly what headers it received.
    let seen: string | null = "UNSET";
    const upstream = Bun.serve({
      port: 0,
      fetch(req) {
        seen = req.headers.get("x-nexus-identity");
        return new Response("ok");
      },
    });

    try {
      __setRoutesForTest({
        "echo.tnhc.dev": { upstream: `http://127.0.0.1:${upstream.port}`, requiresAuth: false },
      });

      // The attack: a client asserts an identity the proxy never minted. On a
      // public route no token is issued, so anything arriving here would be
      // taken at face value by an app that trusts the header.
      // http, not https: the proxy keeps the inbound scheme when rewriting the
      // upstream URL, and cloudflared always reaches the origin over http.
      const res = await handleRequest(new Request("http://echo.tnhc.dev/", {
        headers: { "x-nexus-identity": "forged.by.client" },
      }));

      expect(res.status).toBe(200);
      expect(seen).toBeNull();
    } finally {
      upstream.stop(true);
    }
  });
});

describe("isWebSocketUpgrade", () => {
  // Dynamic import, like every other test here: importing the proxy at module
  // scope used to bind port 8080 and fight the running server.
  const load = async () => (await import("../proxy")).isWebSocketUpgrade;
  const req = (headers: Record<string, string>) =>
    new Request("http://chat.tnhc.dev/gateway", { headers });

  it("recognises a plain upgrade request", async () => {
    const isWebSocketUpgrade = await load();
    expect(isWebSocketUpgrade(req({ upgrade: "websocket", connection: "Upgrade" }))).toBe(true);
  });

  it("recognises what browsers actually send", async () => {
    // Chrome and Firefox send a comma-separated list here. An equality check
    // against "upgrade" misses every real browser while passing a hand-rolled
    // curl, which is the worst way for this to be wrong.
    const isWebSocketUpgrade = await load();
    expect(
      isWebSocketUpgrade(req({ upgrade: "websocket", connection: "keep-alive, Upgrade" })),
    ).toBe(true);
  });

  it("is case-insensitive in both headers", async () => {
    const isWebSocketUpgrade = await load();
    expect(isWebSocketUpgrade(req({ upgrade: "WebSocket", connection: "UPGRADE" }))).toBe(true);
  });

  it("ignores an ordinary request", async () => {
    const isWebSocketUpgrade = await load();
    expect(isWebSocketUpgrade(req({}))).toBe(false);
  });

  it("ignores a non-websocket upgrade", async () => {
    const isWebSocketUpgrade = await load();
    expect(isWebSocketUpgrade(req({ upgrade: "h2c", connection: "Upgrade" }))).toBe(false);
  });

  it("requires Connection, not just Upgrade", async () => {
    const isWebSocketUpgrade = await load();
    expect(isWebSocketUpgrade(req({ upgrade: "websocket" }))).toBe(false);
  });

  it("does not match a Connection value that merely contains the word", async () => {
    const isWebSocketUpgrade = await load();
    expect(
      isWebSocketUpgrade(req({ upgrade: "websocket", connection: "no-upgrade-here" })),
    ).toBe(false);
  });
});

describe("response encoding hygiene", () => {
  // A browser sends Accept-Encoding on every request; curl sends none unless
  // asked. So the upstream compressed for browsers and not for our tests, and
  // the mismatch below was invisible from the command line while breaking every
  // real page load.
  it("does not forward Content-Encoding, because the body is already decoded", async () => {
    const { sanitizeResponseHeaders } = await import("../proxy");
    const headers = new Headers({
      "content-encoding": "gzip",
      "content-length": "123",
      "content-type": "application/json",
    });
    sanitizeResponseHeaders(headers);
    expect(headers.get("content-encoding")).toBeNull();
    expect(headers.get("content-length")).toBeNull();
    // Everything else must survive: this is hygiene, not a purge.
    expect(headers.get("content-type")).toBe("application/json");
  });
});
