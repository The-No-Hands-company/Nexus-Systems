// Plugin system — hot-loadable tools, providers, and hooks (Phase 10.1)
// Loads from: npm packages, local directories, single JS/TS files

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { join, resolve, isAbsolute } from "node:path";
import { pathToFileURL } from "node:url";
import type { Tool, Skill } from "../core/types.js";
import type { LLMProvider } from "../llm/index.js";

// ─── Plugin Manifest ────────────────────────────────────────────────────

export interface PluginManifest {
  name: string;
  version: string;
  description?: string;
  /** Entry point (relative to plugin dir or absolute) */
  main?: string;
  /** What this plugin provides */
  provides?: ("tools" | "provider" | "skill" | "hooks")[];
}

export interface LoadedPlugin {
  manifest: PluginManifest;
  tools: Tool[];
  providers: { prefix: string; provider: LLMProvider }[];
  skills: Skill[];
  hooks: PluginHook[];
  source: string;
}

export interface PluginHook {
  event: "before_tool" | "after_tool" | "before_llm" | "after_llm" | "on_message" | "on_error";
  handler: (...args: any[]) => void | Promise<void>;
}

// ─── Plugin Loader ──────────────────────────────────────────────────────

export class PluginManager {
  private plugins = new Map<string, LoadedPlugin>();
  private pluginDirs: string[] = [];

  constructor(pluginDirs?: string[]) {
    this.pluginDirs = pluginDirs || [];
  }

  /** Load a plugin from a directory containing a plugin manifest (plugin.json or package.json) */
  async loadFromDir(dir: string): Promise<LoadedPlugin> {
    const resolved = resolve(dir);
    if (!existsSync(resolved)) throw new Error(`Plugin directory not found: ${resolved}`);

    // Read manifest
    const manifest = this.readManifest(resolved);

    // Import the entry point
    const entryFile = manifest.main || "index.js";
    const entryPath = join(resolved, entryFile);
    if (!existsSync(entryPath)) throw new Error(`Plugin entry not found: ${entryPath}`);

    const module = await import(pathToFileURL(entryPath).href);
    return this.processModule(module, manifest, resolved);
  }

  /** Load a plugin from a single JS/TS file */
  async loadFromFile(filepath: string): Promise<LoadedPlugin> {
    const resolved = resolve(filepath);
    if (!existsSync(resolved)) throw new Error(`Plugin file not found: ${resolved}`);

    const manifest: PluginManifest = {
      name: resolved.split("/").pop()?.replace(/\.[jt]s$/, "") || "unnamed",
      version: "0.0.0",
    };

    const module = await import(pathToFileURL(resolved).href);
    return this.processModule(module, manifest, resolved);
  }

  /** Load a plugin from an npm package name */
  async loadFromPackage(packageName: string): Promise<LoadedPlugin> {
    const module = await import(packageName);
    const manifest: PluginManifest = {
      name: packageName,
      version: module.version || "0.0.0",
      description: module.description,
    };
    return this.processModule(module, manifest, `npm:${packageName}`);
  }

  /** Discover and load all plugins from configured directories */
  async discoverPlugins(): Promise<{ loaded: string[]; errors: { path: string; error: string }[] }> {
    const loaded: string[] = [];
    const errors: { path: string; error: string }[] = [];

    for (const dir of this.pluginDirs) {
      if (!existsSync(dir)) continue;

      for (const entry of readdirSync(dir)) {
        const fullPath = join(dir, entry);
        if (!statSync(fullPath).isDirectory()) continue;

        try {
          const plugin = await this.loadFromDir(fullPath);
          this.plugins.set(plugin.manifest.name, plugin);
          loaded.push(plugin.manifest.name);
        } catch (err: any) {
          errors.push({ path: fullPath, error: err.message });
        }
      }
    }

    return { loaded, errors };
  }

  /** Get all loaded plugins */
  getPlugins(): LoadedPlugin[] {
    return [...this.plugins.values()];
  }

  /** Get all tools from all plugins */
  getAllTools(): Tool[] {
    return this.getPlugins().flatMap((p) => p.tools);
  }

  /** Get all providers from all plugins */
  getAllProviders(): { prefix: string; provider: LLMProvider }[] {
    return this.getPlugins().flatMap((p) => p.providers);
  }

  /** Get all skills from all plugins */
  getAllSkills(): Skill[] {
    return this.getPlugins().flatMap((p) => p.skills);
  }

  /** Get all hooks for a specific event */
  getHooks(event: PluginHook["event"]): PluginHook["handler"][] {
    return this.getPlugins()
      .flatMap((p) => p.hooks)
      .filter((h) => h.event === event)
      .map((h) => h.handler);
  }

  /** Unload a plugin by name */
  unload(name: string): boolean {
    return this.plugins.delete(name);
  }

  // ─── Internal ─────────────────────────────────────────────────────

  private readManifest(dir: string): PluginManifest {
    // Try plugin.json first, then package.json
    const pluginJsonPath = join(dir, "plugin.json");
    if (existsSync(pluginJsonPath)) {
      return JSON.parse(readFileSync(pluginJsonPath, "utf-8")) as PluginManifest;
    }

    const packageJsonPath = join(dir, "package.json");
    if (existsSync(packageJsonPath)) {
      const pkg = JSON.parse(readFileSync(packageJsonPath, "utf-8"));
      return {
        name: pkg.name || "unnamed",
        version: pkg.version || "0.0.0",
        description: pkg.description,
        main: pkg.main || pkg.exports?.["."] || "index.js",
      };
    }

    return { name: dir.split("/").pop() || "unnamed", version: "0.0.0", main: "index.js" };
  }

  private processModule(module: any, manifest: PluginManifest, source: string): LoadedPlugin {
    const plugin: LoadedPlugin = {
      manifest,
      tools: [],
      providers: [],
      skills: [],
      hooks: [],
      source,
    };

    // Convention: export `tools` as Tool[]
    if (Array.isArray(module.tools)) {
      plugin.tools = module.tools;
    }
    // Or a single tool
    if (module.tool && typeof module.tool === "object" && module.tool.name) {
      plugin.tools.push(module.tool);
    }

    // Convention: export `providers` as { prefix, provider }[]
    if (Array.isArray(module.providers)) {
      plugin.providers = module.providers;
    }
    if (module.provider && typeof module.provider === "object") {
      plugin.providers.push(module.provider);
    }

    // Convention: export `skills` as Skill[]
    if (Array.isArray(module.skills)) {
      plugin.skills = module.skills;
    }
    if (module.skill && typeof module.skill === "object") {
      plugin.skills.push(module.skill);
    }

    // Convention: export `hooks` as PluginHook[]
    if (Array.isArray(module.hooks)) {
      plugin.hooks = module.hooks;
    }

    // Convention: export `setup(context)` for programmatic registration
    if (typeof module.setup === "function") {
      const ctx = {
        registerTool: (t: Tool) => plugin.tools.push(t),
        registerProvider: (prefix: string, provider: LLMProvider) =>
          plugin.providers.push({ prefix, provider }),
        registerSkill: (s: Skill) => plugin.skills.push(s),
        registerHook: (h: PluginHook) => plugin.hooks.push(h),
      };
      // setup can be sync or async — handle both
      const result = module.setup(ctx);
      if (result && typeof result.then === "function") {
        // Return a promise-aware plugin — caller must await
        // For simplicity, we handle this synchronously; async setup is best-effort
      }
    }

    this.plugins.set(manifest.name, plugin);
    return plugin;
  }
}
