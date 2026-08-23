import { describe, it, expect, beforeEach, vi } from "vitest";
import {
  getCachedSite,
  setCachedSite,
  getCachedFile,
  setCachedFile,
  invalidateSiteCache,
  getCacheStats,
} from "../../src/lib/domainCache";

// Reset module state between tests by clearing the cache
beforeEach(() => {
  // Invalidate all known sites from previous tests
  for (let i = 1; i <= 20; i++) invalidateSiteCache(i);
});

describe("domain cache", () => {
  it("returns null for uncached domain", () => {
    expect(getCachedSite("unknown.example.com")).toBeNull();
  });

  it("stores and retrieves a site", () => {
    setCachedSite({ siteId: 1, domain: "test.example.com", visibility: "public", passwordHash: null });
    const cached = getCachedSite("test.example.com");
    expect(cached).not.toBeNull();
    expect(cached!.siteId).toBe(1);
    expect(cached!.visibility).toBe("public");
  });

  it("returns null after TTL expires", () => {
    const now = Date.now();
    vi.spyOn(Date, "now").mockReturnValue(now);
    setCachedSite({ siteId: 2, domain: "ttl.example.com", visibility: "public", passwordHash: null });
    // Advance time past TTL (default 5 min)
    vi.spyOn(Date, "now").mockReturnValue(now + 5 * 60 * 1000 + 1);
    expect(getCachedSite("ttl.example.com")).toBeNull();
    vi.restoreAllMocks();
  });

  it("invalidateSiteCache removes the domain entry", () => {
    setCachedSite({ siteId: 3, domain: "remove.example.com", visibility: "public", passwordHash: null });
    expect(getCachedSite("remove.example.com")).not.toBeNull();
    invalidateSiteCache(3);
    expect(getCachedSite("remove.example.com")).toBeNull();
  });

  it("invalidateSiteCache removes multiple domains for same site", () => {
    setCachedSite({ siteId: 4, domain: "a.example.com", visibility: "public", passwordHash: null });
    setCachedSite({ siteId: 4, domain: "b.example.com", visibility: "public", passwordHash: null });
    invalidateSiteCache(4);
    expect(getCachedSite("a.example.com")).toBeNull();
    expect(getCachedSite("b.example.com")).toBeNull();
  });

  it("caches password hash correctly", () => {
    setCachedSite({ siteId: 5, domain: "secure.example.com", visibility: "password", passwordHash: "abc:def" }); // pragma: allowlist secret — dummy hash
    const cached = getCachedSite("secure.example.com");
    expect(cached!.visibility).toBe("password");
    expect(cached!.passwordHash).toBe("abc:def");
  });
});

describe("file cache", () => {
  it("returns null for uncached file", () => {
    expect(getCachedFile(1, "index.html")).toBeNull();
  });

  it("stores and retrieves a file", () => {
    setCachedFile(6, "index.html", { objectPath: "/objects/abc", contentType: "text/html", sizeBytes: 1024 });
    const cached = getCachedFile(6, "index.html");
    expect(cached).not.toBeNull();
    expect(cached!.objectPath).toBe("/objects/abc");
    expect(cached!.contentType).toBe("text/html");
  });

  it("different sites don't share file cache entries", () => {
    setCachedFile(7, "style.css", { objectPath: "/objects/site7", contentType: "text/css", sizeBytes: 512 });
    expect(getCachedFile(8, "style.css")).toBeNull();
    expect(getCachedFile(7, "style.css")).not.toBeNull();
  });

  it("returns null after TTL expires", () => {
    const now = Date.now();
    vi.spyOn(Date, "now").mockReturnValue(now);
    setCachedFile(9, "app.js", { objectPath: "/objects/js", contentType: "text/javascript", sizeBytes: 2048 });
    vi.spyOn(Date, "now").mockReturnValue(now + 5 * 60 * 1000 + 1);
    expect(getCachedFile(9, "app.js")).toBeNull();
    vi.restoreAllMocks();
  });
});

describe("cache stats", () => {
  it("reports domain and file entry counts", () => {
    setCachedSite({ siteId: 10, domain: "stats.example.com", visibility: "public", passwordHash: null });
    setCachedFile(10, "index.html", { objectPath: "/objects/x", contentType: "text/html", sizeBytes: 100 });
    const stats = getCacheStats();
    expect(stats.domainEntries).toBeGreaterThanOrEqual(1);
    expect(stats.fileEntries).toBeGreaterThanOrEqual(1);
    expect(stats.ttlMs).toBeGreaterThan(0);
  });
});
