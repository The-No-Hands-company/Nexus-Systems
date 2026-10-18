import { afterEach, beforeEach, describe, expect, it } from "bun:test";
import { heartbeatWithCloud, registerWithCloud, startHeartbeat } from "../src/cloud";

const keys = [
  "NEXUS_CLOUD_URL",
  "NEXUS_CALENDAR_TOOL_ID",
  "NEXUS_CALENDAR_PATH",
  "NEXUS_CALENDAR_PUBLIC_URL",
  "NEXUS_CALENDAR_ENABLE_CLOUD_INTEGRATION",
  "NEXUS_CALENDAR_HEALTH_TIMEOUT_MS",
] as const;

describe("Calendar observed Cloud health", () => {
  const saved = new Map<string, string | undefined>();
  let cloud: ReturnType<typeof Bun.serve>;
  let calendar: ReturnType<typeof Bun.serve>;
  let received: { path: string; body: Record<string, unknown> }[];
  let probes: number;
  let response: () => Response | Promise<Response>;
  let base: string;

  beforeEach(() => {
    for (const key of keys) {
      saved.set(key, process.env[key]);
      delete process.env[key];
    }
    received = [];
    probes = 0;
    response = () => Response.json({ service: "nexus-calendar", status: "ok", ready: true });
    calendar = Bun.serve({
      hostname: "127.0.0.1",
      port: 0,
      fetch(req) {
        expect(new URL(req.url).pathname).toBe("/health");
        probes++;
        return response();
      },
    });
    cloud = Bun.serve({
      hostname: "127.0.0.1",
      port: 0,
      async fetch(req) {
        received.push({
          path: new URL(req.url).pathname,
          body: (await req.json()) as Record<string, unknown>,
        });
        return Response.json({ ok: true });
      },
    });
    process.env.NEXUS_CLOUD_URL = `http://127.0.0.1:${cloud.port}`;
    base = `http://127.0.0.1:${calendar.port}`;
  });
  afterEach(() => {
    calendar.stop(true);
    cloud.stop(true);
    for (const key of keys) {
      const value = saved.get(key);
      if (value === undefined) delete process.env[key];
      else process.env[key] = value;
    }
  });

  it("registers production defaults only after a valid health observation", async () => {
    await registerWithCloud(base);
    expect(probes).toBe(1);
    expect(received[0]).toMatchObject({
      path: "/api/v1/tools",
      body: {
        id: "nexus-calendar",
        health: "healthy",
        path: "/calendar",
        publicUrl: "https://calendar.tnhc.dev",
        upstreamUrl: base,
      },
    });
  });

  it("uses an isolated identity and route for registration and heartbeats", async () => {
    process.env.NEXUS_CALENDAR_TOOL_ID = "calendar-proof";
    process.env.NEXUS_CALENDAR_PATH = "/calendar-proof";
    process.env.NEXUS_CALENDAR_PUBLIC_URL = "http://127.0.0.1:9999";
    response = () => Response.json({ service: "calendar-proof", status: "ok", ready: true });
    await registerWithCloud(base);
    await heartbeatWithCloud(base);
    expect(probes).toBe(2);
    expect(received[0]?.body).toMatchObject({
      id: "calendar-proof",
      path: "/calendar-proof",
      publicUrl: "http://127.0.0.1:9999",
      health: "healthy",
    });
    expect(received[1]).toMatchObject({
      path: "/api/v1/tools/calendar-proof/heartbeat",
      body: { health: "healthy", upstreamUrl: base },
    });
  });

  for (const [name, makeResponse] of [
    [
      "HTTP failure",
      () =>
        Response.json({ service: "nexus-calendar", status: "ok", ready: true }, { status: 503 }),
    ],
    ["wrong service", () => Response.json({ service: "other", status: "ok", ready: true })],
    ["missing readiness", () => Response.json({ service: "nexus-calendar", status: "ok" })],
    ["not ready", () => Response.json({ service: "nexus-calendar", status: "ok", ready: false })],
    [
      "failure status",
      () => Response.json({ service: "nexus-calendar", status: "error", ready: true }),
    ],
    ["invalid JSON", () => new Response("not JSON")],
    ["null JSON", () => Response.json(null)],
  ] as const) {
    it(`reports degraded after ${name} on registration and heartbeat`, async () => {
      response = makeResponse;
      await registerWithCloud(base);
      await heartbeatWithCloud(base);
      expect(received.map((item) => item.body.health)).toEqual(["degraded", "degraded"]);
    });
  }

  it("reports a failed connection as degraded", async () => {
    calendar.stop(true);
    await registerWithCloud(base);
    expect(received[0]?.body.health).toBe("degraded");
  });

  it("bounds slow probes and reports timeout as degraded", async () => {
    process.env.NEXUS_CALENDAR_HEALTH_TIMEOUT_MS = "25";
    response = async () => {
      await Bun.sleep(150);
      return Response.json({ service: "nexus-calendar", status: "ok", ready: true });
    };
    const start = Date.now();
    await heartbeatWithCloud(base);
    expect(Date.now() - start).toBeLessThan(140);
    expect(received[0]?.body.health).toBe("degraded");
  });

  it("opt-out prevents registration and heartbeat network calls", async () => {
    process.env.NEXUS_CALENDAR_ENABLE_CLOUD_INTEGRATION = "false";
    await registerWithCloud(base);
    await heartbeatWithCloud(base);
    const stop = startHeartbeat(base);
    await Bun.sleep(10);
    stop();
    expect(probes).toBe(0);
    expect(received).toEqual([]);
  });
});
