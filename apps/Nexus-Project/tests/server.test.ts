import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { startTestServer } from "./support/server";

describe("nexus-project server", () => {
  let t: Awaited<ReturnType<typeof startTestServer>>;
  beforeAll(async () => {
    t = await startTestServer();
  });
  afterAll(() => t.close());

  it("answers health without identity", async () => {
    const res = await t.raw(null, "GET", "/health");
    expect(res.status).toBe(200);
    expect(res.body).toMatchObject({ service: "nexus-project", status: "ok" });
  });

  it("answers status without identity", async () => {
    const res = await t.raw(null, "GET", "/api/v1/status");
    expect(res.status).toBe(200);
    expect(res.body.capabilities).toEqual(["projects", "tasks", "scheduling"]);
  });

  it("sends the privacy headers on every response", async () => {
    const res = await t.raw(null, "GET", "/health");
    expect(res.headers.get("cache-control")).toBe("no-store");
    expect(res.headers.get("referrer-policy")).toBe("no-referrer");
    expect(res.headers.get("x-content-type-options")).toBe("nosniff");
  });

  it("returns the error envelope for an unknown path", async () => {
    const res = await t.raw(null, "GET", "/nope");
    expect(res.status).toBe(404);
    expect(res.body).toEqual({ error: "not_found", message: "resource not found" });
  });

  it("requires an identity for the API", async () => {
    const res = await t.raw(null, "GET", "/api/v1/project/nothing-here");
    expect(res.status).toBe(401);
    expect(res.body).toEqual({
      error: "not_authenticated",
      message: "sign in to use Nexus Project",
    });
  });

  it("returns 404 for an unknown API path once authenticated", async () => {
    const res = await t.as("usr-alice").call("GET", "/nothing-here");
    expect(res.status).toBe(404);
  });
});
