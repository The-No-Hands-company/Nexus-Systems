/**
 * Real post-quantum cryptography, via Bun's FFI.
 *
 * Every consumer of this SDK is a server-side Bun process, so the browser was
 * never the right target — and `pqcrypto`'s C sources cannot compile to
 * `wasm32-unknown-unknown` regardless, because that target has no libc. Built
 * natively they compile without complaint, which makes FFI the shortest path
 * from "mock" to "actually Kyber-1024 and Dilithium-5".
 *
 * Build the library with:
 *
 *   cd packages/phantom-sdk/wasm && cargo build --release
 *
 * Everything crosses the boundary as hex in NUL-terminated C strings. Slower
 * than passing buffers, but identity operations are rare and the alternative
 * is manual lifetime rules on both sides of an FFI boundary.
 */

import { existsSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import type { PhantomSDK, PhantomIdentity } from "./index";
import { hexEncode } from "./index";

/** Where the compiled library is expected to be. */
export function nativeLibraryPath(): string {
  const override = process.env.PHANTOM_NATIVE_LIB?.trim();
  if (override) return override;

  const here = dirname(fileURLToPath(import.meta.url));
  const ext =
    process.platform === "darwin" ? "dylib" : process.platform === "win32" ? "dll" : "so";
  const base = process.platform === "win32" ? "phantom_wasm" : "libphantom_wasm";
  return join(here, "..", "wasm", "target", "release", `${base}.${ext}`);
}

export function nativeLibraryExists(): boolean {
  try {
    return existsSync(nativeLibraryPath());
  } catch {
    return false;
  }
}

/**
 * Open the native library and wrap it in the SDK interface.
 *
 * Throws when the library is missing or Bun's FFI is unavailable, which is the
 * signal for the caller to try the next implementation.
 */
export async function createNativeSDK(): Promise<PhantomSDK> {
  const path = nativeLibraryPath();
  if (!existsSync(path)) {
    throw new Error(
      `Phantom native library not found at ${path}. ` +
        `Build it: cd packages/phantom-sdk/wasm && cargo build --release`,
    );
  }

  // Imported by name so this module can be loaded (and type-checked) outside
  // Bun; only opening the library actually requires it.
  const { dlopen, FFIType, CString, ptr } = (await import(
    /* @vite-ignore */ "bun:ffi"
  )) as typeof import("bun:ffi");

  const { cstring, u64, i32, ptr: ptrT, void: voidT } = {
    cstring: FFIType.cstring,
    u64: FFIType.u64,
    i32: FFIType.i32,
    ptr: FFIType.ptr,
    void: FFIType.void,
  };

  const lib = dlopen(path, {
    phantom_generate_identity: { args: [cstring], returns: u64 },
    phantom_identity_info: { args: [u64], returns: ptrT },
    phantom_sign: { args: [u64, cstring], returns: ptrT },
    phantom_verify: { args: [u64, cstring, cstring], returns: i32 },
    phantom_encapsulate: { args: [u64], returns: ptrT },
    phantom_decapsulate: { args: [u64, cstring], returns: ptrT },
    phantom_hash: { args: [cstring], returns: ptrT },
    phantom_release: { args: [u64], returns: voidT },
    phantom_version: { args: [], returns: ptrT },
    phantom_free_string: { args: [ptrT], returns: voidT },
  });

  const s = lib.symbols;

  /** NUL-terminate for the C side. */
  const cstr = (v: string): Uint8Array => new TextEncoder().encode(v + "\0");

  /**
   * Read a returned C string and free it.
   *
   * Every string the library returns is heap-allocated over there; not handing
   * it back leaks, and every path out of here does.
   */
  const takeString = (p: number | bigint | null): string | null => {
    if (p === null || p === 0 || p === 0n) return null;
    try {
      // String(...) matters: `new CString()` yields a String *object*, not a
      // primitive. It prints identically and serialises identically, so it
      // looks right everywhere except `===`, where an object never equals a
      // string — which made a perfectly good KEM round-trip appear to fail.
      return String(new CString(p as number));
    } finally {
      s.phantom_free_string(p as never);
    }
  };

  const takeJson = <T>(p: number | bigint | null): T | null => {
    const raw = takeString(p);
    return raw === null ? null : (JSON.parse(raw) as T);
  };

  return {
    async generateIdentity(name: string): Promise<PhantomIdentity> {
      const handle = Number(s.phantom_generate_identity(ptr(cstr(name)) as never));
      if (!handle) throw new Error("Phantom: identity generation failed");
      const info = takeJson<{ did: string; publicKey: string; signingPublicKey: string }>(
        s.phantom_identity_info(handle as never) as never,
      );
      if (!info) throw new Error("Phantom: identity vanished immediately after creation");
      return { handle, ...info };
    },

    async sign(handle: number, message: Uint8Array): Promise<string> {
      const sig = takeString(
        s.phantom_sign(handle as never, ptr(cstr(hexEncode(message))) as never) as never,
      );
      if (sig === null) throw new Error("Phantom: signing failed (unknown handle?)");
      return sig;
    },

    async verify(handle: number, message: Uint8Array, signature: string): Promise<boolean> {
      const r = s.phantom_verify(
        handle as never,
        ptr(cstr(hexEncode(message))) as never,
        ptr(cstr(signature)) as never,
      );
      // -1 is "could not evaluate", which is not the same as "not verified".
      // Collapsing it to false would let an unknown handle read as a mere bad
      // signature.
      if (Number(r) < 0) throw new Error("Phantom: verification could not be evaluated");
      return Number(r) === 1;
    },

    async encapsulate(handle: number) {
      const out = takeJson<{ ciphertext: string; sharedSecret: string }>(
        s.phantom_encapsulate(handle as never) as never,
      );
      if (!out) throw new Error("Phantom: encapsulation failed (unknown handle?)");
      return out;
    },

    async decapsulate(handle: number, ciphertext: string): Promise<string> {
      const ss = takeString(
        s.phantom_decapsulate(handle as never, ptr(cstr(ciphertext)) as never) as never,
      );
      if (ss === null) throw new Error("Phantom: decapsulation failed");
      return ss;
    },

    async getDID(handle: number): Promise<string> {
      const info = takeJson<{ did: string }>(s.phantom_identity_info(handle as never) as never);
      if (!info) throw new Error("Phantom: unknown handle");
      return info.did;
    },

    release(handle: number): void {
      s.phantom_release(handle as never);
    },

    async phantomHash(data: Uint8Array): Promise<string> {
      const h = takeString(s.phantom_hash(ptr(cstr(hexEncode(data))) as never) as never);
      if (h === null) throw new Error("Phantom: hashing failed");
      return h;
    },

    version(): string {
      return takeString(s.phantom_version() as never) ?? "phantom-native/unknown";
    },

    isMock: false,
  };
}
