import { describe, it, expect, beforeAll, afterAll } from "bun:test";
import { createServer } from "../src/server";

/**
 * The checks in front of the shell.
 *
 * Nexus-Terminal hands out an uncontained shell on this host, so these three
 * behaviours are the entire security boundary (see docs/TERMINAL-SECURITY.md).
 * They are asserted here rather than assumed, and they are asserted through the
 * HTTP surface rather than by calling the helpers directly — what matters is
 * what an attacker reaches, not what a function returns.
 */
describe("terminal attach guards", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;
  let authServer: ReturnType<typeof Bun.serve>;
  const originalAuthUrl = process.env.NEXUS_AUTH_INTERNAL_URL;

  beforeAll(async () => {
    // This is a real HTTP boundary with a deliberately tiny Auth double: the
    // terminal must use the role Auth gives it for the caller's own cookie.
    authServer = Bun.serve({
      hostname: "127.0.0.1",
      port: 0,
      fetch(request) {
        const cookie = request.headers.get("cookie") ?? "";
        const role = cookie.match(/(?:^|;\s*)role=([^;]*)/)?.[1];

        if (role === "outage") return new Response(null, { status: 503 });
        if (role === undefined || role === "missing-session") {
          return new Response(null, { status: 401 });
        }
        if (!role) return Response.json({ user: { id: "user-without-role" } });

        return Response.json({ user: { id: `user-${role}`, role } });
      },
    });
    process.env.NEXUS_AUTH_INTERNAL_URL = `http://127.0.0.1:${authServer.port}`;

    // Explicitly off. The default must be safe, and asserting against it here
    // also proves the flag is actually read.
    delete process.env.NEXUS_TERMINAL_ENABLED;
    process.env.PORT = "0";
    handle = await createServer();
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => {
    handle.close();
    authServer.stop();
    if (originalAuthUrl === undefined) delete process.env.NEXUS_AUTH_INTERNAL_URL;
    else process.env.NEXUS_AUTH_INTERNAL_URL = originalAuthUrl;
  });

  it("refuses to attach when the shell is not explicitly enabled", async () => {
    const res = await fetch(`${base}/api/v1/terminal/attach`);
    expect(res.status).toBe(403);
    expect((await res.json() as { error: string }).error).toBe("terminal_disabled");
  });

  it("still serves health while the shell is disabled", async () => {
    // Turning the shell off must not make Cloud think the node is degraded.
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["status"]).toBe("ok");
    expect(body["shellEnabled"]).toBe(false);
  });

  it("refuses an unauthenticated attach even once enabled", async () => {
    process.env.NEXUS_TERMINAL_ENABLED = "true";
    try {
      // No cookie at all: this must not reach a shell.
      const res = await fetch(`${base}/api/v1/terminal/attach`);
      expect(res.status).toBe(401);
      expect((await res.json() as { error: string }).error).toBe("not_authenticated");
    } finally {
      delete process.env.NEXUS_TERMINAL_ENABLED;
    }
  });

  it("refuses a forged identity header", async () => {
    // Identity is asked of Auth with the caller's cookies. A caller that sets
    // its own subject header must gain nothing — this is the difference
    // between an auth check and a suggestion.
    process.env.NEXUS_TERMINAL_ENABLED = "true";
    try {
      const res = await fetch(`${base}/api/v1/terminal/attach`, {
        headers: { "x-nexus-subject": "admin", "x-forwarded-user": "admin" },
      });
      expect(res.status).toBe(401);
    } finally {
      delete process.env.NEXUS_TERMINAL_ENABLED;
    }
  });

  it("keeps the audit endpoint behind the same check", async () => {
    // The audit log records who ran what; reading it must not be easier than
    // the thing it audits.
    const res = await fetch(`${base}/api/v1/terminal/audit`);
    expect(res.status).toBe(401);
  });

  it("allows founders and admins through attach authorization", async () => {
    process.env.NEXUS_TERMINAL_ENABLED = "true";
    try {
      for (const role of ["founder", "admin"]) {
        const res = await fetch(`${base}/api/v1/terminal/attach`, {
          headers: { cookie: `role=${role}` },
        });
        // fetch cannot upgrade a WebSocket, so an authorised caller reaches
        // the upgrade handler and receives its explicit HTTP fallback.
        expect(res.status).toBe(400);
        expect((await res.json() as { error: string }).error).toBe("upgrade_failed");
      }
    } finally {
      delete process.env.NEXUS_TERMINAL_ENABLED;
    }
  });

  it("rejects ordinary roles before terminal attach capacity or upgrade", async () => {
    process.env.NEXUS_TERMINAL_ENABLED = "true";
    try {
      for (const role of ["member", "operator", ""]) {
        const res = await fetch(`${base}/api/v1/terminal/attach`, {
          headers: { cookie: `role=${role}` },
        });
        expect(res.status).toBe(403);
        expect((await res.json() as { error: string }).error).toBe("forbidden");
      }
    } finally {
      delete process.env.NEXUS_TERMINAL_ENABLED;
    }
  });

  it("allows founders and admins to read audit records", async () => {
    for (const role of ["founder", "admin"]) {
      const res = await fetch(`${base}/api/v1/terminal/audit`, {
        headers: { cookie: `role=${role}` },
      });
      expect(res.status).toBe(200);
    }
  });

  it("rejects ordinary roles from the audit endpoint", async () => {
    for (const role of ["member", "operator", ""]) {
      const res = await fetch(`${base}/api/v1/terminal/audit`, {
        headers: { cookie: `role=${role}` },
      });
      expect(res.status).toBe(403);
      expect((await res.json() as { error: string }).error).toBe("forbidden");
    }
  });

  it("fails closed when Auth has no session or is unavailable", async () => {
    for (const role of ["missing-session", "outage"]) {
      const res = await fetch(`${base}/api/v1/terminal/audit`, {
        headers: { cookie: `role=${role}` },
      });
      expect(res.status).toBe(401);
      expect((await res.json() as { error: string }).error).toBe("not_authenticated");
    }
  });
});
