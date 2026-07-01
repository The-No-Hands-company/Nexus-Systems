/**
 * DeepCode shared config loader for TypeScript tools.
 *
 * Discovers and merges settings from (in priority order, last wins):
 *   1. ~/.deepcode/settings.json   (user-global)
 *   2. ./.deepcode/settings.json   (project-local)
 *   3. env var DEEPCODE_SETTINGS   (comma-separated extra paths)
 *
 * Usage:
 *   import { loadSettings } from '../../.deepcode/dc_config.js';
 *   const settings = loadSettings();
 */

import { readFileSync } from 'node:fs';
import { homedir } from 'node:os';
import { join, resolve, dirname } from 'node:path';

export interface DeepCodeSettings {
  version: string;
  description: string;
  workspace: {
    name: string;
    root: string;
    company: string;
    program: string;
  };
  paths: Record<string, string>;
  scan: {
    excludeDirectories: string[];
    includeRoots: string[];
    sourceExtensions: string[];
    markdownExtensions: string[];
  };
  execution: {
    timeoutSeconds: number;
    maxConcurrency: number;
    httpTimeoutSeconds: number;
    retryAttempts: number;
  };
  wip: {
    maxActiveGlobal: number;
    maxActivePerLane: number;
  };
  ai: {
    proxyPort: number;
    defaultProvider: string;
    failoverProvider: string;
    budgetAlertThreshold: number;
  };
  cadence: Record<string, string[]>;
  pipelines: Record<string, string[]>;
  qualityGates: Record<string, { description: string; workflows: string[] }>;
  workflows: Record<string, { cwd: string; command: string }>;
  [key: string]: unknown;
}

function deepMerge<T extends Record<string, unknown>>(
  target: T,
  source: Record<string, unknown>,
): T {
  for (const key of Object.keys(source)) {
    if (
      key in target &&
      typeof target[key] === 'object' &&
      !Array.isArray(target[key]) &&
      typeof source[key] === 'object' &&
      !Array.isArray(source[key])
    ) {
      target[key] = deepMerge(
        target[key] as Record<string, unknown>,
        source[key] as Record<string, unknown>,
      ) as T[Extract<keyof T, string>];
    } else {
      (target as Record<string, unknown>)[key] = source[key];
    }
  }
  return target;
}

function tryReadJson(path: string): Record<string, unknown> | null {
  try {
    return JSON.parse(readFileSync(path, 'utf-8'));
  } catch {
    return null;
  }
}

function findProjectRoot(start?: string): string {
  let dir = start ? resolve(start) : process.cwd();
  // eslint-disable-next-line no-constant-condition
  while (true) {
    try {
      readFileSync(join(dir, '.deepcode', 'settings.json'));
      return dir;
    } catch {
      /* not here */
    }
    const parent = dirname(dir);
    if (parent === dir) break;
    dir = parent;
  }
  return process.cwd();
}

export function loadSettings(startDir?: string): DeepCodeSettings {
  let settings: Record<string, unknown> = {};

  // 1) user-global
  const globalPath = join(homedir(), '.deepcode', 'settings.json');
  const global = tryReadJson(globalPath);
  if (global) settings = deepMerge(settings, global);

  // 2) project-local
  const projectRoot = findProjectRoot(startDir);
  const projectPath = join(projectRoot, '.deepcode', 'settings.json');
  const project = tryReadJson(projectPath);
  if (project) settings = deepMerge(settings, project);

  // 3) DEEPCODE_SETTINGS env var
  const envPaths = process.env.DEEPCODE_SETTINGS || '';
  for (const p of envPaths.split(',')) {
    const trimmed = p.trim();
    if (trimmed) {
      const extra = tryReadJson(trimmed);
      if (extra) settings = deepMerge(settings, extra);
    }
  }

  return settings as unknown as DeepCodeSettings;
}

export function resolvePath(settings: DeepCodeSettings, key: string): string {
  const paths = settings.paths || {};
  const raw = paths[key];
  if (!raw) throw new Error(`paths.${key} not found in settings`);
  const dhtsRoot = paths.dhtsRoot || '.';
  return resolve(dhtsRoot, raw);
}
