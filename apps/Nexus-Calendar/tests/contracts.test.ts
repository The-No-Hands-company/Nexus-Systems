import { describe, expect, it } from "bun:test";
import { buildSystemsApiRegistrationPayload } from "../src/contracts";

describe("Calendar Systems API registration", () => {
  it("declares its proxied delivery contract without self-authorizing", () => {
    const payload = buildSystemsApiRegistrationPayload("http://127.0.0.1:3068");

    expect(payload).toMatchObject({
      path: "/calendar",
      publicUrl: "https://calendar.tnhc.dev",
      delivery: "proxied-app",
    });
    expect("requiresAuth" in payload).toBe(false);
  });
});
