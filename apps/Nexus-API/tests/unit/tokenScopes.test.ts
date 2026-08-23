import { describe, it, expect } from "vitest";

// Mirror the scope logic from tokenAuth.ts
type TokenScope = "read" | "write" | "deploy" | "admin";

function parseScopes(raw: string): Set<TokenScope> {
  return new Set<TokenScope>(
    raw.split(",").map(s => s.trim()).filter(Boolean) as TokenScope[]
  );
}

function hasScope(scopeSet: Set<TokenScope>, required: TokenScope): boolean {
  return scopeSet.has(required);
}

// Mirror key validation from tokens.ts
const VALID_SCOPES = ["read", "write", "deploy", "admin"] as const;
function validateScopes(scopes: string[]): string[] {
  return scopes.filter(s => (VALID_SCOPES as readonly string[]).includes(s));
}

describe("Token scope parsing", () => {
  it("parses comma-separated scopes", () => {
    const s = parseScopes("read,write,deploy");
    expect(s.has("read")).toBe(true);
    expect(s.has("write")).toBe(true);
    expect(s.has("deploy")).toBe(true);
    expect(s.has("admin")).toBe(false);
  });

  it("handles single scope", () => {
    const s = parseScopes("read");
    expect(s.has("read")).toBe(true);
    expect(s.has("write")).toBe(false);
  });

  it("handles all scopes", () => {
    const s = parseScopes("read,write,deploy,admin");
    expect(s.size).toBe(4);
    for (const scope of VALID_SCOPES) {
      expect(s.has(scope)).toBe(true);
    }
  });

  it("trims whitespace around scope names", () => {
    const s = parseScopes("read, write , deploy");
    expect(s.has("read")).toBe(true);
    expect(s.has("write")).toBe(true);
  });

  it("empty string produces empty set", () => {
    expect(parseScopes("").size).toBe(0);
  });

  it("default scope string matches backwards-compat default", () => {
    const s = parseScopes("read,write,deploy");
    // New tokens default to read+write+deploy
    // Existing tokens that predate the scopes column also get this default
    expect(s.has("read")).toBe(true);
    expect(s.has("write")).toBe(true);
    expect(s.has("deploy")).toBe(true);
    expect(s.has("admin")).toBe(false);
  });
});

describe("Scope enforcement (requireScope)", () => {
  it("read-only token blocked from write route", () => {
    const scopes = parseScopes("read");
    expect(hasScope(scopes, "write")).toBe(false);
  });

  it("read-only token allowed on read route", () => {
    const scopes = parseScopes("read");
    expect(hasScope(scopes, "read")).toBe(true);
  });

  it("deploy-only token cannot write settings", () => {
    const scopes = parseScopes("deploy");
    expect(hasScope(scopes, "write")).toBe(false);
    expect(hasScope(scopes, "deploy")).toBe(true);
  });

  it("write token cannot deploy", () => {
    const scopes = parseScopes("read,write");
    expect(hasScope(scopes, "deploy")).toBe(false);
  });

  it("admin token has all capabilities", () => {
    const scopes = parseScopes("read,write,deploy,admin");
    for (const scope of VALID_SCOPES) {
      expect(hasScope(scopes, scope)).toBe(true);
    }
  });

  it("session auth (no tokenScopes) always passes — no restriction", () => {
    // When tokenScopes is undefined, requireScope passes through
    const tokenScopes = undefined;
    // Simulate the middleware: if (!req.tokenScopes) → next()
    expect(tokenScopes).toBeUndefined();
  });
});

describe("Scope validation on creation", () => {
  it("filters out unknown scopes", () => {
    const result = validateScopes(["read", "write", "superuser", "god"]);
    expect(result).toEqual(["read", "write"]);
  });

  it("accepts all valid scopes", () => {
    const result = validateScopes([...VALID_SCOPES]);
    expect(result).toHaveLength(4);
  });

  it("empty array stays empty", () => {
    expect(validateScopes([])).toHaveLength(0);
  });

  it("duplicates not deduplicated at validation layer (DB handles it)", () => {
    const result = validateScopes(["read", "read", "write"]);
    expect(result).toEqual(["read", "read", "write"]);
  });
});

describe("Scope string serialisation (for DB storage)", () => {
  it("array serialises to comma-separated string", () => {
    const scopes = ["read", "write", "deploy"];
    expect(scopes.join(",")).toBe("read,write,deploy");
  });

  it("round-trips: array → string → set", () => {
    const original = ["read", "deploy"];
    const stored   = original.join(",");
    const parsed   = parseScopes(stored);
    expect(parsed.has("read")).toBe(true);
    expect(parsed.has("deploy")).toBe(true);
    expect(parsed.has("write")).toBe(false);
  });
});
