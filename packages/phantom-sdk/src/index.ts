/**
 * Phantom SDK — Post-quantum identity, signing, and encryption.
 * 
 * Architecture:
 * - Secret keys stay in WASM memory (never exposed to JS)
 * - JS gets opaque numeric handles
 * - All crypto uses Kyber-1024 (KEM) + Dilithium-5 (signatures) + Blake3 (hashing)
 *
 * Integration pattern per app:
 *   ```ts
 *   import { createPhantomSDK } from "@nexus/phantom-sdk";
 *   const phantom = await createPhantomSDK();
 *   const id = await phantom.generateIdentity("nexus-graphic");
 *   const sig = await phantom.sign(id.handle, "payload");
 *   ```
 */

export interface PhantomIdentity {
  handle: number;
  did: string;
  publicKey: string;        // hex
  signingPublicKey: string;  // hex
}

export interface PhantomSDK {
  generateIdentity(name: string): Promise<PhantomIdentity>;
  sign(handle: number, message: Uint8Array): Promise<string>;        // returns hex signature
  verify(handle: number, message: Uint8Array, signature: string): Promise<boolean>;
  encapsulate(handle: number): Promise<{ ciphertext: string; sharedSecret: string }>;
  decapsulate(handle: number, ciphertext: string): Promise<string>;  // returns hex sharedSecret
  getDID(handle: number): Promise<string>;
  release(handle: number): void;
  phantomHash(data: Uint8Array): Promise<string>;  // returns hex hash
  version(): string;
  /**
   * True when this is the stand-in, not real cryptography.
   *
   * Exposed because callers were given no way to tell. A security substrate
   * that cannot be asked whether it is actually running is one that will be
   * assumed to be running.
   */
  isMock: boolean;
}

// ── WASM-based implementation ─────────────────────────────────────

type WasmExports = {
  generate_identity: (name: string) => any;
  sign: (handle: number, messageHex: string) => any;
  verify: (handle: number, messageHex: string, signatureHex: string) => any;
  encapsulate: (handle: number) => any;
  decapsulate: (handle: number, ciphertextHex: string) => any;
  get_did: (handle: number) => any;
  release: (handle: number) => void;
  phantom_hash: (dataHex: string) => any;
  version: () => string;
  memory: WebAssembly.Memory;
};

function hexEncode(bytes: Uint8Array): string {
  return Array.from(bytes).map(b => b.toString(16).padStart(2, "0")).join("");
}

function hexDecode(hex: string): Uint8Array {
  const bytes = new Uint8Array(hex.length / 2);
  for (let i = 0; i < hex.length; i += 2) {
    bytes[i / 2] = parseInt(hex.substring(i, i + 2), 16);
  }
  return bytes;
}

/**
 * Load the compiled WASM module, if one has been built.
 *
 * This used to throw unconditionally with build instructions, which meant the
 * real path was unreachable even after building. It now actually tries.
 */
async function loadWasm(): Promise<WasmExports> {
  // The specifier is assembled rather than written as a literal so the
  // type-checker does not try to resolve a path that only exists after
  // `wasm-pack build`. Consumers must still compile when it has not been run.
  const spec = ["..", "wasm", "pkg", "phantom_wasm.js"].join("/");
  const pkg = await import(/* @vite-ignore */ spec);
  return pkg as unknown as WasmExports;
}

// ── Pure JS mock (for testing without WASM) ────────────────────────

function createMockSDK(): PhantomSDK {
  let nextHandle = 1;

  const identities = new Map<number, { did: string; publicKey: string; signingPublicKey: string }>();
  // Store real signatures so sign→verify works deterministically
  const signatures = new Map<string, string>(); // key: `${handle}:${msgLength}`, value: hex sig

  return {
    async generateIdentity(name: string) {
      const h = nextHandle++;
      const handle = h;
      const hashHex = hexEncode(new TextEncoder().encode(name + ":" + h)).substring(0, 16);
      const did = `did:phantom:mock:${hashHex}`;
      identities.set(handle, {
        did,
        publicKey: "ab".repeat(800),       // mock Kyber-1024 pk (1600 hex chars)
        signingPublicKey: "cd".repeat(2592), // mock Dilithium-5 pk (5184 hex chars)
      });
      return { handle, did, publicKey: identities.get(handle)!.publicKey, signingPublicKey: identities.get(handle)!.signingPublicKey };
    },

    async sign(handle, message) {
      if (!identities.has(handle)) throw new Error("Identity not found");
      const hexMsg = hexEncode(message);
      const key = `${handle}:${hexMsg.substring(0, 32)}`;
      const sig = hexMsg.repeat(64).substring(0, 2592); // mock Dilithium-5 sig size
      signatures.set(key, sig);
      return sig;
    },

    async verify(handle, message, signature) {
      if (!identities.has(handle)) return false;
      const hexMsg = hexEncode(message);
      const key = `${handle}:${hexMsg.substring(0, 32)}`;
      return signatures.get(key) === signature;
    },

    async encapsulate(handle) {
      if (!identities.has(handle)) throw new Error("Identity not found");
      // Repeated bytes, not a key — this is the mock.
      return { ciphertext: "aa".repeat(768), sharedSecret: "bb".repeat(32) }; // pragma: allowlist secret
    },

    async decapsulate(handle, _ciphertext) {
      if (!identities.has(handle)) throw new Error("Identity not found");
      return "bb".repeat(32);
    },

    async getDID(handle) {
      return identities.get(handle)?.did ?? "";
    },

    release(handle) {
      identities.delete(handle);
    },

    async phantomHash(data) {
      return hexEncode(data).repeat(8).substring(0, 64);
    },

    version() {
      return "phantom-sdk-mock/0.1.0";
    },

    isMock: true,
  };
}

// ── Factory ────────────────────────────────────────────────────────

export async function createPhantomSDK(): Promise<PhantomSDK> {
  try {
    const wasm = await loadWasm();
    return createWasmSDK(wasm);
  } catch (err) {
    // The fallback used to be silent. A bare catch that swaps counterfeit
    // cryptography in for real cryptography, while every caller reports
    // success, is worse than having no integration at all: absence is visible,
    // and this was not.
    const reason = err instanceof Error ? err.message : String(err);

    if (requireReal()) {
      throw new Error(
        `Phantom: refusing to start with mock cryptography (PHANTOM_REQUIRE_REAL is set). ` +
          `The WASM module could not be loaded: ${reason}`,
      );
    }

    console.error(
      "\n" +
        "  ┌──────────────────────────────────────────────────────────────┐\n" +
        "  │  PHANTOM IS NOT PROVIDING ANY CRYPTOGRAPHY                   │\n" +
        "  ├──────────────────────────────────────────────────────────────┤\n" +
        "  │  The WASM module could not be loaded, so a stand-in is in    │\n" +
        "  │  use. It returns fabricated keys and signatures that verify  │\n" +
        "  │  against nothing. DIDs are prefixed did:phantom:mock:.       │\n" +
        "  │                                                              │\n" +
        "  │  Build it:  cd packages/phantom-sdk/wasm                     │\n" +
        "  │             wasm-pack build --target bundler                 │\n" +
        "  │  Enforce:   PHANTOM_REQUIRE_REAL=1 to refuse to start        │\n" +
        "  └──────────────────────────────────────────────────────────────┘\n" +
        `  reason: ${reason}\n`,
    );
    return createMockSDK();
  }
}

/**
 * Whether a caller has demanded real cryptography or nothing.
 *
 * Off by default, deliberately: 82 services call this at boot, and turning it
 * on before the module builds would take the ecosystem down rather than secure
 * it. Turn it on per-service as each is verified, and globally once the build
 * is part of deployment.
 */
function requireReal(): boolean {
  const v = (globalThis as { process?: { env?: Record<string, string | undefined> } })
    .process?.env?.PHANTOM_REQUIRE_REAL;
  return v === "1" || v === "true";
}

function createWasmSDK(wasm: WasmExports): PhantomSDK {
  return {
    async generateIdentity(name: string) {
      const result = wasm.generate_identity(name);
      const raw = typeof result === "string" ? JSON.parse(result) : result;
      return {
        handle: raw.handle as number,
        did: raw.did as string,
        publicKey: raw.publicKey as string,
        signingPublicKey: raw.signingPublicKey as string,
      };
    },

    async sign(handle, message) {
      const result = wasm.sign(handle, hexEncode(message));
      const raw = typeof result === "string" ? JSON.parse(result) : result;
      return raw.signature as string;
    },

    async verify(handle, message, signature) {
      const result = wasm.verify(handle, hexEncode(message), signature);
      const raw = typeof result === "string" ? JSON.parse(result) : result;
      return raw.valid as boolean;
    },

    async encapsulate(handle) {
      const result = wasm.encapsulate(handle);
      const raw = typeof result === "string" ? JSON.parse(result) : result;
      return { ciphertext: raw.ciphertext as string, sharedSecret: raw.sharedSecret as string };
    },

    async decapsulate(handle, ciphertext) {
      const result = wasm.decapsulate(handle, ciphertext);
      const raw = typeof result === "string" ? JSON.parse(result) : result;
      return raw.sharedSecret as string;
    },

    async getDID(handle) {
      const result = wasm.get_did(handle);
      const raw = typeof result === "string" ? JSON.parse(result) : result;
      return raw.did as string;
    },

    release(handle) {
      wasm.release(handle);
    },

    async phantomHash(data) {
      const result = wasm.phantom_hash(hexEncode(data));
      const raw = typeof result === "string" ? JSON.parse(result) : result;
      return raw.hash as string;
    },

    version() {
      return wasm.version();
    },

    isMock: false,
  };
}

// ── Test utilities ─────────────────────────────────────────────────

export { createMockSDK, hexEncode, hexDecode };
