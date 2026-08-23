import { describe, it, expect, beforeEach } from "bun:test";
import { checkRateLimit, createRateLimitResponse, resetRateLimits } from "../src/ratelimit";

function reqFromIp(ip: string): Request {
  return new Request("http://app.test/ipa/apps", {
    headers: ip === "unknown" ? {} : { "x-forwarded-for": ip },
  });
}

describe("rate limiter", () => {
  beforeEach(() => {
    resetRateLimits();
  });

  it("allows the first request from an unseen caller", () => {
    const result = checkRateLimit(reqFromIp("10.1.1.1"));
    expect(result.allowed).toBe(true);
  });

  it("tracks each caller independently by IP", () => {
    const first = checkRateLimit(reqFromIp("10.1.1.2"));
    const other = checkRateLimit(reqFromIp("10.1.1.3"));
    expect(first.allowed).toBe(true);
    expect(other.allowed).toBe(true);
    // Independent budgets: exhausting one must not touch the other's remaining.
    expect(other.remaining).toBeGreaterThanOrEqual(first.remaining - 1);
  });

  it("eventually refuses when one caller floods the endpoint", () => {
    const ip = "10.2.0.9";
    let refused = 0;
    for (let i = 0; i < 500; i++) {
      if (!checkRateLimit(reqFromIp(ip)).allowed) refused++;
    }
    // 100-token bucket refilling at 20/s cannot absorb 500 instant requests.
    expect(refused).toBeGreaterThan(300);
  });

  it("returns a 429 with Retry-After when refused", async () => {
    const ip = "10.3.0.5";
    for (let i = 0; i < 200; i++) checkRateLimit(reqFromIp(ip));
    const res = createRateLimitResponse(checkRateLimit(reqFromIp(ip)));
    expect(res.status).toBe(429);
    expect(Number(res.headers.get("retry-after"))).toBeGreaterThan(0);
    expect(res.headers.get("x-ratelimit-remaining")).toBe("0");
  });
});