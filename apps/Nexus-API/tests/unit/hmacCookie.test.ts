import { describe, it, expect, beforeEach, vi } from "vitest";
import crypto from "crypto";

// Re-implement the HMAC cookie logic here so we can test it in isolation
// This mirrors the exact implementation in hostRouter.ts and access.ts
function signUnlockCookie(siteId: number, secret: string): string {
  const issuedAt = Math.floor(Date.now() / 1000).toString();
  const payload = `${siteId}:${issuedAt}`;
  const hmac = crypto.createHmac("sha256", secret).update(payload).digest("base64url");
  return `${Buffer.from(payload).toString("base64url")}.${hmac}`;
}

function verifyUnlockCookie(cookieValue: string | undefined, siteId: number, secret: string): boolean {
  if (!cookieValue) return false;
  try {
    const [encodedPayload, hmac] = cookieValue.split(".");
    if (!encodedPayload || !hmac) return false;
    const payload = Buffer.from(encodedPayload, "base64url").toString();
    const [cookieSiteId, issuedAt] = payload.split(":");
    if (parseInt(cookieSiteId, 10) !== siteId) return false;
    const age = Math.floor(Date.now() / 1000) - parseInt(issuedAt, 10);
    if (age > 86400) return false;
    const expectedHmac = crypto.createHmac("sha256", secret).update(payload).digest("base64url");
    return crypto.timingSafeEqual(Buffer.from(hmac), Buffer.from(expectedHmac));
  } catch {
    return false;
  }
}

const SECRET = "test-secret-key-for-unit-tests-only"; // pragma: allowlist secret — unit-test fixture

describe("HMAC unlock cookie", () => {
  it("valid cookie verifies correctly", () => {
    const cookie = signUnlockCookie(42, SECRET);
    expect(verifyUnlockCookie(cookie, 42, SECRET)).toBe(true);
  });

  it("wrong site ID is rejected", () => {
    const cookie = signUnlockCookie(42, SECRET);
    expect(verifyUnlockCookie(cookie, 99, SECRET)).toBe(false);
  });

  it("wrong secret is rejected", () => {
    const cookie = signUnlockCookie(42, SECRET);
    expect(verifyUnlockCookie(cookie, 42, "wrong-secret")).toBe(false);
  });

  it("undefined cookie is rejected", () => {
    expect(verifyUnlockCookie(undefined, 42, SECRET)).toBe(false);
  });

  it("empty string is rejected", () => {
    expect(verifyUnlockCookie("", 42, SECRET)).toBe(false);
  });

  it("missing dot separator is rejected", () => {
    expect(verifyUnlockCookie("nodothere", 42, SECRET)).toBe(false);
  });

  it("truncated HMAC is rejected", () => {
    const cookie = signUnlockCookie(42, SECRET);
    const truncated = cookie.slice(0, -4);
    expect(verifyUnlockCookie(truncated, 42, SECRET)).toBe(false);
  });

  it("bit-flipped HMAC is rejected", () => {
    const cookie = signUnlockCookie(42, SECRET);
    const [payload, hmac] = cookie.split(".");
    // Flip the first char of the HMAC
    const flipped = String.fromCharCode(hmac.charCodeAt(0) ^ 1) + hmac.slice(1);
    expect(verifyUnlockCookie(`${payload}.${flipped}`, 42, SECRET)).toBe(false);
  });

  it("expired cookie (>24h) is rejected", () => {
    const issuedAt = Math.floor(Date.now() / 1000) - 86401; // 1 second past expiry
    const payload = `42:${issuedAt}`;
    const hmac = crypto.createHmac("sha256", SECRET).update(payload).digest("base64url");
    const cookie = `${Buffer.from(payload).toString("base64url")}.${hmac}`;
    expect(verifyUnlockCookie(cookie, 42, SECRET)).toBe(false);
  });

  it("just-issued cookie is valid", () => {
    const cookie = signUnlockCookie(42, SECRET);
    expect(verifyUnlockCookie(cookie, 42, SECRET)).toBe(true);
  });

  it("cookie for site 0 does not match site 1", () => {
    const cookie = signUnlockCookie(0, SECRET);
    expect(verifyUnlockCookie(cookie, 1, SECRET)).toBe(false);
  });

  it("different sites get different cookies for same timestamp", () => {
    // Mock Date.now for determinism
    const now = Date.now();
    vi.spyOn(Date, "now").mockReturnValue(now);
    const cookie42 = signUnlockCookie(42, SECRET);
    const cookie43 = signUnlockCookie(43, SECRET);
    vi.restoreAllMocks();
    expect(cookie42).not.toBe(cookie43);
  });
});
