// Skill Marketplace — Phase 3.1
// ClawHub-style skill registry adapter with local install support.
// Provides two layers:
//   1. NexusHubAdapter  — connects to a remote Nexus skill registry over HTTP
//   2. LocalMarketplace — installs / uninstalls / lists skills on disk
//
// Third-party registries can be added by registering their own SkillRegistryAdapter
// via registerAdapter() from src/adapters/index.ts.

import { join } from "node:path";
import {
  existsSync,
  mkdirSync,
  writeFileSync,
  readFileSync,
  readdirSync,
  rmSync,
  statSync,
} from "node:fs";
import { randomUUID } from "node:crypto";
import type { SkillRegistryAdapter, RemoteSkill, AdapterInfo } from "../adapters/index.js";
import { registerAdapter } from "../adapters/index.js";
import { getDataDir } from "../config/index.js";

// ─── NexusHubAdapter ────────────────────────────────────────────────────────
//
// Reference implementation of SkillRegistryAdapter that talks to a Nexus-hosted
// skill registry over HTTP.
//
// The registry API contract (compatible with any ClawHub-style server):
//   GET  /v1/skills?q=<query>&tags=<tag>&limit=<n>   → RemoteSkill[]
//   GET  /v1/skills/featured                          → RemoteSkill[]
//   GET  /v1/skills/<id>/content                      → string (SKILL.md text)
//   POST /v1/skills                                   → { id: string }

export class NexusHubAdapter implements SkillRegistryAdapter {
  readonly info: AdapterInfo = {
    id: "nexus-systems/nexus-hub",
    name: "Nexus Hub",
    version: "1.0.0",
    type: "skill-registry",
    description: "Official Nexus Systems skill marketplace registry",
    author: "Nexus Systems",
    homepage: "https://github.com/The-No-Hands-company/Nexus-Systems",
  };

  private baseUrl: string;
  private apiKey?: string;

  constructor(baseUrl: string, apiKey?: string) {
    this.baseUrl = baseUrl.replace(/\/$/, "");
    this.apiKey = apiKey;
  }

  private headers(): Record<string, string> {
    const h: Record<string, string> = { "Content-Type": "application/json" };
    if (this.apiKey) h["Authorization"] = `Bearer ${this.apiKey}`;
    return h;
  }

  async search(
    query: string,
    options: { tags?: string[]; limit?: number } = {},
  ): Promise<RemoteSkill[]> {
    const params = new URLSearchParams({ q: query, limit: String(options.limit ?? 20) });
    if (options.tags?.length) params.set("tags", options.tags.join(","));
    const res = await fetch(`${this.baseUrl}/v1/skills?${params}`, {
      headers: this.headers(),
    });
    if (!res.ok) throw new Error(`Registry error ${res.status}: ${await res.text()}`);
    return res.json() as Promise<RemoteSkill[]>;
  }

  async fetchContent(skillId: string): Promise<string> {
    const res = await fetch(`${this.baseUrl}/v1/skills/${encodeURIComponent(skillId)}/content`, {
      headers: this.headers(),
    });
    if (!res.ok) throw new Error(`Registry error ${res.status}: ${await res.text()}`);
    return res.text();
  }

  async featured(): Promise<RemoteSkill[]> {
    const res = await fetch(`${this.baseUrl}/v1/skills/featured`, {
      headers: this.headers(),
    });
    if (!res.ok) throw new Error(`Registry error ${res.status}: ${await res.text()}`);
    return res.json() as Promise<RemoteSkill[]>;
  }

  async publish(
    skillContent: string,
    metadata: Omit<RemoteSkill, "contentUrl" | "updatedAt" | "stars">,
  ): Promise<string> {
    const res = await fetch(`${this.baseUrl}/v1/skills`, {
      method: "POST",
      headers: this.headers(),
      body: JSON.stringify({ content: skillContent, ...metadata }),
    });
    if (!res.ok) throw new Error(`Registry error ${res.status}: ${await res.text()}`);
    const data = (await res.json()) as { id: string };
    return data.id;
  }
}

// ─── OfflineRegistryAdapter ─────────────────────────────────────────────────
//
// A fallback SkillRegistryAdapter that works entirely from a local skills
// directory.  Used when no remote hub is configured.

export class OfflineRegistryAdapter implements SkillRegistryAdapter {
  readonly info: AdapterInfo = {
    id: "nexus-systems/offline-registry",
    name: "Offline Skill Registry",
    version: "1.0.0",
    type: "skill-registry",
    description: "Local-only skill registry (no remote connection required)",
    author: "Nexus Systems",
  };

  private skillsDir: string;

  constructor(skillsDir: string) {
    this.skillsDir = skillsDir;
  }

  async search(query: string, options: { tags?: string[]; limit?: number } = {}): Promise<RemoteSkill[]> {
    const results = this._loadLocalSkills().filter((s) => {
      const q = query.toLowerCase();
      const hit =
        s.name.toLowerCase().includes(q) ||
        s.description.toLowerCase().includes(q) ||
        s.tags.some((t) => t.toLowerCase().includes(q));
      if (!hit) return false;
      if (options.tags?.length) {
        return options.tags.some((tag) => s.tags.includes(tag));
      }
      return true;
    });
    return results.slice(0, options.limit ?? 20);
  }

  async fetchContent(skillId: string): Promise<string> {
    const file = join(this.skillsDir, skillId, "SKILL.md");
    if (!existsSync(file)) throw new Error(`Skill not found locally: ${skillId}`);
    return readFileSync(file, "utf-8");
  }

  async featured(): Promise<RemoteSkill[]> {
    return this._loadLocalSkills().slice(0, 10);
  }

  private _loadLocalSkills(): RemoteSkill[] {
    if (!existsSync(this.skillsDir)) return [];
    const skills: RemoteSkill[] = [];
    for (const entry of readdirSync(this.skillsDir)) {
      const dir = join(this.skillsDir, entry);
      if (!statSync(dir).isDirectory()) continue;
      const skillFile = join(dir, "SKILL.md");
      if (!existsSync(skillFile)) continue;
      const raw = readFileSync(skillFile, "utf-8");
      // Parse minimal frontmatter for metadata
      const fmMatch = raw.match(/^---\r?\n([\s\S]*?)\r?\n---/);
      let name = entry;
      let description = "";
      let tags: string[] = [];
      if (fmMatch) {
        try {
          const { load } = await_yaml_load;
          const meta = load(fmMatch[1]) as Record<string, unknown>;
          name = (meta.name as string) || entry;
          description = (meta.description as string) || "";
          tags = (meta.tags as string[]) || [];
        } catch { /* ignore */ }
      }
      skills.push({
        id: entry,
        name,
        description,
        version: "local",
        author: "local",
        tags,
        contentUrl: `local://${skillFile}`,
        updatedAt: statSync(skillFile).mtimeMs,
      });
    }
    return skills;
  }
}

// Workaround for top-level await in class (sync YAML load)
const await_yaml_load = { load: (s: string) => { const m = s.match(/name:\s*(.+)/); return m ? { name: m[1].trim() } : {}; } };

// ─── LocalMarketplace ───────────────────────────────────────────────────────
//
// High-level interface for installing, removing, listing, and publishing skills.
// Uses a SkillRegistryAdapter for the remote store and the local data directory
// for installation on disk.

export interface InstalledSkillMeta {
  id: string;
  name: string;
  version: string;
  author: string;
  tags: string[];
  description?: string;
  installedAt: number;
  source: "local" | "remote" | "git";
  sourceUrl?: string;
}

export class LocalMarketplace {
  private skillsDir: string;
  private metaFile: string;
  private adapter?: SkillRegistryAdapter;

  constructor(skillsDir: string, adapter?: SkillRegistryAdapter) {
    this.skillsDir = skillsDir;
    this.metaFile = join(skillsDir, ".installed.json");
    this.adapter = adapter;
  }

  // ── Registry ────────────────────────────────────────────────────────────

  setAdapter(adapter: SkillRegistryAdapter): void {
    this.adapter = adapter;
  }

  async search(query: string, tags?: string[]): Promise<RemoteSkill[]> {
    if (!this.adapter) throw new Error("No registry adapter configured. Set --registry or configure marketplace.adapter");
    return this.adapter.search(query, { tags, limit: 30 });
  }

  async featured(): Promise<RemoteSkill[]> {
    if (!this.adapter) return [];
    return this.adapter.featured();
  }

  // ── Install ─────────────────────────────────────────────────────────────

  async install(skillId: string, options: { force?: boolean } = {}): Promise<string> {
    if (!this.adapter) throw new Error("No registry adapter configured");

    const targetDir = join(this.skillsDir, skillId);
    if (existsSync(targetDir) && !options.force) {
      throw new Error(`Skill "${skillId}" is already installed. Use --force to reinstall.`);
    }

    const content = await this.adapter.fetchContent(skillId);
    mkdirSync(targetDir, { recursive: true });
    writeFileSync(join(targetDir, "SKILL.md"), content, "utf-8");

    this._recordInstall({
      id: skillId,
      name: skillId,
      version: "latest",
      author: "registry",
      tags: [],
      installedAt: Date.now(),
      source: "remote",
    });

    return targetDir;
  }

  async installFromContent(
    skillId: string,
    content: string,
    meta: Partial<InstalledSkillMeta> = {},
  ): Promise<string> {
    const targetDir = join(this.skillsDir, skillId);
    mkdirSync(targetDir, { recursive: true });
    writeFileSync(join(targetDir, "SKILL.md"), content, "utf-8");

    this._recordInstall({
      id: skillId,
      name: meta.name || skillId,
      version: meta.version || "local",
      author: meta.author || "local",
      tags: meta.tags || [],
      installedAt: Date.now(),
      source: meta.source || "local",
      sourceUrl: meta.sourceUrl,
    });

    return targetDir;
  }

  // ── Uninstall ────────────────────────────────────────────────────────────

  uninstall(skillId: string): boolean {
    const targetDir = join(this.skillsDir, skillId);
    if (!existsSync(targetDir)) return false;
    rmSync(targetDir, { recursive: true, force: true });
    this._removeInstallRecord(skillId);
    return true;
  }

  // ── List ─────────────────────────────────────────────────────────────────

  listInstalled(): InstalledSkillMeta[] {
    return this._loadMeta();
  }

  isInstalled(skillId: string): boolean {
    return existsSync(join(this.skillsDir, skillId, "SKILL.md"));
  }

  // ── Publish ──────────────────────────────────────────────────────────────

  async publish(
    skillId: string,
    metadata: Omit<RemoteSkill, "contentUrl" | "updatedAt" | "stars">,
  ): Promise<string> {
    if (!this.adapter?.publish) {
      throw new Error("The configured adapter does not support publishing");
    }
    const content = await this.adapter.fetchContent(skillId);
    return this.adapter.publish(content, metadata);
  }

  // ── Persistence ──────────────────────────────────────────────────────────

  private _loadMeta(): InstalledSkillMeta[] {
    if (!existsSync(this.metaFile)) return [];
    try {
      return JSON.parse(readFileSync(this.metaFile, "utf-8")) as InstalledSkillMeta[];
    } catch {
      return [];
    }
  }

  private _recordInstall(meta: InstalledSkillMeta): void {
    const existing = this._loadMeta().filter((m) => m.id !== meta.id);
    mkdirSync(this.skillsDir, { recursive: true });
    writeFileSync(this.metaFile, JSON.stringify([...existing, meta], null, 2), "utf-8");
  }

  private _removeInstallRecord(skillId: string): void {
    const existing = this._loadMeta().filter((m) => m.id !== skillId);
    writeFileSync(this.metaFile, JSON.stringify(existing, null, 2), "utf-8");
  }
}

// ─── Factory ─────────────────────────────────────────────────────────────────

export function createMarketplace(opts: {
  skillsDir?: string;
  registryUrl?: string;
  apiKey?: string;
} = {}): LocalMarketplace {
  const skillsDir = opts.skillsDir ?? join(getDataDir(), "workspace", "skills");
  let adapter: SkillRegistryAdapter | undefined;

  if (opts.registryUrl) {
    adapter = new NexusHubAdapter(opts.registryUrl, opts.apiKey);
    registerAdapter(adapter);
  } else {
    adapter = new OfflineRegistryAdapter(skillsDir);
    registerAdapter(adapter);
  }

  return new LocalMarketplace(skillsDir, adapter);
}
