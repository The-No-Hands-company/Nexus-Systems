import { afterEach, beforeAll, describe, expect, it } from "bun:test";

beforeAll(async () => {
  // These tests exercise forwarding and isolation, not the login gate.
  process.env.GATE_SKIP_AUTH = "true";
  const { __setMaintenanceForTest } = await import("../proxy");
  __setMaintenanceForTest([]);
});

afterEach(async () => {
  const { __setRoutesForTest } = await import("../proxy");
  __setRoutesForTest({});
});

type Seen = Record<string, string | null>;

function listen(onRequest: (req: Request) => Response | void) {
  return Bun.serve({
    port: 0,
    hostname: "127.0.0.1",
    fetch(req) {
      return onRequest(req) ?? new Response("ok");
    },
  });
}

describe("routes Cloud marks as a member's app", () => {
  it("become public, isolated routes, and a mistyped flag does not", async () => {
    const { buildRouteMap } = await import("../proxy");
    const map = buildRouteMap([
      { domain: "mine.tnhc.dev", upstream: "http://127.0.0.1:20001", requiresAuth: true, userDeployed: true },
      { domain: "typo.tnhc.dev", upstream: "http://127.0.0.1:20002", userDeployed: "true" as never },
      { domain: "draw.tnhc.dev", upstream: "http://127.0.0.1:8091", requiresAuth: true },
    ]);
    expect(map["mine.tnhc.dev"]).toMatchObject({ kind: "site", userDeployed: true, requiresAuth: false });
    expect(map["typo.tnhc.dev"]).toMatchObject({ kind: "app" });
    expect(map["typo.tnhc.dev"]?.userDeployed).toBeUndefined();
    expect(map["draw.tnhc.dev"]).toMatchObject({ kind: "app", requiresAuth: true });
  });

  it("never see the visitor's cookie, credentials, address or tag, and cannot plant a parent-domain cookie", async () => {
    let seen: Seen = {};
    const app = listen((req) => {
      seen = {
        cookie: req.headers.get("cookie"),
        authorization: req.headers.get("authorization"),
        tag: req.headers.get("x-nexus-client-tag"),
        cfIp: req.headers.get("cf-connecting-ip"),
        xff: req.headers.get("x-forwarded-for"),
        identity: req.headers.get("x-nexus-identity"),
        subject: req.headers.get("x-nexus-subject"),
        host: req.headers.get("x-forwarded-host"),
      };
      const headers = new Headers({ "content-type": "text/plain" });
      headers.append("set-cookie", "nexus_session=planted; Domain=.tnhc.dev; Path=/");
      headers.append("set-cookie", "theme=dark; Path=/");
      return new Response("hello", { headers });
    });
    try {
      const { handleRequest, buildRouteMap, __setRoutesForTest } = await import("../proxy");
      __setRoutesForTest(
        buildRouteMap([{ domain: "mine.tnhc.dev", upstream: `http://127.0.0.1:${app.port}`, userDeployed: true }]),
      );
      const res = await handleRequest(
        new Request("http://mine.tnhc.dev/page", {
          headers: {
            cookie: "nexus_session=victim",
            authorization: "Bearer victim",
            "cf-connecting-ip": "203.0.113.77",
            "x-forwarded-for": "203.0.113.77",
            "x-nexus-identity": "forged",
            "x-nexus-subject": "forged",
          },
        }),
      );
      expect(res.status).toBe(200);
      expect(seen.cookie).toBeNull();
      expect(seen.authorization).toBeNull();
      expect(seen.tag).toBeNull();
      expect(seen.cfIp).toBeNull();
      expect(seen.xff).toBeNull();
      expect(seen.identity).toBeNull();
      expect(seen.subject).toBeNull();
      expect(seen.host).toBe("mine.tnhc.dev");
      expect(res.headers.getSetCookie()).toEqual(["theme=dark; Path=/"]);
    } finally {
      app.stop(true);
    }
  });

  it("are reachable without a session even when the gate is on", async () => {
    const app = listen(() => new Response("public app"));
    delete process.env.GATE_SKIP_AUTH;
    try {
      const { handleRequest, buildRouteMap, __setRoutesForTest } = await import("../proxy");
      __setRoutesForTest(
        buildRouteMap([{ domain: "mine.tnhc.dev", upstream: `http://127.0.0.1:${app.port}`, userDeployed: true }]),
      );
      const res = await handleRequest(new Request("http://mine.tnhc.dev/"));
      expect(res.status).toBe(200);
      expect(await res.text()).toBe("public app");
    } finally {
      process.env.GATE_SKIP_AUTH = "true";
      app.stop(true);
    }
  });

  it("may be called with a same-site mutation, which only ecosystem apps refuse", async () => {
    const app = listen(() => new Response("created", { status: 201 }));
    try {
      const { handleRequest, buildRouteMap, __setRoutesForTest } = await import("../proxy");
      __setRoutesForTest(
        buildRouteMap([{ domain: "mine.tnhc.dev", upstream: `http://127.0.0.1:${app.port}`, userDeployed: true }]),
      );
      const res = await handleRequest(
        new Request("http://mine.tnhc.dev/api", { method: "POST", headers: { "sec-fetch-site": "same-site" }, body: "{}" }),
      );
      expect(res.status).toBe(201);
    } finally {
      app.stop(true);
    }
  });

  it("do not change what an ordinary registered app receives", async () => {
    let seen: Seen = {};
    const app = listen((req) => {
      seen = { cookie: req.headers.get("cookie"), tag: req.headers.get("x-nexus-client-tag") };
    });
    try {
      const { handleRequest, buildRouteMap, __setRoutesForTest } = await import("../proxy");
      __setRoutesForTest(buildRouteMap([{ domain: "draw.tnhc.dev", upstream: `http://127.0.0.1:${app.port}` }]));
      await handleRequest(new Request("http://draw.tnhc.dev/", { headers: { cookie: "nexus_session=me", "cf-connecting-ip": "203.0.113.5" } }));
      expect(seen.cookie).toBe("nexus_session=me");
      expect(seen.tag).toMatch(/^[A-Za-z0-9_-]{22}$/);
    } finally {
      app.stop(true);
    }
  });

  it("get a WebSocket upgrade that carries no visitor tag", async () => {
    const app = listen(() => new Response("ok"));
    try {
      const { handleRequest, buildRouteMap, __setRoutesForTest } = await import("../proxy");
      __setRoutesForTest(
        buildRouteMap([
          { domain: "mine.tnhc.dev", upstream: `http://127.0.0.1:${app.port}`, userDeployed: true },
          { domain: "draw.tnhc.dev", upstream: `http://127.0.0.1:${app.port}` },
        ]),
      );
      const upgrades: Array<{ omitClientTag?: boolean }> = [];
      const server = {
        upgrade: (_req: Request, opts: { data: { omitClientTag?: boolean } }) => {
          upgrades.push(opts.data);
          return true;
        },
      };
      const ws = (host: string) =>
        new Request(`http://${host}/socket`, { headers: { upgrade: "websocket", connection: "Upgrade" } });
      await handleRequest(ws("mine.tnhc.dev"), server as never);
      await handleRequest(ws("draw.tnhc.dev"), server as never);
      expect(upgrades[0]?.omitClientTag).toBe(true);
      expect(upgrades[1]?.omitClientTag).toBeUndefined();
    } finally {
      app.stop(true);
    }
  });
});
