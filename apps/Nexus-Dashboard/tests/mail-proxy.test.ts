import { afterEach, beforeEach, describe, expect, it } from "bun:test";

// Must match the other proxy tests: AUTH_INTERNAL_URL is captured into a
// top-level const the first time src/server is imported in a `bun test` run,
// so every test file sharing that module graph must agree on the value.
process.env.NEXUS_AUTH_INTERNAL_URL = "http://127.0.0.1:4399";
process.env.NEXUS_EMAIL_URL = "http://127.0.0.1:4397";
const { handleRequest } = await import("../src/server");

type BunServer = ReturnType<typeof Bun.serve>;
let auth: BunServer | null = null;
let mail: BunServer | null = null;

/** The user id the mock Auth returns, or null to be signed out. */
let authUser: string | null = "usr-alice";
/** Headers the mock mail service last saw, so a test can assert on them. */
let mailSaw: Headers | null = null;
let mailPath: string | null = null;

beforeEach(() => {
  authUser = "usr-alice";
  mailSaw = null;
  mailPath = null;

  auth = Bun.serve({
    port: 4399,
    fetch(req) {
      const u = new URL(req.url);
      if (u.pathname !== "/api/v1/auth/me") return new Response("nope", { status: 404 });
      if (authUser === null) return Response.json({ error: "unauthenticated" }, { status: 401 });
      return Response.json({ user: { id: authUser, username: "alice", role: "user" } });
    },
  });

  mail = Bun.serve({
    port: 4397,
    fetch(req) {
      mailSaw = req.headers;
      mailPath = new URL(req.url).pathname + new URL(req.url).search;
      return Response.json({ ok: true });
    },
  });
});

afterEach(() => {
  auth?.stop(true);
  mail?.stop(true);
});

describe("mail proxy", () => {
  it("refuses an unauthenticated caller before contacting mail at all", async () => {
    authUser = null;
    const res = await handleRequest(new Request("http://app.test/ipa/mail/folders"));
    expect(res.status).toBe(401);
    expect(mailSaw).toBeNull();
  });

  it("forwards the caller's subject as told by Auth", async () => {
    await handleRequest(new Request("http://app.test/ipa/mail/folders"));
    expect(mailSaw?.get("x-nexus-subject")).toBe("usr-alice");
  });

  it("ignores an X-Nexus-Subject the browser supplied", async () => {
    // The one that matters. The mail service trusts this header to decide whose
    // mailbox to open, so if a caller could set it, any signed-in user could
    // read anyone's mail by adding one header.
    await handleRequest(
      new Request("http://app.test/ipa/mail/folders", {
        headers: { "x-nexus-subject": "usr-victim" },
      }),
    );
    expect(mailSaw?.get("x-nexus-subject")).toBe("usr-alice");
  });

  it("refuses a path outside the mail API's surface", async () => {
    const res = await handleRequest(new Request("http://app.test/ipa/mail/../admin"));
    expect(res.status).toBe(404);
    expect(mailSaw).toBeNull();
  });

  it("allows mailbox-scoped permanent delete", async () => {
    await handleRequest(
      new Request("http://app.test/ipa/mail/messages/abc", { method: "DELETE" }),
    );
    expect(mailPath).toBe("/api/v1/messages/abc");
  });

  it("allows empty-trash endpoint and still enforces route methods", async () => {
    await handleRequest(
      new Request("http://app.test/ipa/mail/folders/trash/empty", { method: "POST" }),
    );
    expect(mailPath).toBe("/api/v1/folders/trash/empty");

    mailPath = null;
    mailSaw = null;
    const res = await handleRequest(
      new Request("http://app.test/ipa/mail/folders/trash/empty", { method: "GET" }),
    );
    expect(res.status).toBe(405);
    expect(mailSaw).toBeNull();
  });

  it("passes the query string through for search", async () => {
    await handleRequest(new Request("http://app.test/ipa/mail/search?q=invoice"));
    expect(mailPath).toBe("/api/v1/search?q=invoice");
  });

  it("degrades rather than 500s when mail is down", async () => {
    // A shell page must stay usable when one service behind it is not.
    mail?.stop(true);
    mail = null;
    const res = await handleRequest(new Request("http://app.test/ipa/mail/folders"));
    expect(res.status).toBe(503);
    expect(await res.json()).toEqual({ error: "mail_unavailable" });
  });

  it("returns a correlation id and logs only safe mail proxy diagnostics", async () => {
    const originalInfo = console.info;
    const calls: unknown[][] = [];
    console.info = (...args: unknown[]) => {
      calls.push(args);
    };

    try {
      const res = await handleRequest(
        new Request("http://app.test/ipa/mail/search?q=invoice", {
          headers: {
            cookie: "nexus_session=abc123",
            authorization: "Bearer secret",
            "x-nexus-subject": "usr-victim",
          },
        }),
      );

      expect(res.status).toBe(200);
      const requestId = res.headers.get("x-request-id");
      expect(typeof requestId).toBe("string");
      expect(requestId?.length).toBeGreaterThan(0);

      const logCall = calls.find((args) => args[0] === "[mail-proxy]");
      expect(logCall).toBeDefined();
      expect(logCall?.[1]).toEqual({
        requestId,
        method: "GET",
        status: 200,
      });

      const logPayload = JSON.stringify(logCall?.[1] ?? {});
      for (const forbidden of [
        "cookie",
        "authorization",
        "x-nexus-subject",
        "messageId",
        "message_id",
        "invoice",
        "body",
        "usr-alice",
      ]) {
        expect(logPayload).not.toContain(forbidden);
      }
    } finally {
      console.info = originalInfo;
    }
  });

  it("correlates rate-limited Mail responses", async () => {
    const originalInfo = console.info;
    const calls: unknown[][] = [];
    console.info = (...args: unknown[]) => {
      calls.push(args);
    };

    try {
      let response: Response | undefined;
      for (let attempt = 0; attempt < 200; attempt += 1) {
        response = await handleRequest(
          new Request("http://app.test/ipa/mail/folders", {
            headers: { "x-nexus-client-tag": "mail-rate-limit-test" },
          }),
        );
        if (response.status === 429) break;
      }

      expect(response?.status).toBe(429);
      const requestId = response?.headers.get("x-request-id");
      expect(typeof requestId).toBe("string");

      const logCall = calls.find(
        (args) => args[0] === "[mail-proxy]" && (args[1] as { status?: number }).status === 429,
      );
      expect(logCall?.[1]).toEqual({
        requestId,
        method: "GET",
        status: 429,
      });
    } finally {
      console.info = originalInfo;
    }
  });
});
