// AnyClaw — Skills System (Phase 5)
// Loads, parses, and manages skill definitions
// Skills are markdown files with optional YAML frontmatter

import { readFileSync, existsSync, readdirSync, statSync } from "node:fs";
import { join, basename, dirname } from "node:path";
import { load as yamlLoad } from "js-yaml";
import type { Skill } from "../core/types.js";

// ─── Skill Frontmatter ─────────────────────────────────────────────────

interface SkillFrontmatter {
  name?: string;
  description?: string;
  enabled?: boolean;
  tools?: string[];
  tags?: string[];
  when?: string;
}

// ─── Parse SKILL.md ─────────────────────────────────────────────────────

function parseFrontmatter(content: string): { meta: SkillFrontmatter; body: string } {
  const fmMatch = content.match(/^---\r?\n([\s\S]*?)\r?\n---\r?\n([\s\S]*)$/);
  if (fmMatch) {
    try {
      const meta = yamlLoad(fmMatch[1]) as SkillFrontmatter;
      return { meta: meta || {}, body: fmMatch[2] };
    } catch {
      return { meta: {}, body: content };
    }
  }
  return { meta: {}, body: content };
}

/**
 * Load a single skill from a file path
 */
export function loadSkillFromFile(filePath: string, id?: string): Skill | null {
  if (!existsSync(filePath)) return null;

  try {
    const raw = readFileSync(filePath, "utf-8");
    const { meta, body } = parseFrontmatter(raw);
    const skillId = id || meta.name || basename(dirname(filePath));

    return {
      id: skillId,
      name: meta.name || skillId,
      description: meta.description || `Skill from ${filePath}`,
      instructions: body.trim(),
      enabled: meta.enabled !== false,
      source: filePath,
      // tools from frontmatter are name references, not Tool instances
      tags: meta.tags,
      when: meta.when,
    } satisfies Skill;
  } catch {
    return null;
  }
}

/**
 * Discover skills from a directory.
 * Looks for:
 *  - <dir>/<name>/SKILL.md
 *  - <dir>/<name>.md
 */
export function discoverSkills(dir: string): Skill[] {
  if (!existsSync(dir)) return [];

  const skills: Skill[] = [];

  for (const entry of readdirSync(dir)) {
    const fullPath = join(dir, entry);
    const stat = statSync(fullPath);

    if (stat.isDirectory()) {
      // Look for SKILL.md inside subdirectory
      const skillFile = join(fullPath, "SKILL.md");
      if (existsSync(skillFile)) {
        const skill = loadSkillFromFile(skillFile, entry);
        if (skill) skills.push(skill);
      }
    } else if (entry.endsWith(".md") && entry !== "README.md") {
      // Treat .md files as skills
      const name = entry.replace(/\.md$/, "");
      const skill = loadSkillFromFile(fullPath, name);
      if (skill) skills.push(skill);
    }
  }

  return skills;
}

/**
 * Load skills from multiple sources:
 * 1. Configured skills (explicit paths in config)
 * 2. Workspace skills directory
 * 3. User-level skills (~/.anyclaw/skills/)
 */
export function loadAllSkills(
  configured: Record<string, { path: string; enabled: boolean }>,
  extraDirs: string[] = [],
): Skill[] {
  const seen = new Set<string>();
  const skills: Skill[] = [];

  // 1. Explicit configured skills
  for (const [id, conf] of Object.entries(configured)) {
    if (!conf.enabled) continue;
    const skill = loadSkillFromFile(conf.path, id);
    if (skill) {
      skills.push(skill);
      seen.add(skill.id);
    }
  }

  // 2. Auto-discover from directories
  for (const dir of extraDirs) {
    const discovered = discoverSkills(dir);
    for (const skill of discovered) {
      if (!seen.has(skill.id)) {
        skills.push(skill);
        seen.add(skill.id);
      }
    }
  }

  return skills;
}

/**
 * Build a system prompt injection for active skills
 */
export function buildSkillsPrompt(skills: Skill[]): string {
  const active = skills.filter((s) => s.enabled);
  if (active.length === 0) return "";

  const parts = ["<skills>"];
  for (const skill of active) {
    parts.push(`<skill name="${skill.name}">`);
    if (skill.description) parts.push(`Description: ${skill.description}`);
    parts.push(skill.instructions);
    parts.push(`</skill>`);
  }
  parts.push("</skills>");
  return parts.join("\n");
}

/**
 * Filter skills by tag or conditional 'when' expression
 */
export function filterSkills(
  skills: Skill[],
  opts: { tags?: string[]; context?: Record<string, string> } = {},
): Skill[] {
  return skills.filter((skill) => {
    if (!skill.enabled) return false;

    // Tag filter
    if (opts.tags && skill.tags) {
      const hasMatch = skill.tags.some((t) => opts.tags!.includes(t));
      if (!hasMatch) return false;
    }

    // 'when' conditional (simple key=value check against context)
    if (skill.when && opts.context) {
      const [key, value] = skill.when.split("=").map((s) => s.trim());
      if (key && value && opts.context[key] !== value) return false;
    }

    return true;
  });
}
