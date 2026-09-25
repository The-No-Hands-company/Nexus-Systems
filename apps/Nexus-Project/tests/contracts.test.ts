import { describe, expect, it } from "bun:test";
import { buildSystemsApiRegistrationPayload } from "../src/contracts";

describe("Cloud registration", () => {
  it("declares proxied delivery and does not self-authorize", () => {
    const payload = buildSystemsApiRegistrationPayload("http://127.0.0.1:3152");
    expect(payload).toMatchObject({
      id: "nexus-project",
      upstreamUrl: "http://127.0.0.1:3152",
      path: "/project",
      publicUrl: "https://project.tnhc.dev",
      delivery: "proxied-app",
    });
    // Whether an app is behind the SSO gate is Cloud's operator-only switch.
    expect("requiresAuth" in payload).toBe(false);
  });
});
