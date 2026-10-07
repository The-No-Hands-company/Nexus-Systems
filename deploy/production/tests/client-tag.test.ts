import { describe, it, expect } from "bun:test";
import { stripAddressHeaders, clientTag, ADDRESS_HEADERS, __rotateForTest } from "../client-tag";

describe("client tag", () => {
  it("is stable for one address within a rotation period and 22 url-safe chars", () => {
    const a = clientTag("203.0.113.7");
    expect(a).toBe(clientTag("203.0.113.7"));
    expect(a).toMatch(/^[A-Za-z0-9_-]{22}$/);
    expect(clientTag("203.0.113.8")).not.toBe(a);
  });
  it("changes for the same address after rotation", () => {
    const before = clientTag("198.51.100.4");
    __rotateForTest();
    expect(clientTag("198.51.100.4")).not.toBe(before);
  });
  it("never contains the address and maps a missing address to 'unknown'", () => {
    expect(clientTag("192.0.2.55")).not.toContain("192.0.2.55");
    expect(clientTag(null)).toBe("unknown");
    expect(clientTag("")).toBe("unknown");
  });
  it("rotates on its own after 24 hours", () => {
    const t0 = 1_800_000_000_000;
    const a = clientTag("203.0.113.9", t0);
    expect(clientTag("203.0.113.9", t0 + 23 * 3600_000)).toBe(a);
    expect(clientTag("203.0.113.9", t0 + 24 * 3600_000 + 1)).not.toBe(a);
  });
});

describe("stripAddressHeaders", () => {
  it("removes every address- and location-bearing header", () => {
    const h = new Headers();
    for (const n of ADDRESS_HEADERS) h.set(n, "203.0.113.1");
    h.set("accept", "text/html");
    stripAddressHeaders(h);
    for (const n of ADDRESS_HEADERS) expect(h.has(n)).toBe(false);
    expect(h.get("accept")).toBe("text/html");
  });
});
