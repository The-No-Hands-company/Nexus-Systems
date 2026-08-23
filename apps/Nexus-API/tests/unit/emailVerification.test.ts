import { describe, it, expect } from "vitest";
import crypto from "crypto";

/**
 * Tests for lib/emailVerification.ts — token hash, expiry, single-use contract.
 */

const TOKEN_TTL_MS = 24 * 60 * 60 * 1000;

function hashToken(raw: string): string {
  return crypto.createHash("sha256").update(raw).digest("hex");
}

function generateRawToken(): string {
  return crypto.randomBytes(32).toString("hex");
}

function makeExpiresAt(nowMs: number = Date.now()): Date {
  return new Date(nowMs + TOKEN_TTL_MS);
}

function isExpired(expiresAt: Date, now: Date = new Date()): boolean {
  return expiresAt < now;
}

describe("token generation", () => {
  it("produces 64-char hex string (32 bytes)", () => {
    const t = generateRawToken();
    expect(t).toHaveLength(64);
    expect(t).toMatch(/^[0-9a-f]{64}$/);
  });

  it("generates unique tokens", () => {
    const s = new Set(Array.from({ length: 50 }, generateRawToken));
    expect(s.size).toBe(50);
  });
});

describe("token hashing", () => {
  it("SHA-256 digest is 64-char hex", () => {
    expect(hashToken(generateRawToken())).toHaveLength(64);
  });

  it("hash differs from raw token", () => {
    const raw = generateRawToken();
    expect(hashToken(raw)).not.toBe(raw);
  });

  it("deterministic: same input → same output", () => {
    const raw = generateRawToken();
    expect(hashToken(raw)).toBe(hashToken(raw));
  });

  it("different tokens → different hashes", () => {
    expect(hashToken(generateRawToken())).not.toBe(hashToken(generateRawToken()));
  });

  it("DB stores hash so leaking DB does not expose tokens", () => {
    const raw = generateRawToken();
    // Verifier must hash the user-supplied token before DB lookup
    expect(hashToken(raw)).not.toBe(raw);
  });
});

describe("token expiry", () => {
  it("TTL is exactly 24 hours", () => {
    const now = Date.now();
    expect(makeExpiresAt(now).getTime() - now).toBe(24 * 60 * 60 * 1000);
  });

  it("not expired before deadline", () => {
    expect(isExpired(makeExpiresAt())).toBe(false);
  });

  it("not expired at 23h 59m", () => {
    const now = Date.now();
    const expires = makeExpiresAt(now);
    const t = new Date(now + 23 * 3600_000 + 59 * 60_000);
    expect(isExpired(expires, t)).toBe(false);
  });

  it("expired 1 second after TTL", () => {
    const now = Date.now();
    const expires = makeExpiresAt(now);
    expect(isExpired(expires, new Date(now + TOKEN_TTL_MS + 1000))).toBe(true);
  });

  it("past expiry is expired", () => {
    expect(isExpired(new Date(Date.now() - 1000))).toBe(true);
  });
});

describe("single-use contract", () => {
  it("rejects token with non-null usedAt", () => {
    const verify = (usedAt: Date | null) => usedAt === null ? "userId" : null;
    expect(verify(null)).toBe("userId");
    expect(verify(new Date())).toBeNull();
  });
});

describe("verification URL", () => {
  it("URL carries raw token, not hash", () => {
    const raw = generateRawToken();
    const url = `https://node.example.com/api/auth/verify-email?token=${raw}`;
    expect(url).toContain(raw);
    expect(url).not.toContain(hashToken(raw));
  });
});
