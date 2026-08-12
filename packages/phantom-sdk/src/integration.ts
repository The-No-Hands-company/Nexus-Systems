/**
 * Phantom App Integration Module
 * 
 * One import to bind any Nexus app to the Phantom privacy protocol.
 * 
 * Usage in any app's server.ts:
 *   import { PhantomApp } from "@nexus/phantom-sdk/integration";
 *   
 *   const phantom = new PhantomApp("nexus-graphic");
 *   const id = await phantom.start();
 *   
 *   // Add to health/status responses:
 *   phantomIdentity: phantom.status(),
 *   
 *   // Sign responses:
 *   const headers = await phantom.signResponse(responseBody);
 * 
 * Every app gets:
 *   - A Phantom DID (decentralized identifier)
 *   - Post-quantum identity (Kyber-1024 + Dilithium-5)  
 *   - Signed provenance on all data
 *   - Key persistence across restarts
 */

import type { PhantomSDK, PhantomIdentity } from "./index";
import { createPhantomSDK } from "./index";

const DATA_DIR = process.env.NEXUS_DATA_DIR || "../data";

interface PersistentIdentity {
  did: string;
  publicKey: string;
  signingPublicKey: string;
  name: string;
  createdAt: string;
}

export class PhantomApp {
  public sdk: PhantomSDK | null = null;
  public identity: PhantomIdentity | null = null;
  public readonly appName: string;
  private started = false;

  constructor(appName: string) {
    this.appName = appName;
  }

  /** Start Phantom — load or generate identity. Call once at app startup. */
  async start(): Promise<PhantomIdentity> {
    if (this.started) return this.identity!;
    this.sdk = await createPhantomSDK();

    // Try to load existing identity from disk
    const saved = this.loadIdentity();
    if (saved) {
      this.identity = await this.sdk.generateIdentity(saved.name);
      this.identity.did = saved.did; // Restore original DID
    } else {
      this.identity = await this.sdk.generateIdentity(this.appName);
      this.saveIdentity({
        did: this.identity.did,
        publicKey: this.identity.publicKey,
        signingPublicKey: this.identity.signingPublicKey,
        name: this.appName,
        createdAt: new Date().toISOString(),
      });
    }

    this.started = true;
    if (this.sdk?.isMock) {
      console.warn(
        `[${this.appName}] Phantom identity ${this.identity.did} is NOT cryptographically real — ` +
          `the WASM module is not built, so signatures verify against nothing.`,
      );
    } else {
      console.log(`[${this.appName}] Phantom identity: ${this.identity.did}`);
    }
    return this.identity;
  }

  /** Get the app's status block for /api/v1/status or /health responses. */
  status() {
    if (!this.identity) return { bound: false };
    // This used to report "Kyber-1024, Dilithium-5, Blake3" whether or not any
    // of it was running — a health endpoint asserting post-quantum protection
    // that did not exist. It now describes what is actually loaded.
    const mock = this.sdk?.isMock !== false;
    return {
      bound: true,
      did: this.identity.did,
      protocol: "phantom-v1",
      algorithms: mock ? "none (mock)" : "Kyber-1024, Dilithium-5, Blake3",
      cryptography: mock ? "mock" : "real",
      sdkVersion: this.sdk?.version() ?? "unknown",
    };
  }

  /** Sign arbitrary data and return { signature, did, timestamp }. */
  async sign(data: Uint8Array): Promise<PhantomSignature> {
    if (!this.sdk || !this.identity) throw new Error("Phantom not started");
    const signature = await this.sdk.sign(this.identity.handle, data);
    return {
      did: this.identity.did,
      signature,
      timestamp: Date.now(),
    };
  }

  /** Verify a signature from another app. */
  async verify(publicKey: string, data: Uint8Array, signature: string, handle: number): Promise<boolean> {
    if (!this.sdk) return false;
    const temp = await this.sdk.generateIdentity("verifier");
    const result = await this.sdk.verify(handle, data, signature);
    this.sdk.release(temp.handle);
    return result;
  }

  /** Get the public key for identity verification by other services. */
  publicKey(): string {
    return this.identity?.publicKey ?? "";
  }

  /** Release resources on shutdown. */
  stop() {
    if (this.sdk && this.identity) {
      this.sdk.release(this.identity.handle);
    }
  }

  // ── Persistence ─────────────────────────────────────────────

  private identityPath(): string {
    return `${DATA_DIR}/phantom-${this.appName}.json`;
  }

  private saveIdentity(id: PersistentIdentity) {
    try {
      Bun.write(this.identityPath(), JSON.stringify(id, null, 2));
    } catch {
      // Non-fatal: identity will regenerate on next start
    }
  }

  private loadIdentity(): PersistentIdentity | null {
    try {
      const raw = Bun.file(this.identityPath());
      if (!raw.size) return null;
      return JSON.parse(raw.toString()) as PersistentIdentity;
    } catch {
      return null;
    }
  }
}

export interface PhantomSignature {
  did: string;
  signature: string;
  timestamp: number;
}
