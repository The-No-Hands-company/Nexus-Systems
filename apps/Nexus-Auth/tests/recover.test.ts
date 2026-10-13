import { beforeAll, describe, expect, test } from "bun:test";
import { handleRequest } from "../src/server";
import { createUser, authenticateUser } from "../src/users";
import { issueRecoveryCodes, countRemainingRecoveryCodes } from "../src/recovery";
import { createSession, validateSession } from "../src/sessions";

/**
 * Signing in with a recovery code after losing the password. The API existed;
 * nothing a person could open used it, so codes were useless in practice.
 */

const BASE = "http://auth.tnhc.dev";
const OLD = "the old forgotten password";  // pragma: allowlist secret
const NEW = "a brand new strong password";  // pragma: allowlist secret

beforeAll(() => {
  process.env.NEXUS_AUTH_COOKIE_DOMAIN = ".tnhc.dev";
});

let counter = 0;
function freshTag(): string {
  counter += 1;
  return `recover-test-${counter}-${Date.now()}`;
}

function post(path: string, fields: Record<string, string>, tag = freshTag()): Promise<Response> {
  return handleRequest(new Request(`${BASE}${path}`, {
    method: "POST",
    headers: { "content-type": "application/x-www-form-urlencoded", "x-nexus-client-tag": tag },
    body: new URLSearchParams(fields).toString(),
  }));
}

function account() {
  const username = `rec${Date.now().toString(36)}${Math.floor(Math.random() * 1e6).toString(36)}`;
  const user = createUser({ username, email: `${username}@example.com`, password: OLD, role: "user" });
  const codes = issueRecoveryCodes(user.id);
  return { user, username, email: `${username}@example.com`, codes };
}

function grantFrom(html: string): string {
  const grant = html.match(/name="grant" value="([^"]+)"/)?.[1];
  if (!grant) throw new Error("no reset grant in page");
  return grant;
}

describe("the recover page", () => {
  test("should render a script-free form under the onboarding policy", async () => {
    const res = await handleRequest(new Request(`${BASE}/recover`));
    expect(res.status).toBe(200);
    expect(res.headers.get("content-security-policy")).toContain("default-src 'none'");
    expect(res.headers.get("cache-control")).toContain("no-store");
    const html = await res.text();
    expect(html).toContain('action="/recover"');
    expect(html).not.toContain("<script");
  });

  test("the login page should link to it", async () => {
    expect(await (await handleRequest(new Request(`${BASE}/login`))).text()).toContain('href="/recover"');
  });
});

describe("recovering an account, when the code is right", () => {
  test("should spend the code, set the new password, end every other session and sign in", async () => {
    const { user, username, email, codes } = account();
    const elsewhere = createSession({ userId: user.id });

    const step1 = await post("/recover", { email, code: codes[0]! });
    expect(step1.status).toBe(200);
    expect(countRemainingRecoveryCodes(user.id)).toBe(9);
    const grant = grantFrom(await step1.text());

    const step2 = await post("/recover/password", { grant, password: NEW, confirm: NEW });
    expect(step2.status).toBe(200);
    expect(step2.headers.get("set-cookie") ?? "").toContain("HttpOnly");
    expect(await step2.text()).toContain("9 recovery codes left");

    expect(authenticateUser(username, NEW)).not.toBeNull();
    expect(authenticateUser(username, OLD)).toBeNull();
    expect(validateSession(elsewhere.token)).toBeFalsy();
  });

  test("the reset step should work once only", async () => {
    const { email, codes } = account();
    const grant = grantFrom(await (await post("/recover", { email, code: codes[0]! })).text());
    expect((await post("/recover/password", { grant, password: NEW, confirm: NEW })).status).toBe(200);
    expect((await post("/recover/password", { grant, password: NEW, confirm: NEW })).status).toBe(400);
  });

  test("a mismatched or short new password should be refused and the step stay usable", async () => {
    const { email, codes } = account();
    const grant = grantFrom(await (await post("/recover", { email, code: codes[0]! })).text());
    const mismatch = await post("/recover/password", { grant, password: NEW, confirm: `${NEW}!` });
    expect(mismatch.status).toBe(400);
    expect(await mismatch.text()).toContain("do not match");
    expect((await post("/recover/password", { grant, password: "short", confirm: "short" })).status).toBe(400);  // pragma: allowlist secret
    expect((await post("/recover/password", { grant, password: NEW, confirm: NEW })).status).toBe(200);
  });
});

describe("recovering an account, when the input is wrong", () => {
  test("a wrong code or unknown email should give the same answer and spend nothing", async () => {
    const { user, email } = account();
    const wrongCode = await post("/recover", { email, code: "0".repeat(32) });
    const unknown = await post("/recover", { email: "nobody@example.com", code: "0".repeat(32) });
    expect(wrongCode.status).toBe(400);
    expect(unknown.status).toBe(400);
    const a = (await wrongCode.text()).match(/role="alert">([^<]+)</)?.[1];
    const b = (await unknown.text()).match(/role="alert">([^<]+)</)?.[1];
    expect(a).toBeTruthy();
    expect(a).toBe(b);
    expect(countRemainingRecoveryCodes(user.id)).toBe(10);
  });

  test("a forged reset grant should be refused", async () => {
    const res = await post("/recover/password", { grant: "forged", password: NEW, confirm: NEW });
    expect(res.status).toBe(400);
  });

  test("repeated wrong codes should be rate limited", async () => {
    const tag = freshTag();
    for (let i = 0; i < 5; i++) await post("/recover", { email: "x@example.com", code: "bad" }, tag);
    const limited = await post("/recover", { email: "x@example.com", code: "bad" }, tag);
    expect(limited.status).toBe(429);
  });
});
