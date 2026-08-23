import { describe, it, expect, beforeEach } from "bun:test";
import { handleRequest } from "../src/server";
import { resetRateLimits } from "../src/ratelimit";

beforeEach(() => resetRateLimits());

function loginReq(username: string): Request {
  return new Request("http://localhost:4310/api/v1/auth/login", {
    method: "POST",
    headers: { "content-type": "application/json", "x-forwarded-for": "10.9.9.9" },
    body: JSON.stringify({ username, password: "wrong-password-12345" }),
  });
}

describe("login rate limiting", () => {
  it("returns 401 for wrong credentials within budget", async () => {
    const res = await handleRequest(loginReq("ratelimit-test-user"));
    expect(res.status).toBe(401);
  });

  it("escalates to 429 after repeated failures from the same IP + username", async () => {
    // Default max is 5 failures per 15-minute window.
    for (let i = 0; i < 5; i++) await handleRequest(loginReq("ratelimit-flood"));
    const res = await handleRequest(loginReq("ratelimit-flood"));
    expect(res.status).toBe(429);
    expect(Number(res.headers.get("retry-after"))).toBeGreaterThan(0);
  });

  it("does not lock out a different username from the same IP", async () => {
    // Flood one account; a second must still get 401 (not 429).
    for (let i = 0; i < 6; i++) await handleRequest(loginReq("flood-target"));
    const other = await handleRequest(loginReq("other-user"));
    expect(other.status).toBe(401);
  });
});
