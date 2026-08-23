import { describe, it, expect } from "vitest";

// Import the function under test. Since matchRedirectPattern is not exported
// from hostRouter.ts (it's an internal helper), we test an identical copy here.
// If the implementation changes, this copy must stay in sync.

function matchRedirectPattern(reqPath: string, reqQuery: string, pattern: string): Record<string, string> | null {
  if (pattern.startsWith("^")) {
    try {
      const re = new RegExp(pattern, "i");
      const full = reqQuery ? `${reqPath}?${reqQuery}` : reqPath;
      const m = full.match(re);
      if (!m) return null;
      return m.groups ?? {};
    } catch { return null; }
  }

  const qMark = pattern.indexOf("?");
  const pathPattern  = qMark === -1 ? pattern : pattern.slice(0, qMark);
  const queryPattern = qMark === -1 ? null : pattern.slice(qMark + 1);

  const normReq = reqPath.endsWith("/") && reqPath !== "/" ? reqPath.slice(0, -1) : reqPath;
  const normPat = pathPattern.endsWith("/") && pathPattern !== "/" ? pathPattern.slice(0, -1) : pathPattern;
  const params: Record<string, string> = {};

  if (normPat.endsWith("/*")) {
    const prefix = normPat.slice(0, -2);
    if (normReq !== prefix && !normReq.startsWith(prefix + "/")) return null;
    params["*"] = normReq.slice(prefix.length + 1);
  } else {
    const patParts = normPat.split("/");
    const reqParts = normReq.split("/");
    if (patParts.length !== reqParts.length) return null;
    for (let i = 0; i < patParts.length; i++) {
      if (patParts[i]!.startsWith(":")) {
        params[patParts[i]!.slice(1)] = decodeURIComponent(reqParts[i]!);
      } else if (patParts[i] !== reqParts[i]) {
        return null;
      }
    }
  }

  if (queryPattern) {
    const actualQuery = new URLSearchParams(reqQuery);
    for (const rule of queryPattern.split("&")) {
      if (rule.startsWith("!")) {
        if (actualQuery.has(rule.slice(1))) return null;
      } else {
        const eqIdx = rule.indexOf("=");
        if (eqIdx === -1) {
          if (!actualQuery.has(rule)) return null;
        } else {
          const key = rule.slice(0, eqIdx);
          const val = rule.slice(eqIdx + 1);
          const actual = actualQuery.get(key);
          if (actual === null) return null;
          if (val !== "*" && val !== actual) return null;
          if (val === "*") params[`q_${key}`] = actual;
        }
      }
    }
  }

  return params;
}

describe("redirect pattern matching — path only (backwards compat)", () => {
  it("matches exact path", () => expect(matchRedirectPattern("/about", "", "/about")).not.toBeNull());
  it("rejects wrong path",  () => expect(matchRedirectPattern("/about", "", "/contact")).toBeNull());

  it("captures :param segments", () => {
    const m = matchRedirectPattern("/blog/hello-world", "", "/blog/:slug");
    expect(m?.slug).toBe("hello-world");
  });

  it("captures splat wildcard", () => {
    const m = matchRedirectPattern("/old/a/b/c", "", "/old/*");
    expect(m?.["*"]).toBe("a/b/c");
  });

  it("splat matches empty suffix", () => {
    const m = matchRedirectPattern("/old", "", "/old/*");
    expect(m).not.toBeNull();
  });
});

describe("redirect pattern matching — query strings", () => {
  it("matches when required key is present", () => {
    expect(matchRedirectPattern("/page", "ref=email", "/page?ref")).not.toBeNull();
  });

  it("rejects when required key is absent", () => {
    expect(matchRedirectPattern("/page", "", "/page?ref")).toBeNull();
  });

  it("matches exact key=value", () => {
    expect(matchRedirectPattern("/page", "utm_source=email", "/page?utm_source=email")).not.toBeNull();
  });

  it("rejects mismatched key=value", () => {
    expect(matchRedirectPattern("/page", "utm_source=twitter", "/page?utm_source=email")).toBeNull();
  });

  it("matches wildcard key=*", () => {
    const m = matchRedirectPattern("/page", "ref=newsletter-42", "/page?ref=*");
    expect(m).not.toBeNull();
    expect(m?.q_ref).toBe("newsletter-42");
  });

  it("negation: rejects when key is present", () => {
    expect(matchRedirectPattern("/page", "logged_in=true", "/page?!logged_in")).toBeNull();
  });

  it("negation: matches when key is absent", () => {
    expect(matchRedirectPattern("/page", "other=x", "/page?!logged_in")).not.toBeNull();
  });

  it("multiple query constraints: all must match", () => {
    const pattern = "/page?utm_source=email&!logged_in";
    expect(matchRedirectPattern("/page", "utm_source=email", pattern)).not.toBeNull();
    expect(matchRedirectPattern("/page", "utm_source=email&logged_in=1", pattern)).toBeNull();
    expect(matchRedirectPattern("/page", "utm_source=twitter", pattern)).toBeNull();
  });
});

describe("redirect pattern matching — regex", () => {
  it("matches regex against full path", () => {
    expect(matchRedirectPattern("/blog/2024/post", "", "^/blog/\\d{4}/")).not.toBeNull();
  });

  it("rejects non-matching regex", () => {
    expect(matchRedirectPattern("/about", "", "^/blog/")).toBeNull();
  });

  it("regex includes query string", () => {
    expect(matchRedirectPattern("/search", "q=hello", "^/search\\?q=")).not.toBeNull();
  });

  it("named capture groups are returned", () => {
    const m = matchRedirectPattern("/posts/123", "", "^/posts/(?<id>\\d+)$");
    expect(m?.id).toBe("123");
  });

  it("invalid regex returns null gracefully", () => {
    expect(matchRedirectPattern("/page", "", "^[invalid")).toBeNull();
  });
});

describe("redirect pattern matching — combined path + query", () => {
  it("path :param + query filter", () => {
    const m = matchRedirectPattern("/products/42", "variant=red", "/products/:id?variant=*");
    expect(m?.id).toBe("42");
    expect(m?.q_variant).toBe("red");
  });

  it("splat + query filter", () => {
    const m = matchRedirectPattern("/docs/v2/setup", "lang=en", "/docs/*?lang=en");
    expect(m?.["*"]).toBe("v2/setup");
  });

  it("rejects when path matches but query does not", () => {
    expect(matchRedirectPattern("/products/42", "", "/products/:id?variant=*")).toBeNull();
  });
});
