import { afterAll, describe, expect, it } from "bun:test";
import { createPublicKey, createSign, generateKeyPairSync } from "node:crypto";

const keyPair = generateKeyPairSync("rsa", { modulusLength: 2048 });
const jwk = createPublicKey(keyPair.privateKey).export({ format: "jwk" }) as { [key: string]: unknown };
const jwksServer = Bun.serve({
  port: 0,
  fetch: () => Response.json({ keys: [{ ...jwk, kid: "calendar-test-key", alg: "RS256", use: "sig" }] }),
});

process.env.NEXUS_AUTH_INTERNAL_URL = `http://127.0.0.1:${jwksServer.port}`;
process.env.NEXUS_CALENDAR_JWT_AUDIENCE = "calendar.tnhc.dev";
process.env.NEXUS_CALENDAR_DASHBOARD_SECRET = "calendar-test-dashboard-secret"; // pragma: allowlist secret

const { resolveCaller } = await import("../src/auth");

afterAll(() => jwksServer.stop());

function token(payload: Record<string, unknown>, header: Record<string, unknown> = {}): string {
  const encodedHeader = Buffer.from(JSON.stringify({ alg: "RS256", kid: "calendar-test-key", ...header })).toString("base64url");
  const encodedPayload = Buffer.from(JSON.stringify({
    sub: "usr-alice",
    aud: "calendar.tnhc.dev",
    exp: Math.floor(Date.now() / 1000) + 300,
    ...payload,
  })).toString("base64url");
  const signer = createSign("RSA-SHA256");
  signer.update(`${encodedHeader}.${encodedPayload}`);
  signer.end();
  return `${encodedHeader}.${encodedPayload}.${signer.sign(keyPair.privateKey).toString("base64url")}`;
}

describe("resolveCaller", () => {
  it("ignores a browser-supplied subject without the Dashboard hop secret", async () => {
    const caller = await resolveCaller(new Request("http://calendar.test", {
      headers: { "x-nexus-subject": "usr-victim" },
    }));

    expect(caller).toBeNull();
  });

  it("accepts a Dashboard subject only when its private hop secret matches", async () => {
    const caller = await resolveCaller(new Request("http://calendar.test", {
      headers: {
        "x-nexus-subject": "usr-alice",
        "x-nexus-dashboard-secret": "calendar-test-dashboard-secret", // pragma: allowlist secret
      },
    }));

    expect(caller).toEqual({ subject: "usr-alice" });
  });

  it("resolves the subject from a valid Calendar-audience RS256 identity JWT", async () => {
    const caller = await resolveCaller(new Request("http://calendar.test", {
      headers: { "x-nexus-identity": token({ sub: "usr-jwt" }) },
    }));

    expect(caller).toEqual({ subject: "usr-jwt" });
  });

  it.each([
    ["wrong audience", token({ aud: "nexus-dashboard" })],
    ["expired", token({ exp: Math.floor(Date.now() / 1000) - 1 })],
    ["wrong algorithm", token({}, { alg: "HS256" })],
    ["invalid signature", `${token({}).split(".").slice(0, 2).join(".")}.invalid`],
  ])("rejects a %s identity JWT", async (_caseName, identity) => {
    const caller = await resolveCaller(new Request("http://calendar.test", {
      headers: { "x-nexus-identity": identity },
    }));

    expect(caller).toBeNull();
  });
});
