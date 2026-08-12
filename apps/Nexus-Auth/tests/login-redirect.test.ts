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
