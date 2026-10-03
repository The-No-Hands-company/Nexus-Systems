import path from "node:path";
import { federationEndpoints } from "./api/federation";
import { handleApi, json } from "./api/routes";
import { handleSmartHttp } from "./git/smart-http";
import type { ForgeDB } from "./storage/db";
import type { RepositoryManager } from "./storage/repository";

export interface ForgeOptions {
  db: ForgeDB;
  repos: RepositoryManager;
  publicUrl?: string;
  hooksDir?: string;
  maxPushBytes?: number;
}

export const DEFAULT_HOOKS_DIR = path.join(import.meta.dir, "hooks");

export function createForge(options: ForgeOptions) {
  const smartHttp = {
    db: options.db,
    repos: options.repos,
    hooksDir: options.hooksDir ?? DEFAULT_HOOKS_DIR,
    maxBodyBytes: options.maxPushBytes ?? 512 * 1024 * 1024,
  };
  const api = {
    db: options.db,
    repos: options.repos,
    ...(options.publicUrl ? { publicUrl: options.publicUrl } : {}),
  };

  async function fetch(request: Request): Promise<Response> {
    const url = new URL(request.url);
    if (url.pathname === "/health") {
      return json(200, {
        service: "nexus-forge",
        status: "ok",
        timestamp: new Date().toISOString(),
      });
    }
    if (url.pathname === "/.well-known/nexus-cloud") {
      return json(200, federationEndpoints.wellKnown());
    }
    try {
      return (
        (await handleSmartHttp(request, smartHttp)) ??
        (await handleApi(request, api)) ??
        json(404, { error: "not found" })
      );
    } catch (error) {
      console.error("[forge] request failed", error);
      return json(500, { error: "internal error" });
    }
  }

  return { fetch };
}
