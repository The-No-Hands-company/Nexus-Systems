import { afterEach, beforeEach, describe, expect, it } from "bun:test";
import * as users from "../src/users";
import * as recovery from "../src/recovery";
import * as invites from "../src/invites";
import * as rl from "../src/ratelimit";
import { handleRequest } from "../src/server";

/**
 * Invitations and approval notices sent by email through Nexus-Email.
 *
 * Email is an enhancement: every outcome still hands the operator the link, and
 * an approval is never undone because its notice could not be sent.
 */

const PASSWORD = "correct-horse-battery-staple";  // pragma: allowlist secret
const MAIL_TOKEN = "test-mail-token";  // pragma: allowlist secret
const BASE = "http://auth.test";

type Captured = { authorization: string | null; body: Record<string, string> };

let mail: ReturnType<typeof Bun.serve> | undefined;
let captured: Captured[] = [];

/** A stand-in for Nexus-Email's system-mail endpoint answering with `status`. */
function startFakeMail(status: number) {
  captured = [];
  mail = Bun.serve({
    // Bun 1.3.12 cannot bind port 0 in the test sandbox. A dedicated
    // loopback port keeps the fake mail service deterministic and isolated.
    port: 43153,
    hostname: "127.0.0.1",
    async fetch(req) {
      if (new URL(req.url).pathname !== "/internal/v1/system-mail") return new Response("nope", { status: 404 });
      captured.push({ authorization: req.headers.get("authorization"), body: await req.json() as Record<string, string> });
      return Response.json({ status: status === 202 ? "sent" : "x" }, { status });
    },
  });
  process.env.NEXUS_EMAIL_SYSTEM_MAIL_URL = `http://127.0.0.1:${mail.port}`;
  process.env.NEXUS_EMAIL_SYSTEM_MAIL_TOKEN = MAIL_TOKEN;
}

function post(path: string, body: unknown, headers: Record<string, string> = {}) {
  return handleRequest(new Request(`${BASE}${path}`, {
    method: "POST",
    headers: { "content-type": "application/json", ...headers },
    body: JSON.stringify(body),
  }));
}

async function founderAuth(): Promise<Record<string, string>> {
  users.createUser({ username: "boss", email: "boss@x.dev", password: PASSWORD, role: "founder" });
  const res = await post("/api/v1/auth/login", { username: "boss", password: PASSWORD });
  return { authorization: `Bearer ${(await res.json() as { token: string }).token}` };
}

beforeEach(() => {
  users.clearUsers();
  recovery.clearRecoveryStore();
  invites.clearInviteStore();
  rl.resetRateLimits();
  process.env.NEXUS_AUTH_PUBLIC_URL = "https://auth.tnhc.dev";
});

afterEach(() => {
  mail?.stop(true);
  mail = undefined;
  delete process.env.NEXUS_EMAIL_SYSTEM_MAIL_URL;
  delete process.env.NEXUS_EMAIL_SYSTEM_MAIL_TOKEN;
  delete process.env.NEXUS_AUTH_PUBLIC_URL;
});

describe("sending an invite, when an email is given", () => {
  it("should send the invite template with a bound link and report sent", async () => {
    startFakeMail(202);
    const res = await post("/api/v1/auth/invites", { email: "Ada@Example.com" }, await founderAuth());
    expect(res.status).toBe(201);
    const body = await res.json() as { code: string; link: string; delivery: string; expiresAt: string };
    expect(body.delivery).toBe("sent");
    expect(body.link).toBe(`https://auth.tnhc.dev/join?code=${body.code}`);

    expect(captured).toHaveLength(1);
    expect(captured[0]!.authorization).toBe(`Bearer ${MAIL_TOKEN}`);
    expect(captured[0]!.body).toEqual({ template: "invite", to: "Ada@Example.com", link: body.link, expiresAt: body.expiresAt });

    // Bound to the address it was sent to.
    expect(invites.redeemInvite({ code: body.code, username: "eve", email: "eve@x.dev", password: PASSWORD }).ok).toBe(false);
    expect(invites.redeemInvite({ code: body.code, username: "ada", email: "ada@example.com", password: PASSWORD }).ok).toBe(true);
  });

  it("should still return the link and report failed when Nexus-Email refuses", async () => {
    startFakeMail(500);
    const res = await post("/api/v1/auth/invites", { email: "ada@example.com" }, await founderAuth());
    expect(res.status).toBe(201);
    const body = await res.json() as { link: string; delivery: string };
    expect(body.delivery).toBe("failed");
    expect(body.link).toContain("/join?code=");
  });

  it("should report failed when Nexus-Email is unreachable", async () => {
    process.env.NEXUS_EMAIL_SYSTEM_MAIL_URL = "http://127.0.0.1:1";
    process.env.NEXUS_EMAIL_SYSTEM_MAIL_TOKEN = MAIL_TOKEN;
    const body = await (await post("/api/v1/auth/invites", { email: "ada@example.com" }, await founderAuth())).json() as { delivery: string };
    expect(body.delivery).toBe("failed");
  });

  it("should report disabled, and send nothing, when no mail token is configured", async () => {
    startFakeMail(202);
    delete process.env.NEXUS_EMAIL_SYSTEM_MAIL_TOKEN;
    const body = await (await post("/api/v1/auth/invites", { email: "ada@example.com" }, await founderAuth())).json() as { delivery: string; link: string };
    expect(body.delivery).toBe("disabled");
    expect(body.link).toContain("/join?code=");
    expect(captured).toHaveLength(0);
  });

  it("should refuse an implausible email address", async () => {
    startFakeMail(202);
    for (const email of ["not-an-email", "a b@example.com", "ada@example.com\r\nBcc: x@y.z", `${"a".repeat(250)}@x.dev`]) {
      expect((await post("/api/v1/auth/invites", { email }, await founderAuth().catch(() => ({})))).status).toBe(400);
      users.clearUsers();
    }
    expect(captured).toHaveLength(0);
  });

  it("should stop at 20 emailed invites per operator per day", async () => {
    startFakeMail(202);
    const auth = await founderAuth();
    for (let i = 0; i < 20; i++) {
      expect((await post("/api/v1/auth/invites", { email: `p${i}@example.com` }, auth)).status).toBe(201);
    }
    const limited = await post("/api/v1/auth/invites", { email: "p20@example.com" }, auth);
    expect(limited.status).toBe(429);
    expect(captured).toHaveLength(20);
    // A link-only invite is not an email and is not capped by this.
    expect((await post("/api/v1/auth/invites", {}, auth)).status).toBe(201);
  });
});

describe("creating a link-only invite", () => {
  it("should send nothing and report not_requested", async () => {
    startFakeMail(202);
    const body = await (await post("/api/v1/auth/invites", {}, await founderAuth())).json() as { delivery: string; link: string };
    expect(body.delivery).toBe("not_requested");
    expect(body.link).toContain("/join?code=");
    expect(captured).toHaveLength(0);
  });
});

describe("approving an access request", () => {
  async function pendingRequest(email: string) {
    const res = await post("/api/v1/auth/access-requests", { username: `r${Date.now()}`, email });
    return (await res.json() as { user: { id: string } }).user.id;
  }

  it("should email an approval notice linking to the claim page", async () => {
    startFakeMail(202);
    const auth = await founderAuth();
    const id = await pendingRequest("ada+nexus@example.com");
    const res = await post(`/api/v1/auth/access-requests/${id}/approve`, {}, auth);
    expect(res.status).toBe(200);
    expect((await res.json() as { notice: string }).notice).toBe("sent");
    expect(captured[0]!.body).toEqual({
      template: "access-approved",
      to: "ada+nexus@example.com",
      link: "https://auth.tnhc.dev/claim?email=ada%2Bnexus%40example.com",
    });
  });

  it("should still approve when the notice cannot be sent", async () => {
    startFakeMail(500);
    const auth = await founderAuth();
    const id = await pendingRequest("ada@example.com");
    const res = await post(`/api/v1/auth/access-requests/${id}/approve`, {}, auth);
    expect(res.status).toBe(200);
    expect((await res.json() as { notice: string }).notice).toBe("failed");
    expect(users.getUser(id)?.status).toBe("approved");
  });

  it("should send nothing on rejection", async () => {
    startFakeMail(202);
    const auth = await founderAuth();
    const id = await pendingRequest("ada@example.com");
    await post(`/api/v1/auth/access-requests/${id}/reject`, {}, auth);
    expect(captured).toHaveLength(0);
  });
});
