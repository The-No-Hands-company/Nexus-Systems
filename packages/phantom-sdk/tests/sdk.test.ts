import { describe, it, expect, afterAll } from "bun:test";
import { createMockSDK, createPhantomSDK, hexEncode, hexDecode, type PhantomSDK } from "../src/index";

describe("phantom-sdk (mock)", () => {
  let sdk: PhantomSDK;
  let handle: number;

  it("generates an identity", async () => {
    sdk = createMockSDK();
    const id = await sdk.generateIdentity("test-user");
    expect(id.handle).toBeGreaterThan(0);
    expect(id.did).toStartWith("did:phantom:");
    expect(id.publicKey.length).toBeGreaterThan(0);
    handle = id.handle;
  });

  it("signs and verifies", async () => {
    const msg = new TextEncoder().encode("hello phantom");
    const sig = await sdk.sign(handle, msg);
    expect(sig.length).toBeGreaterThan(0);
    const valid = await sdk.verify(handle, msg, sig);
    expect(valid).toBe(true);
  });

  it("rejects tampered messages", async () => {
    const msg = new TextEncoder().encode("original");
    const sig = await sdk.sign(handle, msg);
    const tampered = await sdk.verify(handle, new TextEncoder().encode("tampered"), sig);
    expect(tampered).toBe(false);
  });

  it("encapsulates and decapsulates", async () => {
    const { ciphertext, sharedSecret } = await sdk.encapsulate(handle);
    const recovered = await sdk.decapsulate(handle, ciphertext);
    expect(recovered).toBe(sharedSecret);
  });

  it("hashes with blake3", async () => {
    const h = await sdk.phantomHash(new TextEncoder().encode("phantom"));
    expect(h.length).toBe(64);
  });

  it("handles multiple identities", async () => {
    const alice = await sdk.generateIdentity("alice");
    const bob = await sdk.generateIdentity("bob");
    expect(alice.handle).not.toBe(bob.handle);
    sdk.release(alice.handle);
    sdk.release(bob.handle);
  });

  it("releases identity", () => {
    const h = handle;
    sdk.release(h);
    expect(sdk.getDID(h)).resolves.toBe("");
  });
});

describe("phantom honesty", () => {
  // The SDK silently substituted a mock for real cryptography and every caller
  // reported success. These pin the properties that make that impossible to
  // repeat: it must be askable, and it must be refusable.

  it("admits when it is not real cryptography", async () => {
    const sdk = await createPhantomSDK();
    expect(typeof sdk.isMock).toBe("boolean");
  });

  it("a mock identity is recognisable as one from its DID alone", async () => {
    const sdk = await createPhantomSDK();
    const id = await sdk.generateIdentity("honesty-check");
    if (sdk.isMock) {
      expect(id.did).toContain("mock");
    } else {
      expect(id.did).not.toContain("mock");
    }
  });

  it("refuses to start on mock crypto when PHANTOM_REQUIRE_REAL is set", async () => {
    const prev = process.env.PHANTOM_REQUIRE_REAL;
    process.env.PHANTOM_REQUIRE_REAL = "1";
    try {
      // Only meaningful while the WASM module is unbuilt; once it builds this
      // resolves for real, which is the outcome we want either way.
      let threw = false;
      let sdk: Awaited<ReturnType<typeof createPhantomSDK>> | null = null;
      try {
        sdk = await createPhantomSDK();
      } catch (e) {
        threw = true;
        expect(String(e)).toContain("mock cryptography");
      }
      if (!threw) {
        expect(sdk?.isMock).toBe(false);
      }
    } finally {
      if (prev === undefined) delete process.env.PHANTOM_REQUIRE_REAL;
      else process.env.PHANTOM_REQUIRE_REAL = prev;
    }
  });
});
