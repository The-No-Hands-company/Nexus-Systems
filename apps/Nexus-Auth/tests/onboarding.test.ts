import { beforeAll, describe, expect, test } from "bun:test";
import { handleRequest } from "../src/server";
import { createInvite } from "../src/invites";
import { findUserByEmail, setUserStatus, authenticateUser } from "../src/users";

/**
 * The three ways into the platform for someone who has no account yet:
 * request access, claim an approved request, redeem an invite.
 *
 * All three used to live on app.tnhc.dev, behind the sign-in gate, so a
 * newcomer was bounced to a login form for an account they did not have yet —
 * and invites had no page at all. They are served by Auth now, on the one host
 * the gate keeps public, as plain forms with no script.
 */

const BASE = "http://auth.tnhc.dev";
const STRONG = "correct horse battery staple";

beforeAll(() => {
  process.env.NEXUS_AUTH_COOKIE_DOMAIN = ".tnhc.dev";
});

let tagCounter = 0;
/** A fresh client tag per call keeps the rate limiter out of unrelated tests. */
function freshTag(): string {
  tagCounter += 1;
  return `onboarding-test-${tagCounter}-${Date.now()}`;
}

function post(path: string, fields: Record<string, string>, tag = freshTag()): Promise<Response> {
  return handleRequest(new Request(`${BASE}${path}`, {
    method: "POST",
    headers: { "content-type": "application/x-www-form-urlencoded", "x-nexus-client-tag": tag },
    body: new URLSearchParams(fields).toString(),
  }));
}

function unique(prefix: string): string {
  return `${prefix}${Date.now().toString(36)}${Math.floor(Math.random() * 1e6).toString(36)}`;
}

describe("onboarding pages, when loaded by someone without an account", () => {
  for (const path of ["/request", "/claim", "/join"]) {
    test(`${path} should render a script-free form under a strict policy`, async () => {
      const res = await handleRequest(new Request(`${BASE}${path}`));
      expect(res.status).toBe(200);
      expect(res.headers.get("content-type")).toContain("text/html");
      const csp = res.headers.get("content-security-policy") ?? "";
      expect(csp).toContain("default-src 'none'");
      expect(csp).toContain("form-action 'self'");
      expect(csp).not.toContain("script-src");
      expect(res.headers.get("cache-control")).toContain("no-store");
      expect(res.headers.get("referrer-policy")).toBe("no-referrer");
      const html = await res.text();
      expect(html).toContain(`action="${path}"`);
      expect(html).not.toContain("<script");
    });
  }

  test("join should prefill the invite code from the link, escaped", async () => {
    const res = await handleRequest(new Request(`${BASE}/join?code=${encodeURIComponent('"><script>x</script>')}`));
    const html = await res.text();
    expect(html).not.toContain("<script>x</script>");
    expect(html).toContain("&quot;&gt;&lt;script&gt;");
  });

  test("claim should prefill the email from the link", async () => {
    const res = await handleRequest(new Request(`${BASE}/claim?email=${encodeURIComponent("ada@example.com")}`));
    expect(await res.text()).toContain('value="ada@example.com"');
  });

  test("the login page should point newcomers at all three ways in", async () => {
    const html = await (await handleRequest(new Request(`${BASE}/login`))).text();
    for (const path of ['href="/request"', 'href="/claim"', 'href="/join"']) expect(html).toContain(path);
  });
});

describe("request then claim, when an operator approves in between", () => {
  test("should create the account, sign the person in and show recovery codes once", async () => {
    const username = unique("claimer");
    const email = `${username}@example.com`;

    const requested = await post("/request", { username, email, note: "hello" });
    expect(requested.status).toBe(201);
    const requestedHtml = await requested.text();
    const code = requestedHtml.match(/data-claim-code="([^"]+)"/)?.[1];
    expect(code).toBeTruthy();
    expect(requestedHtml).toContain(`/claim?email=${encodeURIComponent(email)}`);
    expect(requested.headers.get("cache-control")).toContain("no-store");

    const user = findUserByEmail(email)!;
    expect(user.status).toBe("pending");

    // Not approved yet: claiming must say so, not create anything.
    const early = await post("/claim", { email, claimCode: code!, password: STRONG, confirm: STRONG });
    expect(early.status).toBe(400);
    expect(await early.text()).toContain("not been approved");

    setUserStatus(user.id, "approved");

    const claimed = await post("/claim", { email, claimCode: code!, password: STRONG, confirm: STRONG });
    expect(claimed.status).toBe(200);
    expect(claimed.headers.get("set-cookie") ?? "").toContain("HttpOnly");
    expect(claimed.headers.get("cache-control")).toContain("no-store");
    const claimedHtml = await claimed.text();
    expect(claimedHtml.match(/data-recovery-code/g)?.length ?? 0).toBeGreaterThan(0);
    expect(authenticateUser(username, STRONG)).not.toBeNull();

    const again = await post("/claim", { email, claimCode: code!, password: STRONG, confirm: STRONG });
    expect(again.status).toBe(400);
  });

  test("request should refuse a username that is taken, without revealing anything else", async () => {
    const username = unique("dupe");
    expect((await post("/request", { username, email: `${username}@example.com` })).status).toBe(201);
    const second = await post("/request", { username, email: `${username}-2@example.com` });
    expect(second.status).toBe(409);
    expect(await second.text()).not.toContain("data-claim-code");
  });

  test("request should refuse missing fields", async () => {
    expect((await post("/request", { username: "", email: "" })).status).toBe(400);
  });
});

describe("claim, when the input is wrong", () => {
  test("mismatched passwords should be refused before the code is spent", async () => {
    const username = unique("mismatch");
    const email = `${username}@example.com`;
    const code = (await (await post("/request", { username, email })).text()).match(/data-claim-code="([^"]+)"/)![1]!;
    setUserStatus(findUserByEmail(email)!.id, "approved");

    const res = await post("/claim", { email, claimCode: code, password: STRONG, confirm: `${STRONG}!` });
    expect(res.status).toBe(400);
    expect(await res.text()).toContain("do not match");
    // The code still works afterwards.
    expect((await post("/claim", { email, claimCode: code, password: STRONG, confirm: STRONG })).status).toBe(200);
  });

  test("a short password should be refused with the minimum stated", async () => {
    const res = await post("/claim", { email: "x@example.com", claimCode: "x", password: "short", confirm: "short" });  // pragma: allowlist secret
    expect(res.status).toBe(400);
    expect(await res.text()).toContain("12 characters");
  });

  test("repeated wrong codes should be rate limited", async () => {
    const tag = freshTag();
    for (let i = 0; i < 5; i++) {
      expect((await post("/claim", { email: "nobody@example.com", claimCode: "wrong", password: STRONG, confirm: STRONG }, tag)).status).toBe(400);
    }
    const limited = await post("/claim", { email: "nobody@example.com", claimCode: "wrong", password: STRONG, confirm: STRONG }, tag);
    expect(limited.status).toBe(429);
    expect(limited.headers.get("retry-after")).toBeTruthy();
  });
});

describe("join, when redeeming an invite", () => {
  test("a valid invite should create an active account, sign in and show recovery codes", async () => {
    const { code } = createInvite("usr-test-operator");
    const username = unique("joiner");
    const res = await post("/join", { code, username, email: `${username}@example.com`, password: STRONG, confirm: STRONG });
    expect(res.status).toBe(201);
    expect(res.headers.get("set-cookie") ?? "").toContain("HttpOnly");
    expect((await res.text()).match(/data-recovery-code/g)?.length ?? 0).toBeGreaterThan(0);
    expect(authenticateUser(username, STRONG)).not.toBeNull();

    const reuse = await post("/join", { code, username: unique("other"), email: `${unique("o")}@example.com`, password: STRONG, confirm: STRONG });
    expect(reuse.status).toBe(400);
    expect(await reuse.text()).toContain("not valid");
  });

  test("a taken username should be refused and the invite stay usable", async () => {
    const { code } = createInvite("usr-test-operator");
    const taken = unique("taken");
    await post("/join", { code: createInvite("usr-test-operator").code, username: taken, email: `${taken}@example.com`, password: STRONG, confirm: STRONG });

    const clash = await post("/join", { code, username: taken, email: `${unique("e")}@example.com`, password: STRONG, confirm: STRONG });
    expect(clash.status).toBe(400);
    expect(await clash.text()).toContain("username is taken");

    const fresh = unique("fresh");
    expect((await post("/join", { code, username: fresh, email: `${fresh}@example.com`, password: STRONG, confirm: STRONG })).status).toBe(201);
  });

  test("a signed-in visitor should be sent on rather than shown a sign-up form", async () => {
    const { code } = createInvite("usr-test-operator");
    const username = unique("signedin");
    const joined = await post("/join", { code, username, email: `${username}@example.com`, password: STRONG, confirm: STRONG });
    const cookie = (joined.headers.get("set-cookie") ?? "").split(";")[0]!;
    const res = await handleRequest(new Request(`${BASE}/join`, { headers: { cookie } }));
    expect(res.status).toBe(303);
  });
});
