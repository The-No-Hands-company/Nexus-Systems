import { describe, it, expect } from "bun:test";
import {
  encodeBase32,
  decodeBase32,
  generateTotpSecret,
  generateTotpCode,
  verifyTotpCode,
  buildTotpUri,
} from "../src/totp";

describe("TOTP implementation (RFC 6238)", () => {
  it("encodes and decodes base32 roundtrip", () => {
    const raw = Buffer.from("Hello Nexus Ecosystem!");
    const encoded = encodeBase32(raw);
    const decoded = decodeBase32(encoded);
    expect(Buffer.from(decoded).toString()).toBe("Hello Nexus Ecosystem!");
  });

  it("generates 20-byte base32 secret and valid uri", () => {
    const secret = generateTotpSecret();
    expect(secret.length).toBeGreaterThanOrEqual(32);
    const uri = buildTotpUri("alice", secret);
    expect(uri).toContain("otpauth://totp/Nexus%3Aalice");
    expect(uri).toContain(`secret=${secret}`);
  });

  it("generates 6 digit code and verifies successfully within window", () => {
    const secret = generateTotpSecret();
    const code = generateTotpCode(secret);
    expect(code).toMatch(/^\d{6}$/);

    expect(verifyTotpCode(secret, code)).toBe(true);
    expect(verifyTotpCode(secret, "000000")).toBe(false);
    expect(verifyTotpCode(secret, "bad")).toBe(false);
  });

  it("verifies time-step drift tolerance", () => {
    const secret = generateTotpSecret();
    const currentStep = Math.floor(Date.now() / 1000 / 30);

    const prevCode = generateTotpCode(secret, currentStep - 1);
    const nextCode = generateTotpCode(secret, currentStep + 1);
    const farCode = generateTotpCode(secret, currentStep - 3);

    expect(verifyTotpCode(secret, prevCode, 1)).toBe(true);
    expect(verifyTotpCode(secret, nextCode, 1)).toBe(true);
    expect(verifyTotpCode(secret, farCode, 1)).toBe(false);
  });
});
