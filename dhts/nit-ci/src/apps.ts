import path from "path";

const REPO = path.resolve(import.meta.dirname, "../../..");

export interface MockRoute {
  pattern: string | RegExp;
  json: unknown;
}

export interface NitApp {
  name: string;
  frontendDir: string;
  buildCmd: string;
  distDir: string;
  waitForSelector: string;
  waitTimeout: number;
  mocks: MockRoute[];
  waitMs?: number;
}

export const APPS: NitApp[] = [
  {
    name: "Calendar",
    frontendDir: path.join(REPO, "apps/Nexus-Calendar/frontend"),
    buildCmd: "bun run build",
    distDir: path.join(REPO, "apps/Nexus-Calendar/frontend/dist"),
    waitForSelector: '[role="grid"], main',
    waitTimeout: 8000,
    waitMs: 1000,
    mocks: [
      // least-specific first (registered first = lowest priority)
      { pattern: /\/api\/v1\/calendar\//, json: {} },
      // most-specific last (registered last = highest priority)
      { pattern: /\/api\/v1\/calendar\/events/, json: { events: [] } },
    ],
  },
  {
    name: "Project",
    frontendDir: path.join(REPO, "apps/Nexus-Project/frontend"),
    buildCmd: "bun run build",
    distDir: path.join(REPO, "apps/Nexus-Project/frontend/dist"),
    waitForSelector: "main",
    waitTimeout: 8000,
    waitMs: 1000,
    mocks: [
      // least-specific first
      { pattern: /\/api\/v1\/project\//, json: {} },
      // most-specific last (highest priority)
      { pattern: /\/api\/v1\/project\/me\/tasks/, json: { tasks: [] } },
      { pattern: /\/api\/v1\/project\/workspaces$/, json: { workspaces: [] } },
    ],
  },
  {
    name: "Draw",
    frontendDir: path.join(REPO, "apps/Nexus-Draw/frontend"),
    buildCmd: "bun run build",
    distDir: path.join(REPO, "apps/Nexus-Draw/frontend/dist"),
    waitForSelector: '[role="toolbar"]',
    waitTimeout: 10000,
    waitMs: 1000,
    mocks: [
      // least-specific first
      { pattern: /\/api\//, json: {} },
      // boards list (most-specific last = highest priority)
      { pattern: /\/api\/boards$/, json: [] },
    ],
  },
];

export const VIEWPORTS = [
  { name: "phone", width: 375, height: 812 },
  { name: "desktop", width: 1280, height: 800 },
] as const;
