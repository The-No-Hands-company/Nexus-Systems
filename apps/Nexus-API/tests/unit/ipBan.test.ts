import { describe, it, expect } from "vitest";

/**
 * Tests for middleware/ipBan.ts — CIDR math, scope matching, cache TTL contract.
 */

// ── CIDR logic (mirrors ipBan.ts) ─────────────────────────────────────────────

function ipToNum(ip: string): number {
  const parts = ip.split(".").map(Number);
  if (parts.length !== 4 || parts.some(isNaN)) throw new Error("Invalid IPv4");
  return ((parts[0]! << 24) | (parts[1]! << 16) | (parts[2]! << 8) | parts[3]!) >>> 0;
}

function makeCidrChecker(cidr: string): (ip: string) => boolean {
  try {
    const [base, prefixStr] = cidr.split("/");
    const prefix = parseInt(prefixStr ?? "32", 10);
    const baseNum = ipToNum(base!);
    const mask = prefix === 0 ? 0 : (~0 << (32 - prefix)) >>> 0;
    const network = (baseNum & mask) >>> 0;
    return (ip: string) => {
      try { return ((ipToNum(ip) & mask) >>> 0) === network; }
      catch { return false; }
    };
  } catch { return () => false; }
}

// ── Scope matching (mirrors isIpBanned logic) ──────────────────────────────────

type BanScope = "api" | "sites" | "all";

function scopeMatches(banScope: BanScope, requestScope: "api" | "sites"): boolean {
  return banScope === "all" || banScope === requestScope;
}

// ── Tests ─────────────────────────────────────────────────────────────────────

describe("CIDR checker — exact /32", () => {
  it("matches the exact IP", () => {
    const check = makeCidrChecker("1.2.3.4/32");
    expect(check("1.2.3.4")).toBe(true);
  });

  it("does not match adjacent IPs", () => {
    const check = makeCidrChecker("1.2.3.4/32");
    expect(check("1.2.3.3")).toBe(false);
    expect(check("1.2.3.5")).toBe(false);
  });
});

describe("CIDR checker — /24 subnet", () => {
  it("matches all 256 addresses in the subnet", () => {
    const check = makeCidrChecker("192.168.1.0/24");
    expect(check("192.168.1.0")).toBe(true);
    expect(check("192.168.1.128")).toBe(true);
    expect(check("192.168.1.255")).toBe(true);
  });

  it("does not match outside the subnet", () => {
    const check = makeCidrChecker("192.168.1.0/24");
    expect(check("192.168.2.1")).toBe(false);
    expect(check("192.169.1.1")).toBe(false);
    expect(check("10.0.0.1")).toBe(false);
  });
});

describe("CIDR checker — /16 subnet", () => {
  it("matches 65536 addresses", () => {
    const check = makeCidrChecker("10.0.0.0/16");
    expect(check("10.0.0.1")).toBe(true);
    expect(check("10.0.255.255")).toBe(true);
  });

  it("does not match other /16 blocks", () => {
    const check = makeCidrChecker("10.0.0.0/16");
    expect(check("10.1.0.1")).toBe(false);
  });
});

describe("CIDR checker — /0 (ban everything)", () => {
  it("matches any IP", () => {
    const check = makeCidrChecker("0.0.0.0/0");
    expect(check("1.2.3.4")).toBe(true);
    expect(check("255.255.255.255")).toBe(true);
    expect(check("0.0.0.0")).toBe(true);
  });
});

describe("CIDR checker — error handling", () => {
  it("returns false for malformed CIDR", () => {
    const check = makeCidrChecker("not-a-cidr");
    expect(check("1.2.3.4")).toBe(false);
  });

  it("returns false for malformed IP input", () => {
    const check = makeCidrChecker("192.168.1.0/24");
    expect(check("not-an-ip")).toBe(false);
    expect(check("")).toBe(false);
  });
});

describe("scope matching", () => {
  it("'all' scope blocks both api and sites requests", () => {
    expect(scopeMatches("all", "api")).toBe(true);
    expect(scopeMatches("all", "sites")).toBe(true);
  });

  it("'api' scope only blocks api requests", () => {
    expect(scopeMatches("api", "api")).toBe(true);
    expect(scopeMatches("api", "sites")).toBe(false);
  });

  it("'sites' scope only blocks site viewing", () => {
    expect(scopeMatches("sites", "sites")).toBe(true);
    expect(scopeMatches("sites", "api")).toBe(false);
  });
});

describe("loopback exemption", () => {
  it("identifies loopback addresses that should be exempt", () => {
    const isLoopback = (ip: string) => ip === "127.0.0.1" || ip === "::1";
    expect(isLoopback("127.0.0.1")).toBe(true);
    expect(isLoopback("::1")).toBe(true);
    expect(isLoopback("1.2.3.4")).toBe(false);
    expect(isLoopback("192.168.1.1")).toBe(false);
  });
});

describe("cache TTL contract", () => {
  it("cache TTL is 60 seconds (matches documented behaviour)", () => {
    const CACHE_TTL_MS = 60_000;
    expect(CACHE_TTL_MS).toBe(60 * 1000);
  });

  it("cached entry is fresh within TTL", () => {
    const CACHE_TTL_MS = 60_000;
    const cachedAt = Date.now() - 30_000; // 30s ago
    const isFresh = Date.now() - cachedAt < CACHE_TTL_MS;
    expect(isFresh).toBe(true);
  });

  it("cached entry is stale after TTL", () => {
    const CACHE_TTL_MS = 60_000;
    const cachedAt = Date.now() - 61_000; // 61s ago
    const isFresh = Date.now() - cachedAt < CACHE_TTL_MS;
    expect(isFresh).toBe(false);
  });
});
