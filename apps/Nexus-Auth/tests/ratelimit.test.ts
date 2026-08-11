import { describe, it, expect, beforeEach } from "bun:test";
import * as rl from "../src/ratelimit";

describe("rate limiting", () => {
  beforeEach(() => rl.resetRateLimits());

  it("allows attempts up to the limit then blocks", () => {
    for (let i = 0; i < 5; i++) {
      expect(rl.checkRateLimit("claim", "1.2.3.4", { max: 5, windowMs: 60_000 }).allowed).toBe(true);
      rl.recordFailure("claim", "1.2.3.4");
    }
    const blocked = rl.checkRateLimit("claim", "1.2.3.4", { max: 5, windowMs: 60_000 });
    expect(blocked.allowed).toBe(false);
    if (!blocked.allowed) expect(blocked.retryAfterMs).toBeGreaterThan(0);
  });

  it("keeps buckets independent", () => {
    for (let i = 0; i < 5; i++) rl.recordFailure("claim", "1.2.3.4");
    expect(rl.checkRateLimit("claim", "1.2.3.4", { max: 5 }).allowed).toBe(false);
    expect(rl.checkRateLimit("recover", "1.2.3.4", { max: 5 }).allowed).toBe(true);
  });

  it("keeps keys independent", () => {
    for (let i = 0; i < 5; i++) rl.recordFailure("claim", "1.2.3.4");
    expect(rl.checkRateLimit("claim", "5.6.7.8", { max: 5 }).allowed).toBe(true);
  });

  it("a success clears the failure count", () => {
    for (let i = 0; i < 4; i++) rl.recordFailure("claim", "1.2.3.4");
    rl.clearFailures("claim", "1.2.3.4");
    expect(rl.checkRateLimit("claim", "1.2.3.4", { max: 5 }).allowed).toBe(true);
  });

  it("forgets failures once the window has passed", () => {
    for (let i = 0; i < 5; i++) rl.recordFailure("claim", "1.2.3.4");
    expect(rl.checkRateLimit("claim", "1.2.3.4", { max: 5, windowMs: 1 }).allowed).toBe(false);
    Bun.sleepSync(5);
    expect(rl.checkRateLimit("claim", "1.2.3.4", { max: 5, windowMs: 1 }).allowed).toBe(true);
  });
});
