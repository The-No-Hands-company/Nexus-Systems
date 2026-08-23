import { describe, it, expect } from "vitest";
import {
  generateKeyPair,
  signMessage,
  verifySignature,
  createFederationChallenge,
  stripPemHeaders,
} from "../../src/lib/federation";

describe("generateKeyPair", () => {
  it("generates distinct keys each time", () => {
    const a = generateKeyPair();
    const b = generateKeyPair();
    expect(a.publicKey).not.toBe(b.publicKey);
    expect(a.privateKey).not.toBe(b.privateKey);
  });

  it("keys are PEM-formatted", () => {
    const { publicKey, privateKey } = generateKeyPair(); // pragma: allowlist secret — variable name only, key is runtime-generated
    expect(publicKey).toContain("BEGIN PUBLIC KEY");
    expect(privateKey).toContain("BEGIN PRIVATE KEY"); // pragma: allowlist secret — string assertion, not key material
  });
});

describe("signMessage / verifySignature", () => {
  it("valid signature verifies correctly", () => {
    const { publicKey, privateKey } = generateKeyPair();
    const msg = "hello:world:1234567890";
    const sig = signMessage(privateKey, msg);
    expect(verifySignature(publicKey, msg, sig)).toBe(true);
  });

  it("tampered message fails verification", () => {
    const { publicKey, privateKey } = generateKeyPair();
    const msg = "hello:world:1234567890";
    const sig = signMessage(privateKey, msg);
    expect(verifySignature(publicKey, "hello:world:9999999999", sig)).toBe(false);
  });

  it("wrong public key fails verification", () => {
    const { privateKey } = generateKeyPair();
    const { publicKey: wrongKey } = generateKeyPair();
    const msg = "test-message";
    const sig = signMessage(privateKey, msg);
    expect(verifySignature(wrongKey, msg, sig)).toBe(false);
  });

  it("corrupted signature fails gracefully", () => {
    const { publicKey, privateKey } = generateKeyPair();
    const msg = "test";
    const sig = signMessage(privateKey, msg);
    const corrupted = sig.slice(0, -4) + "AAAA";
    expect(verifySignature(publicKey, msg, corrupted)).toBe(false);
  });

  it("empty message can be signed and verified", () => {
    const { publicKey, privateKey } = generateKeyPair();
    const sig = signMessage(privateKey, "");
    expect(verifySignature(publicKey, "", sig)).toBe(true);
  });

  it("unicode message can be signed and verified", () => {
    const { publicKey, privateKey } = generateKeyPair();
    const msg = "Selamat datang di jaringan federasi 🌐";
    const sig = signMessage(privateKey, msg);
    expect(verifySignature(publicKey, msg, sig)).toBe(true);
  });
});

describe("createFederationChallenge", () => {
  it("generates 64-char hex strings", () => {
    const c = createFederationChallenge();
    expect(c).toMatch(/^[0-9a-f]{64}$/);
  });

  it("each challenge is unique", () => {
    const challenges = new Set(Array.from({ length: 100 }, createFederationChallenge));
    expect(challenges.size).toBe(100);
  });
});

describe("stripPemHeaders", () => {
  it("removes PEM headers and whitespace", () => {
    const { publicKey } = generateKeyPair();
    const stripped = stripPemHeaders(publicKey);
    expect(stripped).not.toContain("BEGIN");
    expect(stripped).not.toContain("END");
    expect(stripped).not.toMatch(/\s/);
    expect(stripped.length).toBeGreaterThan(0);
  });

  it("stripped key can round-trip back to full PEM", () => {
    const { publicKey, privateKey } = generateKeyPair();
    const stripped = stripPemHeaders(publicKey);
    // Reconstruct minimal PEM and verify signature still works
    const reconstructed = `-----BEGIN PUBLIC KEY-----\n${stripped.match(/.{1,64}/g)!.join("\n")}\n-----END PUBLIC KEY-----\n`;
    const sig = signMessage(privateKey, "test");
    expect(verifySignature(reconstructed, "test", sig)).toBe(true);
  });
});
