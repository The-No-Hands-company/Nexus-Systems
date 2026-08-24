import { describe, it, expect, beforeAll } from "bun:test";

beforeAll(() => { process.env.GATE_SKIP_AUTH = "true"; });

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

    // Capture the request at the proxy's fetch boundary. A real Bun.serve
    // listener makes this unit test depend on permission to bind local ports,
    // which sandboxed production checks deliberately do not have.
    let seen: string | null = "UNSET";
    const realFetch = globalThis.fetch;
    globalThis.fetch = Object.assign(
      async (input: Parameters<typeof fetch>[0]) => {
        const req = input instanceof Request ? input : new Request(input);
        seen = req.headers.get("x-nexus-identity");
        return new Response("ok");
      },
      { preconnect: realFetch.preconnect },
    );

    try {
      __setRoutesForTest({
        "echo.tnhc.dev": { upstream: "http://upstream.invalid", requiresAuth: false },
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
      globalThis.fetch = realFetch;
    }
  });
});

describe("Dashboard terminal WebSocket routing", () => {
  const upgradeRequest = (url: string) =>
    new Request(url, {
      headers: {
        connection: "Upgrade",
        upgrade: "websocket",
        origin: "https://app.tnhc.dev",
        cookie: "nexus_session=browser-session; theme=dark",
      },
    });

  it("pins the exact public attach path to Dashboard with its original credentials", async () => {
    const { handleRequest, __setRoutesForTest } = await import("../proxy");
    const previous = process.env.DASHBOARD_UPSTREAM;
    process.env.DASHBOARD_UPSTREAM = "http://127.0.0.1:43132";
    let data: import("../proxy").WsProxyData | null = null;

    try {
      __setRoutesForTest({
        "app.tnhc.dev": { upstream: "http://client-selected.invalid", requiresAuth: false },
      });
      await handleRequest(upgradeRequest("http://app.tnhc.dev/api/terminal/attach?cols=91&rows=27"), {
        upgrade(_request, options) {
          data = options.data;
          return true;
        },
      });

      expect(data?.upstreamUrl).toBe("http://127.0.0.1:43132");
      expect(data?.dashboardTerminalHeaders).toEqual({
        cookie: "nexus_session=browser-session; theme=dark",
        origin: "https://app.tnhc.dev",
      });
      expect(data?.requestUrl).toBe("http://app.tnhc.dev/api/terminal/attach?cols=91&rows=27");
    } finally {
      if (previous === undefined) Reflect.deleteProperty(process.env, "DASHBOARD_UPSTREAM");
      else process.env.DASHBOARD_UPSTREAM = previous;
    }
  });

  it("never carries browser credentials on sibling WebSocket routes or hosts", async () => {
    const { handleRequest, __setRoutesForTest } = await import("../proxy");
    __setRoutesForTest({
      "app.tnhc.dev": { upstream: "http://127.0.0.1:43132", requiresAuth: false },
      "chat.tnhc.dev": { upstream: "http://127.0.0.1:43133", requiresAuth: false },
    });

    for (const url of [
      "http://app.tnhc.dev/api/terminal/attach/",
      "http://app.tnhc.dev/gateway",
      "http://www.app.tnhc.dev/api/terminal/attach",
      "http://chat.tnhc.dev/api/terminal/attach",
    ]) {
      let data: import("../proxy").WsProxyData | null = null;
      await handleRequest(upgradeRequest(url), {
        upgrade(_request, options) {
          data = options.data;
          return true;
        },
      });

      expect(data?.dashboardTerminalHeaders).toBeUndefined();
    }
  });

  it("never synthesizes terminal credentials and rejects a non-loopback fixed upstream", async () => {
    const { handleRequest, __setRoutesForTest } = await import("../proxy");
    const previous = process.env.DASHBOARD_UPSTREAM;
    __setRoutesForTest({
      "app.tnhc.dev": { upstream: "http://client-selected.invalid", requiresAuth: false },
    });

    try {
      process.env.DASHBOARD_UPSTREAM = "http://127.0.0.1:43132";
      let missingData: import("../proxy").WsProxyData | null = null;
      const missing = new Request("http://app.tnhc.dev/api/terminal/attach", {
        headers: { connection: "Upgrade", upgrade: "websocket" },
      });
      await handleRequest(missing, {
        upgrade(_request, options) {
          missingData = options.data;
          return true;
        },
      });
      expect(missingData?.dashboardTerminalHeaders).toEqual({});

      process.env.DASHBOARD_UPSTREAM = "https://credential-sink.example";
      let upgraded = false;
      const response = await handleRequest(upgradeRequest(
        "http://app.tnhc.dev/api/terminal/attach",
      ), {
        upgrade() {
          upgraded = true;
          return true;
        },
      });
      expect(response.status).toBe(503);
      expect(upgraded).toBe(false);
    } finally {
      if (previous === undefined) Reflect.deleteProperty(process.env, "DASHBOARD_UPSTREAM");
      else process.env.DASHBOARD_UPSTREAM = previous;
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
