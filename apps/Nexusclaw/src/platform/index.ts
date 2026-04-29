// Phase 4.1 — Nexus Platform Convergence
//
// This module aligns Nexusclaw with the Nexus Cloud service model:
//   - NexusToolManifest: standardized public capability declaration
//   - PhantomHooks: privacy runtime integration extension points
//   - HostingExtension: deployment descriptor for Nexus Hosting
//
// These produce structured JSON artefacts that Nexus Cloud, Phantom, and
// Hosting can consume to auto-configure agent deployments.

import { writeFileSync, mkdirSync } from "node:fs";
import { join } from "node:path";
import type { AnyClawConfig } from "../config/index.js";

// ─── Nexus Tool Manifest (NTM v1) ────────────────────────────────────────────
// A standardised declaration of what an agent service exposes, consumable
// by Nexus Cloud for service discovery, public URL issuance, and capability
// routing.

export interface NexusToolManifestCapability {
  id: string;
  name: string;
  description: string;
  inputSchema: Record<string, unknown>;
  outputSchema?: Record<string, unknown>;
  /** Whether this capability requires user authentication */
  requiresAuth?: boolean;
  /** Maximum allowed tokens per call */
  tokenBudget?: number;
}

export interface NexusToolManifest {
  /** Manifest schema version */
  ntmVersion: "1.0";
  /** Nexus service ID (matches Nexus Cloud registration) */
  serviceId: string;
  serviceName: string;
  version: string;
  /** Public gateway endpoint (populated by Nexus Cloud on URL issuance) */
  publicUrl?: string;
  /** Internal/local gateway endpoint */
  internalUrl: string;
  /** ISO 8601 generation timestamp */
  generatedAt: string;
  capabilities: NexusToolManifestCapability[];
  /** Adapter IDs registered at manifest generation time */
  adapters: string[];
  /** Installed skill IDs */
  skills: string[];
  /** Runtime metadata */
  runtime: {
    engine: "nexusclaw";
    engineVersion: string;
    nodeVersion: string;
    platform: string;
    /** Memory tiers available */
    memoryTiers: string[];
  };
  /** Nexus Cloud service tags for routing */
  tags: string[];
}

export function buildNexusToolManifest(
  config: AnyClawConfig,
  opts: {
    tools: Array<{ id: string; name: string; description: string; inputSchema?: Record<string, unknown> }>;
    adapters: string[];
    skills: string[];
    gatewayUrl?: string;
    publicUrl?: string;
    tags?: string[];
  },
): NexusToolManifest {
  const capabilities: NexusToolManifestCapability[] = opts.tools.map(t => ({
    id: t.id,
    name: t.name,
    description: t.description,
    inputSchema: t.inputSchema ?? { type: "object", properties: {} },
  }));

  return {
    ntmVersion: "1.0",
    serviceId: (config as any).cloud?.service_id ?? `nexusclaw-${process.pid}`,
    serviceName: (config as any).cloud?.service_name ?? "AnyClaw Agent",
    version: "0.1.0",
    publicUrl: opts.publicUrl,
    internalUrl: opts.gatewayUrl ?? `http://127.0.0.1:${(config as any).gateway?.port ?? 18800}`,
    generatedAt: new Date().toISOString(),
    capabilities,
    adapters: opts.adapters,
    skills: opts.skills,
    runtime: {
      engine: "nexusclaw",
      engineVersion: "0.1.0",
      nodeVersion: process.version,
      platform: process.platform,
      memoryTiers: ["session", "episodic", "vault"],
    },
    tags: opts.tags ?? [],
  };
}

export function writeNexusToolManifest(manifest: NexusToolManifest, outDir: string): string {
  mkdirSync(outDir, { recursive: true });
  const outPath = join(outDir, "nexus-tool-manifest.json");
  writeFileSync(outPath, JSON.stringify(manifest, null, 2), "utf-8");
  return outPath;
}

// ─── Phantom Privacy Hooks ───────────────────────────────────────────────────
// Extension points for the Nexus Phantom privacy runtime.
// Phantom can intercept tool calls, memory writes, and LLM messages to apply
// local differential privacy, PII redaction, or consent enforcement.

export interface PhantomHookContext {
  agentId: string;
  sessionId: string;
  userId?: string;
  operation: "tool-call" | "memory-write" | "llm-message" | "session-start" | "session-end";
  payload: unknown;
  metadata?: Record<string, unknown>;
}

export type PhantomHookResult =
  | { action: "allow"; payload?: unknown }
  | { action: "redact"; payload: unknown; redactedFields: string[] }
  | { action: "block"; reason: string };

export type PhantomHookFn = (ctx: PhantomHookContext) => PhantomHookResult | Promise<PhantomHookResult>;

export class PhantomHookRegistry {
  private _hooks: PhantomHookFn[] = [];
  private _enabled = false;

  /** Register a Phantom privacy hook (e.g. PII redactor, consent checker). */
  register(hook: PhantomHookFn): void {
    this._hooks.push(hook);
    this._enabled = true;
  }

  /** Run all hooks in order.  First block/redact wins. */
  async run(ctx: PhantomHookContext): Promise<PhantomHookResult> {
    if (!this._enabled || this._hooks.length === 0) return { action: "allow" };
    for (const hook of this._hooks) {
      const result = await hook(ctx);
      if (result.action !== "allow") return result;
    }
    return { action: "allow" };
  }

  get enabled(): boolean { return this._enabled; }
  get hookCount(): number { return this._hooks.length; }
}

/** Singleton Phantom hook registry, shared across the runtime. */
export const phantomHooks = new PhantomHookRegistry();

// ─── Hosting Deployment Extension ───────────────────────────────────────────
// Describes how this Nexusclaw instance should be deployed via Nexus Hosting.
// The descriptor is written to `.nexus/hosting.json` and consumed by the
// Nexus Hosting CLI / deployment pipeline.

export interface HostingDeploymentDescriptor {
  /** Schema version */
  version: "1.0";
  serviceName: string;
  /** Docker image or registry reference */
  image?: string;
  /** Dockerfile path relative to project root */
  dockerfile?: string;
  port: number;
  /** Environment variable names required at runtime (values NOT included) */
  requiredEnv: string[];
  /** Optional environment defaults */
  defaultEnv?: Record<string, string>;
  /** Minimum and recommended resource sizing */
  resources: {
    minMemoryMb: number;
    recommendedMemoryMb: number;
    minCpus: number;
    persistentDirs: string[];
  };
  healthCheck: {
    path: string;
    intervalSeconds: number;
    timeoutSeconds: number;
  };
  /** Nexus Cloud public URL domain hint */
  domainHint?: string;
  generatedAt: string;
}

export function buildHostingDescriptor(
  config: AnyClawConfig,
  opts: {
    serviceName?: string;
    dockerfile?: string;
    image?: string;
    requiredEnv?: string[];
    domainHint?: string;
  } = {},
): HostingDeploymentDescriptor {
  const port = (config as any).gateway?.port ?? 18800;
  return {
    version: "1.0",
    serviceName: opts.serviceName ?? (config as any).cloud?.service_name ?? "nexusclaw",
    image: opts.image,
    dockerfile: opts.dockerfile ?? "Dockerfile",
    port,
    requiredEnv: [
      "ANTHROPIC_API_KEY",
      ...(opts.requiredEnv ?? []),
    ],
    defaultEnv: {
      NEXUSCLAW_PORT: String(port),
      NEXUSCLAW_BIND: "0.0.0.0",
    },
    resources: {
      minMemoryMb: 512,
      recommendedMemoryMb: 1024,
      minCpus: 0.5,
      persistentDirs: ["~/.anyclaw"],
    },
    healthCheck: {
      path: "/api/health",
      intervalSeconds: 30,
      timeoutSeconds: 5,
    },
    domainHint: opts.domainHint,
    generatedAt: new Date().toISOString(),
  };
}

export function writeHostingDescriptor(descriptor: HostingDeploymentDescriptor, projectRoot: string): string {
  const dir = join(projectRoot, ".nexus");
  mkdirSync(dir, { recursive: true });
  const outPath = join(dir, "hosting.json");
  writeFileSync(outPath, JSON.stringify(descriptor, null, 2), "utf-8");
  return outPath;
}
