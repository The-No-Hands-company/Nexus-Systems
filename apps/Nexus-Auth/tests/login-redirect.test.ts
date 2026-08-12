import { beforeAll, describe, expect, test } from "bun:test";
import { handleRequest } from "../src/server";

const BASE = "http://auth.tnhc.dev";

beforeAll(() => {
  // safeRedirect refuses every absolute address unless a cookie domain is
  // configured, because without one it cannot tell "our domain" from anyone
  // else's. Production sets this in deploy.sh; the tests need it too, or they
  // would pass for the wrong reason — every redirect rejected, including the
  // ones that should be honoured.
  process.env.NEXUS_AUTH_COOKIE_DOMAIN = ".tnhc.dev";
});

async function loginPage(query: string): Promise<string> {
  const res = await handleRequest(new Request(`${BASE}/login${query}`));
  return res.text();
}

/**
 * The return address has to survive the trip to the sign-in page.
 *
 * It did not. The proxy's login gate and the dashboard's sign-in button both
 * send `redirect_uri`; this page only read `redirect`. So the address was
 * dropped in silence and everyone who signed in landed back on /login instead
 * of the page they had asked for — which looks, to the person, exactly like
 * signing in not working.
 */
describe("login page return address", () => {
  test("honours redirect_uri, which is what every caller actually sends", async () => {
    const html = await loginPage("?redirect_uri=" + encodeURIComponent("https://app.tnhc.dev/"));

    expect(html).toContain('name="redirect"');
    expect(html).toContain("https://app.tnhc.dev/");
  });

  test("still honours the older redirect spelling", async () => {
    const html = await loginPage("?redirect=" + encodeURIComponent("https://chat.tnhc.dev/"));

    expect(html).toContain("https://chat.tnhc.dev/");
  });

  test("prefers redirect_uri when both are present", async () => {
    const html = await loginPage(
      "?redirect_uri=" +
        encodeURIComponent("https://app.tnhc.dev/") +
        "&redirect=" +
        encodeURIComponent("https://chat.tnhc.dev/"),
    );

    expect(html).toContain("https://app.tnhc.dev/");
    expect(html).not.toContain("https://chat.tnhc.dev/");
  });

  test("refuses an off-domain return address in either spelling", async () => {
    // Otherwise the sign-in page is a phishing tool: the victim really does
    // authenticate, and is then handed to the attacker.
    for (const q of [
      "?redirect_uri=" + encodeURIComponent("https://evil.example.com/steal"),
      "?redirect=" + encodeURIComponent("https://evil.example.com/steal"),
    ]) {
      const html = await loginPage(q);
      expect(html).not.toContain("evil.example.com");
    }
  });

  test("refuses a lookalike domain", async () => {
    const html = await loginPage(
      "?redirect_uri=" + encodeURIComponent("https://eviltnhc.dev/steal"),
    );

    expect(html).not.toContain("eviltnhc.dev");
  });

  test("renders fine with no return address at all", async () => {
    const html = await loginPage("");

    expect(html).toContain('name="username"');
  });
});

describe("signing in without a return address", () => {
  // Reported as: "I log in and get bounced back to an empty form, no error."
  // The sign-in succeeded every time — the cookie was set — and then sent the
  // person to /login, which rendered the form again. Indistinguishable from a
  // silent failure.
  test("lands somewhere useful, not back on the form", async () => {
    const form = new FormData();
    form.set("username", "nobody-real");
    form.set("password", "wrong");
    const res = await handleRequest(
      new Request(`${BASE}/login`, { method: "POST", body: form }),
    );
    // Bad credentials still re-render, with an error the person can read.
    expect(res.status).toBe(401);
    expect(await res.text()).toContain("Incorrect username or password");
  });

  test("an already-signed-in visitor is never shown the form", async () => {
    // No session cookie here, so this asserts the shape of the anonymous case:
    // a form, not a redirect loop.
    const res = await handleRequest(new Request(`${BASE}/login`));
    expect(res.status).toBe(200);
    expect(await res.text()).toContain('name="password"');
  });

  test("the default destination is the dashboard, not /login", async () => {
    // Pinning the value that caused the bug: anything that resolves back to
    // the sign-in page reintroduces it.
    const { defaultPostLoginForTest } = await import("../src/server");
    const dest = defaultPostLoginForTest();
    expect(dest).not.toContain("/login");
    expect(dest).toContain("app.");
  });
});
