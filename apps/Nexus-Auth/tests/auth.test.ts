import { describe, expect, test } from "bun:test";
import { buildSystemsApiRegistrationPayload } from "../src/contracts";
import { seedDefaultIdentities } from "../src/state";
import { issueServiceToken, validateServiceToken } from "../src/token";

describe("nexus-auth MVP contracts", () => {
  test("seed flow is idempotent", () => {
    const first = seedDefaultIdentities({ adminUsername: "root", userUsername: "demo" });
    const second = seedDefaultIdentities({ adminUsername: "root", userUsername: "demo" });

    expect(first.createdCount).toBe(2);
    expect(second.createdCount).toBe(0);
    expect(second.identities.map((item) => item.username)).toEqual(["root", "demo"]);
  });

  test("token issue and validate roundtrip", () => {
    const issued = issueServiceToken({
      serviceId: "nexus-cloud",
      audience: "nexus-internal",
      scopes: ["service:write"],
      expiresInSeconds: 120,
    });

    const result = validateServiceToken(issued.token, "nexus-internal");
    expect(result.valid).toBe(true);

    if (result.valid) {
      expect(result.payload.sub).toBe("nexus-cloud");
      expect(result.payload.scopes).toEqual(["service:write"]);
    }
  });

  test("systems-api registration payload shape is stable", () => {
    const payload = buildSystemsApiRegistrationPayload("http://localhost:4310");
    expect(payload.id).toBe("nexus-auth");
    expect(payload.name).toBe("Nexus Auth");
    expect(payload.mode).toBe("orchestrated");
    expect(payload.capabilities).toContain("service-auth");
    expect(payload.metadata.supportsServiceTokens).toBe(true);
  });
});