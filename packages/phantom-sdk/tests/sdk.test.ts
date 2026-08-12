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

describe("native crypto (bun:ffi)", () => {
  // Skips cleanly when the library has not been built, so the suite stays
  // green on a fresh clone — but when it IS built these are the tests that
  // prove the crypto is real rather than plausible-looking.
  it("uses the native library when it is present", async () => {
    const { nativeLibraryExists } = await import("../src/native");
    if (!nativeLibraryExists()) return;

    const sdk = await createPhantomSDK();
    expect(sdk.isMock).toBe(false);
    expect(sdk.version()).toContain("native");
    expect(typeof sdk.version()).toBe("string"); // not a String object
  });

  it("produces real Kyber-1024 and Dilithium-5 key sizes", async () => {
    const { nativeLibraryExists } = await import("../src/native");
    if (!nativeLibraryExists()) return;

    const sdk = await createPhantomSDK();
    const id = await sdk.generateIdentity("size-check");
    try {
      expect(id.publicKey.length / 2).toBe(1568);        // Kyber-1024
      expect(id.signingPublicKey.length / 2).toBe(2592); // Dilithium-5
      expect(id.did).not.toContain("mock");
    } finally {
      sdk.release(id.handle);
    }
  });

  it("verifies its own signatures and rejects tampering", async () => {
    const { nativeLibraryExists } = await import("../src/native");
    if (!nativeLibraryExists()) return;

    const sdk = await createPhantomSDK();
    const id = await sdk.generateIdentity("sign-check");
    try {
      const msg = new TextEncoder().encode("payload");
      const sig = await sdk.sign(id.handle, msg);
      expect(await sdk.verify(id.handle, msg, sig)).toBe(true);
      expect(await sdk.verify(id.handle, new TextEncoder().encode("payl0ad"), sig)).toBe(false);
    } finally {
      sdk.release(id.handle);
    }
  });

  it("recovers the encapsulated secret", async () => {
    const { nativeLibraryExists } = await import("../src/native");
    if (!nativeLibraryExists()) return;

    const sdk = await createPhantomSDK();
    const id = await sdk.generateIdentity("kem-check");
    try {
      const { ciphertext, sharedSecret } = await sdk.encapsulate(id.handle);
      // A strict equality check, deliberately: this failed while the crypto was
      // perfectly correct, because CString returns a String object.
      expect(await sdk.decapsulate(id.handle, ciphertext)).toBe(sharedSecret);
      expect(sharedSecret.length / 2).toBe(32);
    } finally {
      sdk.release(id.handle);
    }
  });
});
