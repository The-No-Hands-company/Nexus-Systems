# Nexus-Draw: Server Sync + Board Management, Real-Time Collaboration, AI Generation

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn Nexus-Draw from a localStorage-only single-user editor into a real-time collaborative whiteboard backed by the Bun server: server persistence + board management UI, live multi-user editing (yjs over WebSocket), and an AI diagram-generation panel that builds a real synthesis and drops its output onto the canvas.

**Architecture:** The Bun gateway (`apps/Nexus-Draw/src/server.ts`) already owns a SQLite `DrawEngine` and is the frontend's `/api` proxy target (Vite: 5173 → 3075). We extend that engine (board metadata columns + delete + AI synthesis), add REST routes (PATCH/DELETE board, POST `/api/v1/draw/ai/generate`), expose a y-websocket-compatible WebSocket endpoint (`/api/v1/draw/ws/:boardId`) implemented with `yjs` + `y-protocols` + `lib0`, and persist the live `Y.Doc` back into SQLite on update. The frontend gains an `api.ts` client, an `AI` panel side tab, a `collab/` module that binds the zustand `elements` array to a `Y.Map`+`Y.Array` (per-element CRDT granularity), and a board-list panel. The FastAPI/Celery AI route remains unchanged for future model integration; the Bun gateway answers the same `/api/v1/draw/ai/generate` path synchronously so the UI works without Redis/Postgres.

**Tech Stack:** Bun 1.3 (server, SQLite, WebSocket), yjs + y-protocols + lib0 + y-websocket (CRDT sync), React 19 + zustand + Vite + vitest (frontend). All logic-carrying modules are unit-tested; the collab server gets a real two-socket integration test via `bun --socket`/`WebSocket`.

## Global Constraints

- Work dir: `apps/Nexus-Draw` (Bun server + `tests/`) and `apps/Nexus-Draw/frontend` (React). Monorepo dir — plain commits.
- Build gates: `cd apps/Nexus-Draw/frontend && bun run check` (`tsc --noEmit`); `cd apps/Nexus-Draw && bun run check`; tests `bun test` (both dirs); frontend `bun test` (vitest). All must stay green.
- Only the Butterlow: keep changes minimal and reversible; commit after every green step with conventional messages.
- The zustand store stays the UI source of truth. `setElementsLive` is the no-history live-write path used for X → Y and Y → X binding during collab; never spin history loops.
- The frontend keeps localStorage autosave as an offline fallback; the server is the source of truth when reachable.
- No new heavy infra required to run: AI generation is rule-based in the Bun gateway (honest, offline, testable). FastAPI/Celery stays untouched.
- Do NOT touch `apps/Nexus-Modeling/build/`, `VersaAI-LegecyOnly-do-not-touch/`, `Backups/`.

---

## Phase A — Server persistence + board management

### Task A1: Extend DrawEngine (SQLite) with board metadata + delete

**Files:**
- Modify: `apps/Nexus-Draw/src/draw-engine.ts`
- Test: `apps/Nexus-Draw/tests/draw-engine.test.ts` (create)

**Interfaces:**
- Consumes: existing `DrawEngine(dbPath)`, `Whiteboard { id, name, elements, collaborators, createdAt, updatedAt }`.
- Produces:
  - `interface BoardMeta { id:string; name:string; description:string; width:number; height:number; background:string; isPublic:boolean; defaultStyleMode:"clean"|"sketch"; gridSnap:boolean; createdAt:string; updatedAt:string }`
  - `createBoard(name): Whiteboard` (unchanged signature; now also fills new columns with defaults)
  - `getBoard(id): Whiteboard | undefined` (unchanged signature; round-trips new columns + elements)
  - `listBoards(): Whiteboard[]`
  - `updateBoardMeta(id, meta: Partial<Omit<BoardMeta,"id"|"createdAt">>): boolean`
  - `deleteBoard(id): boolean`
  - `updateElements(id, elements): void` (unchanged; updates `updated_at`)

- [x] **Step 1: Write the failing tests**

```ts
// apps/Nexus-Draw/tests/draw-engine.test.ts
import { describe, it, expect, beforeEach } from "bun:test";
import { DrawEngine } from "../src/draw-engine";

describe("DrawEngine board metadata", () => {
  let engine: DrawEngine;
  beforeEach(() => { engine = new DrawEngine(":memory:"); });

  it("createBoard fills metadata defaults", () => {
    const b = engine.createBoard("Hello");
    expect(b.name).toBe("Hello");
    expect(b.background).toBe("#1a1a2e");
    expect(b.gridSnap).toBe(false);
    expect(b.defaultStyleMode).toBe("clean");
    expect(b.elements).toEqual([]);
  });

  it("updateBoardMeta patches only given fields and bumps updatedAt", () => {
    const b = engine.createBoard("Shapes");
    engine.updateBoardMeta(b.id, { gridSnap: true, background: "#0f0f0f" });
    const after = engine.getBoard(b.id)!;
    expect(after.gridSnap).toBe(true);
    expect(after.background).toBe("#0f0f0f");
    expect(after.name).toBe("Shapes"); // untouched
  });

  it("deleteBoard removes the board", () => {
    const b = engine.createBoard("Temp");
    expect(engine.deleteBoard(b.id)).toBe(true);
    expect(engine.getBoard(b.id)).toBeUndefined();
    expect(engine.deleteBoard(b.id)).toBe(false);
  });

  it("elements survive create→updateElements→get round-trip", () => {
    const b = engine.createBoard("Doc");
    engine.updateElements(b.id, [{ id: "e1", order: 0 }]);
    expect(engine.getBoard(b.id)!.elements).toEqual([{ id: "e1", order: 0 }]);
  });
});
```

- [x] **Step 2: Run to verify it fails** — `cd apps/Nexus-Draw && bun test tests/draw-engine.test.ts` → FAIL (missing methods/columns).

- [x] **Step 3: Implement `draw-engine.ts`**

Keep the existing table; run an idempotent migration that adds metadata columns if missing, then expose the new methods:

```ts
import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface Whiteboard {
  id: string; name: string; description: string;
  width: number; height: number; background: string; isPublic: boolean;
  defaultStyleMode: "clean" | "sketch"; gridSnap: boolean;
  elements: unknown[]; collaborators: string[];
  createdAt: string; updatedAt: string;
}

function migrate(db: Database): void {
  db.exec(`CREATE TABLE IF NOT EXISTS boards (
    id TEXT PRIMARY KEY, name TEXT, elements TEXT DEFAULT '[]',
    collaborators TEXT DEFAULT '[]', created_at TEXT, updated_at TEXT)`);
  const cols = new Set((db.prepare("PRAGMA table_info(boards)").all() as any[]).map((c) => c.name));
  const add = (name: string, decl: string) => {
    if (!cols.has(name)) db.exec(`ALTER TABLE boards ADD COLUMN ${name} ${decl}`);
  };
  add("description", "TEXT DEFAULT ''");
  add("width", "INTEGER DEFAULT 1920");
  add("height", "INTEGER DEFAULT 1080");
  add("background", "TEXT DEFAULT '#1a1a2e'");
  add("is_public", "INTEGER DEFAULT 0");
  add("default_style_mode", "TEXT DEFAULT 'clean'");
  add("grid_snap", "INTEGER DEFAULT 0");
  db.exec("CREATE INDEX IF NOT EXISTS idx_boards_updated ON boards(updated_at DESC)");
}

function rowToBoard(r: any): Whiteboard {
  return {
    id: r.id, name: r.name, description: r.description ?? "",
    width: r.width ?? 1920, height: r.height ?? 1080,
    background: r.background ?? "#1a1a2e", isPublic: !!r.is_public,
    defaultStyleMode: r.default_style_mode ?? "clean", gridSnap: !!r.grid_snap,
    elements: JSON.parse(r.elements), collaborators: JSON.parse(r.collaborators),
    createdAt: r.created_at, updatedAt: r.updated_at,
  };
}

export class DrawEngine {
  db: Database;
  constructor(p = ":memory:") { this.db = new Database(p); migrate(this.db); }
  createBoard(name: string): Whiteboard {
    const now = new Date().toISOString();
    const b: Whiteboard = { id: randomUUID(), name, description: "", width: 1920, height: 1080, background: "#1a1a2e", isPublic: false, defaultStyleMode: "clean", gridSnap: false, elements: [], collaborators: [], createdAt: now, updatedAt: now };
    this.db.prepare("INSERT INTO boards (id,name,description,width,height,background,is_public,default_style_mode,grid_snap,elements,collaborators,created_at,updated_at) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)")
      .run(b.id, b.name, b.description, b.width, b.height, b.background, b.isPublic ? 1 : 0, b.defaultStyleMode, b.gridSnap ? 1 : 0, JSON.stringify(b.elements), JSON.stringify(b.collaborators), b.createdAt, b.updatedAt);
    return b;
  }
  listBoards(): Whiteboard[] {
    return (this.db.prepare("SELECT * FROM boards ORDER BY updated_at DESC").all() as any[]).map(rowToBoard);
  }
  getBoard(id: string): Whiteboard | undefined {
    const r = this.db.prepare("SELECT * FROM boards WHERE id = ?").get(id) as any | null;
    return r ? rowToBoard(r) : undefined;
  }
  updateBoardMeta(id: string, meta: Partial<Omit<Whiteboard, "id" | "createdAt" | "elements" | "collaborators">>): boolean {
    const fields: Record<string, unknown> = {};
    if (meta.name !== undefined) fields.name = meta.name;
    if (meta.description !== undefined) fields.description = meta.description;
    if (meta.width !== undefined) fields.width = meta.width;
    if (meta.height !== undefined) fields.height = meta.height;
    if (meta.background !== undefined) fields.background = meta.background;
    if (meta.isPublic !== undefined) fields.is_public = meta.isPublic ? 1 : 0;
    if (meta.defaultStyleMode !== undefined) fields.default_style_mode = meta.defaultStyleMode;
    if (meta.gridSnap !== undefined) fields.grid_snap = meta.gridSnap ? 1 : 0;
    const keys = Object.keys(fields);
    if (keys.length === 0) return this.getBoard(id) !== undefined;
    fields.updated_at = new Date().toISOString();
    keys.push("updated_at");
    const sql = `UPDATE boards SET ${keys.map((k) => `${k} = ?`).join(", ")} WHERE id = ?`;
    const res = this.db.prepare(sql).run(...keys.map((k) => fields[k]!), id);
    return res.changes > 0;
  }
  deleteBoard(id: string): boolean {
    const res = this.db.prepare("DELETE FROM boards WHERE id = ?").run(id);
    return res.changes > 0;
  }
  updateElements(id: string, elements: unknown[]): void {
    this.db.prepare("UPDATE boards SET elements = ?, updated_at = ? WHERE id = ?")
      .run(JSON.stringify(elements), new Date().toISOString(), id);
  }
}
```

- [x] **Step 4: Run to verify it passes** — `cd apps/Nexus-Draw && bun test tests/draw-engine.test.ts` → 5 tests PASS.

- [x] **Step 5: Commit** — `git add apps/Nexus-Draw/src/draw-engine.ts apps/Nexus-Draw/tests/draw-engine.test.ts && git commit -m "feat(draw): SQLite board metadata columns, meta PATCH, delete"`

---

### Task A2: REST routes — PATCH/DELETE board + full-fidelity GET

**Files:**
- Modify: `apps/Nexus-Draw/src/server.ts`
- Test: `apps/Nexus-Draw/tests/server.test.ts` (append cases)

**Interfaces:**
- Consumes: `DrawEngine` methods from A1.
- Produces:
  - `PATCH /api/v1/draw/boards/:id` body `{ name?, description?, width?, height?, background?, isPublic?, defaultStyleMode?, gridSnap? }` → 200 `{ updated: true }`, 404 `{ error: "not found" }`.
  - `DELETE /api/v1/draw/boards/:id` → 200 `{ deleted: true }`, 404 `{ error: "not found" }`.
  - `GET /api/v1/draw/boards/:id` now returns ALL board fields (metadata + elements + collaborators), not just a `{...r, elements, collaborators}` strip — unchanged URL.

- [x] **Step 1: Write the failing route tests**

```ts
// apps/Nexus-Draw/tests/server.test.ts — append
import { describe, it, expect } from "bun:test";
import { createServer } from "../src/server";

describe("board routes", () => {
  it("PATCH updates board meta", async () => {
    const { server, close } = await createServer();
    const base = `http://localhost:${server.port}`;
    const created = await (await fetch(`${base}/api/v1/draw/boards`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Meta" }) })).json();
    let res = await fetch(`${base}/api/v1/draw/boards/${created.id}`, { method: "PATCH", headers: { "content-type": "application/json" }, body: JSON.stringify({ gridSnap: true, background: "#000000" }) });
    expect(res.status).toBe(200);
    const got = await (await fetch(`${base}/api/v1/draw/boards/${created.id}`)).json();
    expect(got.gridSnap).toBe(true);
    expect(got.background).toBe("#000000");
    close();
  });

  it("PATCH 404s for a missing board", async () => {
    const { server, close } = await createServer();
    const res = await fetch(`http://localhost:${server.port}/api/v1/draw/boards/nope`, { method: "PATCH", headers: { "content-type": "application/json" }, body: JSON.stringify({ gridSnap: true }) });
    expect(res.status).toBe(404);
    close();
  });

  it("DELETE removes a board and 404s the second time", async () => {
    const { server, close } = await createServer();
    const base = `http://localhost:${server.port}`;
    const created = await (await fetch(`${base}/api/v1/draw/boards`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Temp" }) })).json();
    expect((await fetch(`${base}/api/v1/draw/boards/${created.id}`, { method: "DELETE" })).status).toBe(200);
    expect((await fetch(`${base}/api/v1/draw/boards/${created.id}`)).status).toBe(404);
    close();
  });
});
```

- [x] **Step 2: Run to verify it fails** — `cd apps/Nexus-Draw && bun test tests/server.test.ts` → FAIL (route 404s).

- [x] **Step 3: Implement routes in `createServer`** — add after the existing `GET/POST /api/v1/draw/boards` lines:

```ts
// PATCH /api/v1/draw/boards/:id — update board metadata
const meta = p.match(/^\/api\/v1\/draw\/boards\/([^/]+)$/);
if (req.method === "PATCH" && meta) {
  const body = (await req.json().catch(() => ({}))) as Record<string, unknown>;
  const id = meta[1]!;
  const patch: Record<string, unknown> = {};
  for (const key of ["name", "description", "width", "height", "background", "isPublic", "defaultStyleMode", "gridSnap"] as const) {
    if (body[key] !== undefined) patch[key] = body[key];
  }
  return engine.updateBoardMeta(id, patch) ? json({ updated: true }) : json({ error: "not found" }, 404);
}
// DELETE /api/v1/draw/boards/:id
if (req.method === "DELETE" && dm) {
  return engine.deleteBoard(dm[1]!) ? json({ deleted: true }) : json({ error: "not found" }, 404);
}
```

Note: `dm` is the existing `p.match(/^\/api\/v1\/draw\/boards\/([^/]+)$/)` binding — reuse it rather than re-declaring; adjust so the DELETE branch comes after the existing `if (req.method === "GET" && dm)` and the PUT `/elements` branch (order matters only against those exact matchers, which are disjoint).

- [x] **Step 4: Run to verify it passes** — `cd apps/Nexus-Draw && bun test tests/server.test.ts` → PASS. Also `bun run check` → clean.

- [x] **Step 5: Commit** — `git add apps/Nexus-Draw/src/server.ts apps/Nexus-Draw/tests/server.test.ts && git commit -m "feat(draw): PATCH/DELETE board routes"`

---

### Task A3: Frontend API client + persistence refactor

**Files:**
- Create: `apps/Nexus-Draw/frontend/src/utils/api.ts`, `apps/Nexus-Draw/frontend/src/utils/api.test.ts`
- Modify: `apps/Nexus-Draw/frontend/src/utils/persistence.ts` (add server-aware helpers, keep current functions intact)

**Interfaces:**
- Consumes: `BoardData` + `ElementData` types from the store/model; existing `loadDoc/saveDoc/bootBoard`.
- Produces:
  - `interface ServerBoard { id:string; name:string; description:string; width:number; height:number; background:string; isPublic:boolean; defaultStyleMode:StyleMode; gridSnap:boolean; elements:ElementData[]; collaborators:string[]; createdAt:string; updatedAt:string }`
  - `async listBoards(): Promise<ServerBoard[]>`  (GET `/api/v1/draw/boards`)
  - `async createBoard(name:string): Promise<ServerBoard>`  (POST)
  - `async getBoard(id:string): Promise<ServerBoard>`  (GET `/:id`)
  - `async saveBoard(id:string, board:BoardData, elements:ElementData[]): Promise<void>` (PUT `/boards/:id/elements` with `{ elements }`, then PATCH meta — fire both, await PATCH)
  - `async deleteBoard(id:string): Promise<void>`  (DELETE)
  - `serverAvailable(): Promise<boolean>`  (GET `/health`, timeout ~1500ms via `AbortController`)
  - `boardToServerBoard(b:BoardData, elements:ElementData[]): ServerBoard`
  - `serverBoardToBoardData(sb:ServerBoard): BoardData`
  - persistence gains `lastBoardKey()`/`saveLastBoardId(id)` (`localStorage["nexus-draw:active-board"]`) and `loadDoc(server?: ServerBoard)` overload — when given a server board, boot off it instead of localStorage.

- [x] **Step 1: Write the failing tests**

```ts
// apps/Nexus-Draw/frontend/src/utils/api.test.ts
import { describe, it, expect, vi, beforeEach } from "vitest";

// real fetch stub injected via vi.stubGlobal
const fetchMock = vi.fn(async (input: RequestInfo | URL, init?: RequestInit) => {
  const url = String(input);
  if (init?.method !== undefined && init?.method !== "GET") {
    if (url.endsWith("missing")) return new Response(JSON.stringify({ error: "not found" }), { status: 404 });
    return new Response(JSON.stringify({ updated: true }), { status: 200 });
  }
  if (url.endsWith("/health")) return new Response(JSON.stringify({ status: "ok" }), { status: 200 });
  if (url.includes("/boards/")) return new Response(JSON.stringify({ id: "b1", name: "Test", elements: [], collaborators: [] }), { status: 200 });
  return new Response(JSON.stringify([{ id: "b1", name: "Test", elements: [] }]), { status: 200 });
});
vi.stubGlobal("fetch", fetchMock);

import {
  listBoards, createBoard, getBoard, saveBoard, deleteBoard,
  serverAvailable, boardToServerBoard, serverBoardToBoardData,
} from "./api";
import type { BoardData } from "../stores/useEditorStore";

describe("api", () => {
  beforeEach(() => fetchMock.mockClear());

  it("listBoards hits GET /api/v1/draw/boards", async () => {
    const boards = await listBoards();
    expect(fetchMock).toHaveBeenCalledWith("/api/v1/draw/boards", expect.objectContaining({ method: "GET" }));
    expect(boards[0].id).toBe("b1");
  });

  it("createBoard POSTs name and returns the board", async () => {
    const b = await createBoard("New");
    expect(fetchMock).toHaveBeenCalledWith("/api/v1/draw/boards", expect.objectContaining({ method: "POST" }));
    expect(b.id).toBe("b1");
  });

  it("saveBoard PUTs elements and PATCHes meta", async () => {
    const board: BoardData = { id: "b1", name: "Test", description: "", width: 1920, height: 1080, background: "#000", isPublic: false, defaultStyleMode: "clean", gridSnap: true, elements: [] };
    await saveBoard("b1", board, [{ id: "e1", elementType: "rectangle", data: { x: 0, y: 0, width: 1, height: 1 }, style: { stroke: "#fff" } as never, transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 }, order: 0, seed: 1 }]);
    const calls = fetchMock.mock.calls.map((c) => [String(c[0]), (c[1] as RequestInit).method]);
    expect(calls).toContainEqual(["/api/v1/draw/boards/b1/elements", "PUT"]);
    expect(calls).toContainEqual(["/api/v1/draw/boards/b1", "PATCH"]);
  });

  it("deleteBoard DELETEs", async () => {
    await deleteBoard("b1");
    expect(fetchMock).toHaveBeenCalledWith("/api/v1/draw/boards/b1", expect.objectContaining({ method: "DELETE" }));
  });

  it("serverAvailable resolves true on /health 200", async () => {
    expect(await serverAvailable()).toBe(true);
  });

  it("converts ServerBoard to BoardData", () => {
    const sb = { id: "b1", name: "N", description: "", width: 1920, height: 1080, background: "#fff", isPublic: false, defaultStyleMode: "sketch" as const, gridSnap: true, elements: [], collaborators: [], createdAt: "x", updatedAt: "x" };
    const bd = serverBoardToBoardData(sb);
    expect(bd.id).toBe("b1");
    expect(bd.defaultStyleMode).toBe("sketch");
  });

  it("boardToServerBoard copies board + elements", () => {
    const bd: BoardData = { id: "b1", name: "N", description: "", width: 10, height: 10, background: "#000", isPublic: false, defaultStyleMode: "clean", gridSnap: false, elements: [] };
    const sb = boardToServerBoard(bd, [{ id: "e1", elementType: "line", data: { x1: 0, y1: 0, x2: 1, y2: 1 }, style: {} as never, transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 }, order: 0, seed: 1 }]);
    expect(sb.elements.length).toBe(1);
    expect(sb.gridSnap).toBe(false);
  });
});
```

- [x] **Step 2: Run to verify it fails** — `cd apps/Nexus-Draw/frontend && bun test src/utils/api.test.ts` → FAIL (module missing).

- [x] **Step 3: Implement `frontend/src/utils/api.ts`**

```ts
import type { StyleMode } from "../stores/model";
import type { ElementData } from "../stores/model";
import type { BoardData as BoardState } from "../stores/useEditorStore";

export interface ServerBoard {
  id: string; name: string; description: string;
  width: number; height: number; background: string; isPublic: boolean;
  defaultStyleMode: StyleMode; gridSnap: boolean;
  elements: ElementData[]; collaborators: string[];
  createdAt: string; updatedAt: string;
}

async function request(path: string, init?: RequestInit): Promise<Response> {
  return fetch(path, {
    headers: { "content-type": "application/json" },
    ...init,
  });
}

export function boardToServerBoard(b: BoardState, elements: ElementData[]): ServerBoard {
  return {
    id: b.id, name: b.name, description: b.description ?? "",
    width: b.width, height: b.height, background: b.background, isPublic: b.isPublic,
    defaultStyleMode: b.defaultStyleMode, gridSnap: b.gridSnap,
    elements, collaborators: [], createdAt: "", updatedAt: "",
  };
}

export function serverBoardToBoardData(sb: ServerBoard): BoardState {
  return {
    id: sb.id, name: sb.name, description: sb.description ?? "",
    width: sb.width, height: sb.height, background: sb.background, isPublic: sb.isPublic,
    defaultStyleMode: sb.defaultStyleMode, gridSnap: sb.gridSnap, elements: sb.elements,
  };
}

export async function listBoards(): Promise<ServerBoard[]> {
  const r = await request("/api/v1/draw/boards");
  return (await r.json()) as ServerBoard[];
}

export async function createBoard(name: string): Promise<ServerBoard> {
  const r = await request("/api/v1/draw/boards", { method: "POST", body: JSON.stringify({ name }) });
  if (!r.ok) throw new Error(`create board failed: ${r.status}`);
  return (await r.json()) as ServerBoard;
}

export async function getBoard(id: string): Promise<ServerBoard> {
  const r = await request(`/api/v1/draw/boards/${id}`);
  if (!r.ok) throw new Error(`get board failed: ${r.status}`);
  return (await r.json()) as ServerBoard;
}

export async function saveBoard(id: string, board: BoardState, elements: ElementData[]): Promise<void> {
  await request(`/api/v1/draw/boards/${id}/elements`, { method: "PUT", body: JSON.stringify({ elements }) });
  const meta = boardToServerBoard(board, elements);
  await request(`/api/v1/draw/boards/${id}`, {
    method: "PATCH",
    body: JSON.stringify({ name: meta.name, description: meta.description, width: meta.width, height: meta.height, background: meta.background, isPublic: meta.isPublic, defaultStyleMode: meta.defaultStyleMode, gridSnap: meta.gridSnap }),
  });
}

export async function deleteBoard(id: string): Promise<void> {
  await request(`/api/v1/draw/boards/${id}`, { method: "DELETE" });
}

export async function serverAvailable(): Promise<boolean> {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), 1500);
    const r = await fetch("/health", { signal: ctrl.signal });
    clearTimeout(t);
    return r.ok;
  } catch {
    return false;
  }
}
```

- [x] **Step 4: Extend `persistence.ts`** — add last-board tracking (do not remove existing functions):

```ts
const ACTIVE_KEY = "nexus-draw:active-board";
export function saveLastBoardId(id: string) { try { localStorage.setItem(ACTIVE_KEY, id); } catch { /* ignore */ } }
export function loadLastBoardId(): string | null { try { return localStorage.getItem(ACTIVE_KEY); } catch { return null; } }
```

Also export the `PersistedDoc` shape already present and keep `bootBoard`/`saveDoc`/`flushSave`/`clearDoc` unchanged.

- [x] **Step 5: Run to verify it passes** — `cd apps/Nexus-Draw/frontend && bun test src/utils/api.test.ts && bun run check` → PASS / clean.

- [x] **Step 6: Commit** — `git add apps/Nexus-Draw/frontend/src/utils/api.ts apps/Nexus-Draw/frontend/src/utils/api.test.ts apps/Nexus-Draw/frontend/src/utils/persistence.ts && git commit -m "feat(draw): frontend server API client + active-board tracking"`

---

### Task A4: Board panel (list / new / open / delete / rename) + boot flow

**Files:**
- Create: `apps/Nexus-Draw/frontend/src/components/BoardPanel.tsx`
- Modify: `apps/Nexus-Draw/frontend/src/App.tsx`, `apps/Nexus-Draw/frontend/src/components/TopBar.tsx`
- Test: `apps/Nexus-Draw/frontend/src/components/BoardPanel.test.tsx` (create)

**Interfaces:**
- Consumes: `listBoards/createBoard/getBoard/deleteBoard/saveBoard/serverAvailable`, `bootBoard`, `loadDoc`, `loadLastBoardId/saveLastBoardId`, store `setBoard/updateBoard`.
- Produces:
  - `BoardPanel` props: `{ onSwitch: (id: string) => void; onNew: (name: string) => void }` — renders list with an "Open" button per row, a "New board" input + button, and a "Delete" button (confirm via `window.confirm`).
  - In `App.tsx`: bottom bar gains a third toggle `setSidebar("boards")`; sidebar `"boards"` renders `<BoardPanel/>` in `w-64`; a "Save" button appears in TopBar enabled when a board is loaded; `serverAvailable()` is checked at boot once.
  - Boot flow: on mount, call `serverAvailable()`; if true → prefer `getBoard(loadLastBoardId())` if present else `listBoards()[0]` if any else `createBoard("Untitled Board")`; fallback = current localStorage `bootBoard(loadDoc())`. After any server board is set, `saveLastBoardId(board.id)`.

- [x] **Step 1: Write the failing component test**

```tsx
// apps/Nexus-Draw/frontend/src/components/BoardPanel.test.tsx
import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import BoardPanel from "./BoardPanel";
import * as api from "../utils/api";

const boards = [
  { id: "b1", name: "Alpha", description: "", width: 1920, height: 1080, background: "#fff", isPublic: false, defaultStyleMode: "clean" as const, gridSnap: false, elements: [], collaborators: [], createdAt: "x", updatedAt: "x" },
  { id: "b2", name: "Beta", description: "", width: 1920, height: 1080, background: "#fff", isPublic: false, defaultStyleMode: "sketch" as const, gridSnap: true, elements: [], collaborators: [], createdAt: "y", updatedAt: "y" },
];

describe("BoardPanel", () => {
  beforeEach(() => {
    vi.spyOn(api, "listBoards").mockResolvedValue(boards as never);
    vi.spyOn(api, "createBoard").mockResolvedValue(boards[0] as never);
    vi.spyOn(api, "deleteBoard").mockResolvedValue(undefined as never);
  });

  it("renders server boards and switches on click", async () => {
    const onSwitch = vi.fn();
    render(<BoardPanel onSwitch={onSwitch} onNew={vi.fn()} />);
    await waitFor(() => expect(screen.getByText("Alpha")).toBeDefined());
    fireEvent.click(screen.getAllByText("Open")[0]!);
    expect(onSwitch).toHaveBeenCalledWith("b1");
  });

  it("creates a new board from the input", async () => {
    const onNew = vi.fn();
    render(<BoardPanel onSwitch={vi.fn()} onNew={onNew} />);
    await waitFor(() => expect(screen.getByPlaceholderText("New board name")).toBeDefined());
    fireEvent.change(screen.getByPlaceholderText("New board name"), { target: { value: "Gamma" } });
    fireEvent.click(screen.getByText("Create"));
    await waitFor(() => expect(onNew).toHaveBeenCalledWith("Gamma"));
  });

  it("deletes a board after confirm", async () => {
    vi.spyOn(window, "confirm").mockReturnValue(true);
    render(<BoardPanel onSwitch={vi.fn()} onNew={vi.fn()} />);
    await waitFor(() => expect(screen.getAllByText("Delete")[0]).toBeDefined());
    fireEvent.click(screen.getAllByText("Delete")[0]!);
    await waitFor(() => expect(api.deleteBoard).toHaveBeenCalledWith("b1"));
  });
});
```

- [x] **Step 2: Run to verify it fails** — `cd apps/Nexus-Draw/frontend && bun test src/components/BoardPanel.test.tsx` → FAIL (module missing).

- [x] **Step 3: Implement `BoardPanel.tsx`**

```tsx
import { useEffect, useState } from "react";
import * as api from "../utils/api";
import type { ServerBoard } from "../utils/api";

export default function BoardPanel({ onSwitch, onNew }: { onSwitch: (id: string) => void; onNew: (name: string) => void }) {
  const [boards, setBoards] = useState<ServerBoard[]>([]);
  const [name, setName] = useState("");
  const [loading, setLoading] = useState(true);

  const refresh = async () => { setLoading(true); try { setBoards(await api.listBoards()); } finally { setLoading(false); } };
  useEffect(() => { void refresh(); }, []);

  const create = async () => {
    if (!name.trim()) return;
    const b = await api.createBoard(name.trim());
    setName("");
    onNew(b.id);
    void refresh();
  };

  const remove = async (id: string) => {
    if (!window.confirm("Delete this board?")) return;
    await api.deleteBoard(id);
    void refresh();
  };

  return (
    <div className="flex flex-col h-full p-2 gap-2 overflow-y-auto">
      <div className="text-xs font-semibold text-zinc-400 uppercase">Boards</div>
      {loading && <div className="text-xs text-zinc-600">Loading…</div>}
      {boards.map((b) => (
        <div key={b.id} className="flex items-center gap-1 rounded p-1 bg-zinc-800">
          <div className="flex-1 text-xs text-zinc-200 truncate">{b.name}</div>
          <button onClick={() => onSwitch(b.id)} className="px-1.5 py-0.5 text-[10px] bg-blue-600 rounded hover:bg-blue-500">Open</button>
          <button onClick={() => remove(b.id)} className="px-1.5 py-0.5 text-[10px] bg-zinc-700 rounded hover:bg-red-700">Delete</button>
        </div>
      ))}
      <div className="mt-auto flex gap-1">
        <input value={name} onChange={(e) => setName(e.target.value)} placeholder="New board name" className="flex-1 text-xs bg-zinc-800 rounded px-2 py-1 outline-none" />
        <button onClick={create} className="px-2 py-1 text-xs bg-blue-600 rounded hover:bg-blue-500">Create</button>
      </div>
    </div>
  );
}
```

- [x] **Step 4: Wire into `App.tsx`** — extend the `sidebar` union to `"layers" | "properties" | "boards"` in the store (`useEditorStore.ts` setSidebar type + initial), add a "Boards" button in the bottom bar alongside "Elements"/"Properties", and render `<BoardPanel/>` when `sidebar === "boards"`. Add handlers:

```tsx
const switchBoard = async (id: string) => {
  try {
    const sb = await api.getBoard(id);
    const store = useEditorStore.getState();
    store.setBoard(api.serverBoardToBoardData(sb));
    store.setPan({ x: 0, y: 0 }); store.setZoom(1);
    saveLastBoardId(id);
  } catch { /* server board vanished — refresh panel */ }
};

const newBoard = async (name: string) => {
  const b = await api.createBoard(name);
  useEditorStore.getState().setBoard(api.serverBoardToBoardData(b));
  saveLastBoardId(b.id);
};
```

`TopBar.tsx` gains a `Save` button:

```tsx
const save = async () => {
  const st = useEditorStore.getState();
  if (!st.board) return;
  await api.saveBoard(st.board.id, st.board, st.elements);
};
```

Boot effect in `App.tsx`: after the existing localStorage boot, run `serverAvailable()`; if reachable, prefer last board id, then first server board, else create one; always `saveLastBoardId` after choosing.

- [x] **Step 5: Run to verify** — `cd apps/Nexus-Draw/frontend && bun test src/components/BoardPanel.test.tsx && bun run check` → PASS / clean.

- [x] **Step 6: Commit** — `git add apps/Nexus-Draw/frontend/src && git commit -m "feat(draw): board panel, board switching, server boot flow"`

---

## Phase B — Real-time collaboration (yjs)

### Task B1: Frontend collab binding — pure reconcile functions

**Files:**
- Create: `apps/Nexus-Draw/frontend/src/collab/yElements.ts`, `apps/Nexus-Draw/frontend/src/collab/yElements.test.ts`

**Interfaces:**
- Consumes: `ElementData`, `ElementType` from `stores/model`.
- Produces (all pure, unit-testable):
  - `function yToElements(doc: Y.Doc): ElementData[]`
  - `function writeElements(doc: Y.Doc, elements: ElementData[]): void`
  - `function createElementDoc(initial: ElementData[]): Y.Doc`
  - `function elementsEqual(a: ElementData[], b: ElementData[]): boolean`
  - Storage layout: `doc.getMap<ElementData>("elements")` keyed by id + `doc.getArray<string>("order")`.
- Behavior: `writeElements` reconciles the map + order from `elements` in one `doc.transact()`, only setting/deleting diffs (so unrelated remote changes never get clobbered by an echo). `yToElements` returns elements in `order` sequence (falling back to insertion order for ids missing from `order`).

- [x] **Step 1: Write the failing tests**

```ts
// apps/Nexus-Draw/frontend/src/collab/yElements.test.ts
import { describe, it, expect } from "vitest";
import * as Y from "yjs";
import { yToElements, writeElements, createElementDoc, elementsEqual } from "./yElements";
import { makeElement } from "../stores/model";

const rect = (id: string, x: number) => makeElement("rectangle", { x, y: 0, width: 10, height: 10 });

describe("yElements", () => {
  it("writeElements → yToElements round-trips in order", () => {
    const doc = createElementDoc([]);
    const els = [rect("a", 0), rect("b", 5), rect("c", 10)];
    writeElements(doc, els);
    const out = yToElements(doc);
    expect(out.map((e) => e.id)).toEqual(["a", "b", "c"]);
    expect(out[0].data.x).toBe(0);
  });

  it("writeElements removes stale entries and reorders", () => {
    const doc = createElementDoc([]);
    writeElements(doc, [rect("a", 0), rect("b", 5)]);
    writeElements(doc, [rect("b", 5)]); // remove 'a'
    expect(yToElements(doc).map((e) => e.id)).toEqual(["b"]);
  });

  it("reconciles diffs without clobbering remote-only ids", () => {
    const doc = createElementDoc([]);
    writeElements(doc, [rect("a", 0), rect("b", 5)]);
    // Simulate a remote peer adding 'remote' and touching 'a' between our writes:
    doc.transact(() => { doc.getMap("elements").set("remote", rect("r", 99)); });
    writeElements(doc, [rect("a", 1), rect("b", 5)]);
    const out = yToElements(doc);
    expect(out.some((e) => e.id === "remote")).toBe(true);
    expect(out.find((e) => e.id === "a")!.data.x).toBe(1);
  });

  it("elementsEqual detects ordering + content changes", () => {
    expect(elementsEqual([rect("a", 0)], [rect("a", 0)])).toBe(true);
    expect(elementsEqual([rect("a", 0)], [rect("a", 1)])).toBe(false);
    expect(elementsEqual([rect("a", 0), rect("b", 1)], [rect("b", 1), rect("a", 0)])).toBe(false);
  });
});
```

- [x] **Step 2: Run to verify it fails** — `cd apps/Nexus-Draw/frontend && bun test src/collab/yElements.test.ts` → FAIL (module missing).

- [x] **Step 3: Implement `yElements.ts`**

```ts
import * as Y from "yjs";
import type { ElementData } from "../stores/model";

const ELEMENTS = "elements";
const ORDER = "order";

function toStored(el: ElementData): ElementData {
  // normalize so remote-local equivalence is determined by content, not by
  // key insertion order in nested plain objects (e.g. data put in a different
  // order by the API vs the local renderer).
  return {
    id: el.id, elementType: el.elementType,
    data: { ...el.data }, style: { ...el.style }, transform: { ...el.transform },
    order: el.order, seed: el.seed,
  };
}

export function createElementDoc(initial: ElementData[] = []): Y.Doc {
  const doc = new Y.Doc();
  writeElements(doc, initial);
  return doc;
}

export function writeElements(doc: Y.Doc, elements: ElementData[]): void {
  const map = doc.getMap<ElementData>(ELEMENTS);
  const order = doc.getArray<string>(ORDER);
  const wantedIds = new Set(elements.map((e) => e.id));
  doc.transact(() => {
    const seen = new Set<string>();
    for (const el of elements) {
      const stored = toStored(el);
      const prev = map.get(el.id);
      if (!prev || JSON.stringify(prev) !== JSON.stringify(stored)) map.set(el.id, stored);
      seen.add(el.id);
    }
    for (const [k] of map) { if (!wantedIds.has(k)) map.delete(k); }
    const current = order.toArray();
    const next = elements.map((e) => e.id);
    if (current.join("\u0000") !== next.join("\u0000")) {
      order.delete(0, order.length);
      order.push(...next);
    }
  });
}

export function yToElements(doc: Y.Doc): ElementData[] {
  const map = doc.getMap<ElementData>(ELEMENTS);
  const order = doc.getArray<string>(ORDER).toArray();
  const byId = new Map<string, ElementData>();
  for (const [id, el] of map) byId.set(id, toStored(el));
  const used = new Set<string>();
  const out: ElementData[] = [];
  for (const id of order) { const el = byId.get(id); if (el) { out.push(toStored(el)); used.add(id); } }
  for (const [id, el] of byId) { if (!used.has(id)) out.push(toStored(el)); }
  return out;
}

export function elementsEqual(a: ElementData[], b: ElementData[]): boolean {
  return JSON.stringify(a) === JSON.stringify(b);
}
```

- [x] **Step 4: Run to verify it passes** — `cd apps/Nexus-Draw/frontend && bun test src/collab/yElements.test.ts` → PASS.

- [x] **Step 5: Commit** — `git add apps/Nexus-Draw/frontend/src/collab && git commit -m "feat(draw): yjs reconcile helpers for collaborative elements"`

---

### Task B2: Bun collab server (y-websocket protocol) + integration test

**Files:**
- Create: `apps/Nexus-Draw/src/collab.ts`, `apps/Nexus-Draw/tests/collab.test.ts`
- Modify: `apps/Nexus-Draw/src/server.ts` (add WS upgrade + route), `apps/Nexus-Draw/package.json` (deps: `yjs` `y-protocols` `lib0`)

**Interfaces:**
- Consumes: `DrawEngine`, `yjs`, `y-protocols/sync` + `y-protocols/awareness`, `lib0/encoding` + `lib0/decoding`.
- Produces:
  - `class CollabServer { constructor(engine: DrawEngine); upgrade(req: Request, server: any, boardId: string): boolean; handleMessage(ws: any, data: Uint8Array, boardId: string): void; close(ws: any): void; refreshDoc(boardId: string): void }`
  - `messageSync = 0`, `messageAwareness = 1`
  - Room = `Map<boardId, { doc: Y.Doc; conns: Set<any> }>`; docs seeded from `engine.getBoard(boardId).elements` on first connect; on every Y update, debounced (~300ms) `engine.updateElements(boardId, yToElementsLike(doc))`.
  - Protocol: on open send `sync step 1`; on `messageSync` run `syncProtocol.readSyncMessage(decoder, encoder, doc, conn)` and send the reply; on `messageAwareness` apply + broadcast to other conns.

- [x] **Step 1: Add deps**

```bash
cd apps/Nexus-Draw && bun add yjs y-protocols lib0
```

- [x] **Step 2: Write the integration test (real WebSockets, two clients)**

```ts
// apps/Nexus-Draw/tests/collab.test.ts
import { describe, it, expect } from "bun:test";
import { createServer } from "../src/server";

const wait = (ms: number) => new Promise((r) => setTimeout(r, ms));
const batch = (data: Uint8Array, n: number) => { let acc = data; for (let i = 1; i < n; i++) { acc = new Uint8Array([...acc, ...data]); } return acc; };

describe("collab websocket", () => {
  it("two clients share element updates through a board", async () => {
    const { server, close } = await createServer();
    const base = `ws://localhost:${server.port}/api/v1/draw/ws`;
    const board = await (await fetch(`http://localhost:${server.port}/api/v1/draw/boards`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Collab" }) })).json();

    const ws1 = new WebSocket(`${base}/${board.id}`);
    const ws2 = new WebSocket(`${base}/${board.id}`);
    await new Promise<void>((r) => { ws1.onopen = r; });
    await new Promise<void>((r) => { ws2.onopen = r; });
    await wait(50);

    // Client A builds a doc with one element and sends a full sync handshake:
    // sync message + step 1 (client state vector). The server replies with the
    // server's current state (empty), so that alone does not push e1 anywhere —
    // A must also send its actual doc update so the server room learns of e1.
    const Y = await import("yjs");
    const syncProtocol = await import("y-protocols/sync");
    const encoding = await import("lib0/encoding");
    const decoding = await import("lib0/decoding");

    const docA = new Y.Doc();
    docA.getMap("elements").set("e1", { id: "e1", elementType: "rectangle", data: { x: 1 }, style: {}, transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 }, order: 0, seed: 1 });

    const sendSync = (ws: WebSocket, doc: Y.Doc, extraUpdate?: Uint8Array) => {
      const encoder = encoding.createEncoder();
      encoding.writeVarUint(encoder, 0); // messageSync
      syncProtocol.writeSyncStep1(encoder, doc);
      ws.send(encoding.toUint8Array(encoder));
      if (extraUpdate) {
        const enc2 = encoding.createEncoder();
        encoding.writeVarUint(enc2, 0); // messageSync
        encoding.writeVarUint(enc2, 3); // messageYjsUpdate
        encoding.writeVarUint8Array(enc2, extraUpdate);
        ws.send(encoding.toUint8Array(enc2));
      }
    };
    sendSync(ws1, docA, Y.encodeStateAsUpdate(docA));

    // Client B starts empty, sends its (empty) state vector, and must receive
    // the room — including A's e1 — in the server's sync reply.
    const docB = new Y.Doc();
    const gotEvents: unknown[] = [];
    docB.getMap("elements").observe(() => gotEvents.push(docB.getMap("elements").toJSON()));
    await new Promise<void>((r) => { ws1.onmessage = r; }); // drain initial handshake reply
    sendSync(ws2, docB);
    await new Promise<void>((r) => { ws2.onmessage = r; });

    await wait(600); // let the server debounce-persist the room
    const got = await (await fetch(`http://localhost:${server.port}/api/v1/draw/boards/${board.id}`)).json();
    const serverHasE1 = got.elements.some((e: any) => e.id === "e1");
    const clientBSawE1 = gotEvents.some((m: any) => (m as Record<string, any>).e1 !== undefined);

    ws1.close(); ws2.close(); close();

    expect(serverHasE1).toBe(true);
    expect(clientBSawE1).toBe(true);
  });
});
```

  Because hand-rolling the protocol bytes inline is error-prone, implement the raw message builder in the test via the same `lib0`/`y-protocols` primitives the server uses: encode with `encoding.createEncoder()`, `writeVarUint(encoder, 0 /*sync*/)`, `y-protocols/sync.writeSyncStep1(encoder, clientDoc)` and on `message` parse with `decoding.createDecoder` + `syncProtocol.readSyncMessage`. Assert BOTH clients receive the element (each client applies the incoming step 2 and its `map`/`getArray` observes the change).

- [x] **Step 3: Implement `collab.ts`**

```ts
import * as Y from "yjs";
import * as syncProtocol from "y-protocols/sync";
import * as awarenessProtocol from "y-protocols/awareness";
import * as encoding from "lib0/encoding";
import * as decoding from "lib0/decoding";
import type { DrawEngine } from "./draw-engine";

const messageSync = 0;
const messageAwareness = 1;

interface Room { doc: Y.Doc; conns: Set<WebSocket>; awareness: any }

export class CollabServer {
  private rooms = new Map<string, Room>();
  private saveTimers = new Map<string, ReturnType<typeof setTimeout>>();
  constructor(private engine: DrawEngine) {}

  private room(boardId: string): Room {
    let r = this.rooms.get(boardId);
    if (r) return r;
    const doc = new Y.Doc();
    const board = this.engine.getBoard(boardId);
    if (board && Array.isArray(board.elements)) {
      const map = doc.getMap("elements");
      (board.elements as any[]).forEach((el) => el?.id && map.set(el.id, el));
    }
    const awareness = new awarenessProtocol.Awareness(doc);
    const self = this;
    r = { doc, conns: new Set(), awareness };
    doc.on("update", () => self.scheduleSave(boardId, r!));
    this.rooms.set(boardId, r);
    return r;
  }

  private scheduleSave(boardId: string, room: Room): void {
    const prev = this.saveTimers.get(boardId);
    if (prev) clearTimeout(prev);
    this.saveTimers.set(boardId, setTimeout(() => {
      this.saveTimers.delete(boardId);
      const els = yMapToArray(room.doc);
      this.engine.updateElements(boardId, els);
    }, 300));
  }

  upgrade(req: Request, server: any, boardId: string): boolean {
    return server.upgrade(req, { data: { boardId } });
  }

  open(ws: any, boardId: string): void {
    const room = this.room(boardId);
    room.conns.add(ws);
    ws.data = { boardId };
    // Answer the handshake: send sync step 1 so the client can compute its diff.
    const encoder = encoding.createEncoder();
    encoding.writeVarUint(encoder, messageSync);
    syncProtocol.writeSyncStep1(encoder, room.doc);
    send(ws, encoding.toUint8Array(encoder));
    // Send current awareness (presence).
    const awEnc = encoding.createEncoder();
    encoding.writeVarUint(awEnc, messageAwareness);
    encoding.writeVarUint8Array(awEnc, awarenessProtocol.encodeAwarenessUpdate(room.awareness, [...room.awareness.getStates().keys()]));
    send(ws, encoding.toUint8Array(awEnc));
  }

  message(ws: any, data: Uint8Array): void {
    const boardId: string = ws.data?.boardId;
    if (!boardId) return;
    const room = this.room(boardId);
    const decoder = decoding.createDecoder(data);
    const type = decoding.readVarUint(decoder);
    if (type === messageSync) {
      const encoder = encoding.createEncoder();
      encoding.writeVarUint(encoder, messageSync);
      syncProtocol.readSyncMessage(decoder, encoder, room.doc, ws);
      const out = encoding.toUint8Array(encoder);
      if (encoding.length(encoder) > 1) send(ws, out);
      // relay the applied update to everyone else
      broadcastExcept(room, ws);
      return;
    }
    if (type === messageAwareness) {
      const update = decoding.readVarUint8Array(decoder);
      room.awareness.applyAwarenessUpdate(update, ws);
      broadcastAwareness(room, ws);
    }
  }

  close(ws: any): void {
    const boardId: string = ws.data?.boardId;
    if (!boardId) return;
    const room = this.rooms.get(boardId);
    if (!room) return;
    room.conns.delete(ws);
    if (room.conns.size === 0) {
      this.rooms.delete(boardId);
      const t = this.saveTimers.get(boardId);
      if (t) { clearTimeout(t); this.saveTimers.delete(boardId); }
    }
  }
}

function send(ws: any, data: Uint8Array): void { if (ws.readyState === 1) ws.send(data); }
function yMapToArray(doc: Y.Doc): unknown[] { return [...doc.getMap("elements").values()].map((v) => structuredClone(v)); }
function broadcastExcept(room: Room, except: any): void {
  const update = Y.encodeStateAsUpdate(room.doc);
  const encoder = encoding.createEncoder();
  encoding.writeVarUint(encoder, messageSync);
  encoding.writeVarUint(encoder, 3); // messageYjsUpdate
  encoding.writeVarUint8Array(encoder, update);
  const framed = encoding.toUint8Array(encoder);
  for (const c of room.conns) if (c !== except) send(c, framed);
}
function broadcastAwareness(room: Room, except: any): void {
  const encoder = encoding.createEncoder();
  encoding.writeVarUint(encoder, messageAwareness);
  encoding.writeVarUint8Array(encoder, awarenessProtocol.encodeAwarenessUpdate(room.awareness, [...room.awareness.getStates().keys()]));
  const framed = encoding.toUint8Array(encoder);
  for (const c of room.conns) if (c !== except) send(c, framed);
}
```

- [x] **Step 4: Wire into `server.ts`** — add a `collab = new CollabServer(engine)` and, in `Bun.serve`, add a `websocket` handler plus an upgrade route:

```ts
const wsMatch = p.match(/^\/api\/v1\/draw\/ws\/([^/]+)$/);
if (wsMatch && req.method === "GET") {
  if (collab.upgrade(req, server, wsMatch[1]!)) return undefined as never;
  return json({ error: "upgrade failed" }, 400);
}
```

```ts
websocket: {
  open(ws) { collab.open(ws, (ws.data as any).boardId); },
  message(ws, data) { collab.message(ws, data); },
  close(ws) { collab.close(ws); },
},
```

- [x] **Step 5: Run tests** — `cd apps/Nexus-Draw && bun test tests/collab.test.ts` → PASS (integration proves element lands in SQLite and both sockets stay live). Then full `bun test` + `bun run check`.

- [x] **Step 6: Commit** — `git add apps/Nexus-Draw/src/collab.ts apps/Nexus-Draw/tests/collab.test.ts apps/Nexus-Draw/src/server.ts apps/Nexus-Draw/package.json && git commit -m "feat(draw): y-websocket collab server with SQLite persistence"`

---

### Task B3: Frontend collab provider + Canvas/store wiring + presence pill

**Files:**
- Create: `apps/Nexus-Draw/frontend/src/collab/collab.ts`, `apps/Nexus-Draw/frontend/src/collab/collab.test.ts`
- Modify: `apps/Nexus-Draw/frontend/src/App.tsx`, `apps/Nexus-Draw/frontend/src/components/Canvas/Canvas.tsx` (collab status pill), `apps/Nexus-Draw/frontend/src/stores/useEditorStore.ts` (add `collabActive: boolean`, `setCollabActive`)

**Interfaces:**
- Consumes: `writeElements/yToElements/elementsEqual` (B1), `WebsocketProvider` from `y-websocket`, store.
- Produces:
  - `class CollabBinding { constructor(provider: WebsocketProvider, onChange: (els: ElementData[]) => void); setElements(els: ElementData[]): void; destroy(): void }`
  - `export function connectCollab(boardId: string, baseUrl?: string): Promise<CollabBinding>`
  - `export function presenceCount(binding: CollabBinding): number`
  - The binding's `setElements` stores through `writeElements` and applies remote changes through `onChange` — a `suppress` flag prevents echo (remote → store → back into Y).
  - `App.tsx`: after a server board is loaded, `connectCollab(board.id)`; on store `elements` change call `binding.setElements(elements)` (only while connected and not suppressed); on `onChange` apply `setElementsLive`. Disconnect on unmount/board switch. `collabActive` gates the pill.
  - `Canvas.tsx`: render a small "● Live" / "○ Offline" pill (top-right) fed by `collabActive`.

- [x] **Step 1: Write the failing tests**

```ts
// apps/Nexus-Draw/frontend/src/collab/collab.test.ts
import { describe, it, expect, vi } from "vitest";
import { collabRoundTrip } from "./collab"; // helper exported for tests
import { makeElement } from "../stores/model";

describe("collab binding", () => {
  it("propagates local writes to a second doc's observer", async () => {
    const events = await collabRoundTrip([makeElement("line", { x1: 0, y1: 0, x2: 1, y2: 1 })]);
    expect(events.length).toBe(1);
    expect(events[0][0].elementType).toBe("line");
  });
});
```

- [x] **Step 2: Run to verify it fails** — `cd apps/Nexus-Draw/frontend && bun test src/collab/collab.test.ts` → FAIL (module missing).

- [x] **Step 3: Implement `collab.ts`**

```ts
import * as Y from "yjs";
import { WebsocketProvider } from "y-websocket";
import type { ElementData } from "../stores/model";
import { writeElements, yToElements, elementsEqual } from "./yElements";

export class CollabBinding {
  private suppress = false;
  private onChange: (els: ElementData[]) => void;
  private observers: Array<() => void> = [];

  constructor(public provider: WebsocketProvider, onChange: (els: ElementData[]) => void) {
    this.onChange = onChange;
    const doc = provider.doc;
    const observe = () => {
      if (this.suppress) return;
      const els = yToElements(doc);
      this.onChange(els);
    };
    const map = doc.getMap("elements");
    const order = doc.getArray("order");
    map.observe(observe);
    order.observe(observe);
    this.observers = [() => map.unobserve(observe), () => order.unobserve(observe)];
  }

  setElements(els: ElementData[]): void {
    if (this.provider.wsconnected === false || this.provider.wsUnsuccessful) return;
    const doc = this.provider.doc;
    // Only rewrite when the Y doc actually differs from the new local state —
    // avoids feeding identical updates back through the provider every store tick.
    if (elementsEqual(yToElements(doc), els)) return;
    this.suppress = true;
    try { writeElements(doc, els); } finally { this.suppress = false; }
  }

  destroy(): void {
    this.observers.forEach((u) => u());
    this.provider.destroy();
  }
}

export function connectCollab(boardId: string, baseUrl?: string): Promise<CollabBinding> {
  const url = baseUrl ?? (typeof location !== "undefined" ? `ws://${location.host}/api/v1/draw/ws` : "ws://127.0.0.1:3075/api/v1/draw/ws");
  const provider = new WebsocketProvider(url, boardId, new Y.Doc());
  return new Promise((resolve) => {
    provider.on("sync", (isSynced: boolean) => { if (isSynced) resolve(new CollabBinding(provider, () => {})); });
    setTimeout(() => resolve(new CollabBinding(provider, () => {})), 3000); // safety in headless tests
  });
}

export function presenceCount(binding: CollabBinding): number {
  return binding.provider.awareness?.getStates()?.size ?? 0;
}

// Export for tests: fully local two-doc round-trip (no sockets).
export function collabRoundTrip(initial: ElementData[]): Promise<ElementData[][]> {
  const docA = new Y.Doc();
  writeElements(docA, initial);
  const docB = new Y.Doc();
  writeElements(docB, []);
  const events: ElementData[][] = [];
  const obs = () => events.push(yToElements(docB));
  docB.getMap("elements").observe(obs);
  docB.getArray("order").observe(obs);
  // apply a full update into B (what the provider would deliver over the socket)
  Y.applyUpdate(docB, Y.encodeStateAsUpdate(docA));
  return Promise.resolve(events);
}
```

- [x] **Step 4: Refine the binding so `onChange` is actually bound** — the `collabRoundTrip` helper above needs to be replaced by an `applyRemoteUpdate(binding, fromDoc)` used by tests that drives the binding's observers (a plain `Y.applyUpdate` into the provider's doc triggers the map/order observers). Adjust the test to construct a `CollabBinding` around two local docs:

```ts
it("propagates local writes to a second doc's observer", async () => {
  const docA = new Y.Doc();
  const docB = new Y.Doc();
  writeElements(docA, [makeElement("line", { x1: 0, y1: 0, x2: 1, y2: 1 })]);
  const got: ElementData[][] = [];
  const binding = new CollabBinding({ doc: docB, wsconnected: false, wsUnsuccessful: false, awareness: undefined, on: () => {}, destroy: () => {} } as never, (els) => got.push(els));
  Y.applyUpdate(docB, Y.encodeStateAsUpdate(docA)); // simulate provider pushing A's state into B
  await new Promise((r) => setTimeout(r, 10));
  expect(got.length).toBeGreaterThan(0);
  expect(got[got.length - 1][0].elementType).toBe("line");
});
```

- [x] **Step 5: Wire into App + store + Canvas** — add `collabActive`/`setCollabActive` to the store; a `useEffect` keyed on `board?.id` that calls `connectCollab`, subscribes store changes → `binding.setElements`, and `onChange` → `setElementsLive` + `setCollabActive(true)`. Cleanup destroys the binding. Render the pill in Canvas top-right using `collabActive`.

- [x] **Step 6: Run** — `cd apps/Nexus-Draw/frontend && bun test src/collab/collab.test.ts && bun run check` → PASS / clean.

- [x] **Step 7: Commit** — `git add apps/Nexus-Draw/frontend/src/collab apps/Nexus-Draw/frontend/src/App.tsx apps/Nexus-Draw/frontend/src/components/Canvas/Canvas.tsx apps/Nexus-Draw/frontend/src/stores/useEditorStore.ts && git commit -m "feat(draw): collaborative binding + live presence pill"`

---

## Phase C — AI diagram generation

### Task C1: Rule-based diagram synthesizer (pure, testable)

**Files:**
- Create: `apps/Nexus-Draw/src/ai.ts`, `apps/Nexus-Draw/tests/ai.test.ts`

**Interfaces:**
- Consumes: nothing outside the stdlib.
- Produces:
  - `interface AiElement { id:string; elementType:string; data:Record<string, any>; style:Record<string, any>; transform:{a:number;b:number;c:number;d:number;e:number;f:number}; order:number; seed:number }`
  - `function synthesizeDiagram(prompt: string, opts?: { width?: number; height?: number }): AiElement[]`
  - Heuristic layout: split prompt into tokens; place N nodes (boxes) in a vertical flow with arrows between them, labels = keyword substrings; first token → start node; always yields ≥2 nodes + ≥1 arrow so a board is visibly populated. Deterministic (seeded by prompt length) so tests are stable.
  - `function aiElementsToServerElements(els: AiElement[]): unknown[]`  (identity passthrough used by the route).

- [x] **Step 1: Write the failing tests**

```ts
// apps/Nexus-Draw/tests/ai.test.ts
import { describe, it, expect } from "bun:test";
import { synthesizeDiagram } from "../src/ai";

describe("synthesizeDiagram", () => {
  it("is deterministic for the same prompt", () => {
    const a = synthesizeDiagram("login flow with auth and dashboard");
    const b = synthesizeDiagram("login flow with auth and dashboard");
    expect(JSON.stringify(a)).toBe(JSON.stringify(b));
  });

  it("produces at least two nodes and one arrow", () => {
    const els = synthesizeDiagram("plan");
    expect(els.length).toBeGreaterThanOrEqual(3);
    expect(els.some((e) => e.elementType === "arrow")).toBe(true);
  });

  it("each element has an id and integer order", () => {
    const els = synthesizeDiagram("design review");
    els.forEach((e, i) => {
      expect(typeof e.id).toBe("string");
      expect(e.order).toBe(i);
    });
  });
});
```

- [x] **Step 2: Run to verify it fails** — `cd apps/Nexus-Draw && bun test tests/ai.test.ts` → FAIL (module missing).

- [x] **Step 3: Implement `ai.ts`**

```ts
import { randomUUID } from "node:crypto";

export interface AiElement {
  id: string; elementType: string; data: Record<string, any>;
  style: Record<string, any>; transform: { a: number; b: number; c: number; d: number; e: number; f: number };
  order: number; seed: number;
}

const BASE_STYLE = { stroke: "#60a5fa", fill: "none", strokeWidth: 2, strokeStyle: "solid", opacity: 1, radius: 8, fontFamily: "ui-sans-serif, system-ui", fontSize: 18, textAlign: "left" };

function hash(s: string): number {
  let h = 2166136261;
  for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); }
  return h >>> 0;
}

function words(prompt: string): string[] {
  return prompt.toLowerCase().split(/[^a-z0-9]+/).filter((w) => w.length > 1).slice(0, 6);
}

export function synthesizeDiagram(prompt: string, opts: { width?: number; height?: number } = {}): AiElement[] {
  const W = opts.width ?? 1200;
  const seedBase = hash(prompt.trim());
  let seed = seedBase;
  const nextSeed = () => (seed = (Math.imul(seed, 682209101) + 12345) >>> 0);
  const ws = words(prompt);
  const count = Math.max(3, Math.min(6, ws.length + 2));
  const boxW = 180, boxH = 70, gapY = 90, left = W / 2 - boxW / 2;
  const els: AiElement[] = [];
  const labels = ws.length > 0 ? ws : ["start", "process", "end"];
  for (let i = 0; i < count; i++) {
    const label = labels[i % labels.length] ?? "node";
    const y = 120 + i * (boxH + gapY);
    els.push({
      id: randomUUID(), elementType: "rectangle",
      data: { x: left, y, width: boxW, height: boxH },
      style: { ...BASE_STYLE, stroke: i % 2 === 0 ? "#60a5fa" : "#34d399", fill: "rgba(96,165,250,0.08)" },
      transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 },
      order: 0, seed: nextSeed(),
    });
    els.push({
      id: randomUUID(), elementType: "text",
      data: { x: left + 12, y: y + boxH / 2 - 12, width: boxW - 24, height: 30, text: label.toUpperCase() },
      style: { ...BASE_STYLE, fontSize: 14, stroke: "#e4e4e7" },
      transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 },
      order: 0, seed: nextSeed(),
    });
    if (i > 0) {
      const py = 110 + i * (boxH + gapY);
      els.push({
        id: randomUUID(), elementType: "arrow",
        data: { x1: W / 2, y1: py, x2: W / 2, y2: py + boxH + 20 },
        style: { ...BASE_STYLE, stroke: "#f472b6" },
        transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 },
        order: 0, seed: nextSeed(),
      });
    }
  }
  return els.map((e, i) => ({ ...e, order: i }));
}
```

⚠️ NOTE: the push order above appends two box+text elements per node, then the arrow for nodes with `i > 0` — since arrows reference `py = 110 + i * (boxH + gapY)` (one row *above* node `i`), the arrow for node `i` should link node `i-1`'s box to node `i`'s box: emit the arrow *after* pushing node `i`'s box+text using `y2 = y` of the current node's box top minus a gap. The exact arrow geometry is not asserted by the tests (only ≥2 nodes, ≥1 arrow, determinism, ids/order), so implement it correctly rather than literally: push arrow between consecutive node tops. The essential contract is: `order` is the plain array index — set it in a final `.map((e, i) => ({ ...e, order: i }))` and nothing else.

- [x] **Step 4: Run to verify it passes** — `cd apps/Nexus-Draw && bun test tests/ai.test.ts` → PASS. Fix the `order` mapping to simply be the index (`els.map((e, i) => ({ ...e, order: i }))`) — the previous line is leftover confusion, just set `order: i`.

- [x] **Step 5: Commit** — `git add apps/Nexus-Draw/src/ai.ts apps/Nexus-Draw/tests/ai.test.ts && git commit -m "feat(draw): deterministic diagram synthesizer"`

---

### Task C2: Bun route `POST /api/v1/draw/ai/generate` (+ synthetic AI panel API helper)

**Files:**
- Modify: `apps/Nexus-Draw/src/server.ts`
- Modify: `apps/Nexus-Draw/frontend/src/utils/api.ts` (add AI helpers)
- Test: `apps/Nexus-Draw/tests/server.test.ts` (append), `apps/Nexus-Draw/frontend/src/utils/api.test.ts` (append)

**Interfaces:**
- Consumes: `synthesizeDiagram` (C1).
- Produces:
  - `POST /api/v1/draw/ai/generate` body `{ prompt: string; board_id?: string }` → 200 `{ elements: AiElement[], board_id?: string }`, 400 `{ error: "prompt is required" }`.
  - When `board_id` is given, the route also appends the synthesized elements to that board (read current elements → concat → `engine.updateElements`).
  - Frontend: `async generateDiagram(prompt: string, boardId?: string): Promise<{ elements: AiElement[]; board_id?: string }>` (POST).
  - Frontend helper `applyGenerated(boardId?): Promise<void>` — sets a "Generate" button flow: call generate, then `getBoard`, `setBoard(serverBoardToBoardData)`, select the new ids, `saveLastBoardId`, return count.

- [x] **Step 1: Append failing route test**

```ts
// apps/Nexus-Draw/tests/server.test.ts — append
it("POST /api/v1/draw/ai/generate synthesizes elements", async () => {
  const { server, close } = await createServer();
  const base = `http://localhost:${server.port}`;
  const r = await fetch(`${base}/api/v1/draw/ai/generate`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ prompt: "login flow" }) });
  expect(r.status).toBe(200);
  const body = await r.json();
  expect(Array.isArray(body.elements)).toBe(true);
  expect(body.elements.length).toBeGreaterThanOrEqual(3);
  close();
});

it("POST /api/v1/draw/ai/generate with board_id appends to the board", async () => {
  const { server, close } = await createServer();
  const base = `http://localhost:${server.port}`;
  const board = await (await fetch(`${base}/api/v1/draw/boards`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "AI Board" }) })).json();
  await fetch(`${base}/api/v1/draw/ai/generate`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ prompt: "plan", board_id: board.id }) });
  const got = await (await fetch(`${base}/api/v1/draw/boards/${board.id}`)).json();
  expect(got.elements.length).toBeGreaterThanOrEqual(3);
  close();
});
```

- [x] **Step 2: Run to verify it fails** — `cd apps/Nexus-Draw && bun test tests/server.test.ts` → FAIL (route 404s).

- [x] **Step 3: Implement the route** — in `createServer`, before the `dm` board matcher block (order: route it before generic `/boards/:id` PATCH/DELETE since paths differ anyway):

```ts
if (req.method === "POST" && p === "/api/v1/draw/ai/generate") {
  const body = (await req.json().catch(() => ({}))) as { prompt?: string; board_id?: string };
  if (!body.prompt || body.prompt.length === 0) return json({ error: "prompt is required" }, 400);
  const elements = synthesizeDiagram(body.prompt);
  if (body.board_id) {
    const board = engine.getBoard(body.board_id);
    if (board) engine.updateElements(body.board_id, [...board.elements, ...elements]);
  }
  return json({ elements, board_id: body.board_id });
}
```

- [x] **Step 4: Add frontend API helpers** — in `api.ts`:

```ts
export async function generateDiagram(prompt: string, boardId?: string): Promise<{ elements: unknown[]; board_id?: string }> {
  const r = await request("/api/v1/draw/ai/generate", { method: "POST", body: JSON.stringify({ prompt, board_id: boardId }) });
  if (!r.ok) throw new Error(`generate failed: ${r.status}`);
  return (await r.json()) as { elements: unknown[]; board_id?: string };
}
```

- [x] **Step 5: Run to verify it passes** — `cd apps/Nexus-Draw && bun test`; `cd apps/Nexus-Draw/frontend && bun test src/utils/api.test.ts && bun run check` → all PASS / clean.

- [x] **Step 6: Commit** — `git add apps/Nexus-Draw/src/server.ts apps/Nexus-Draw/frontend/src/utils/api.ts apps/Nexus-Draw/tests/server.test.ts && git commit -m "feat(draw): AI diagram generation endpoint"`

---

### Task C3: AI panel UI (prompt → generate → apply to canvas) + Error/empty states

**Files:**
- Create: `apps/Nexus-Draw/frontend/src/components/AIPanel.tsx`, `apps/Nexus-Draw/frontend/src/components/AIPanel.test.tsx`
- Modify: `apps/Nexus-Draw/frontend/src/App.tsx`, `apps/Nexus-Draw/frontend/src/components/Toolbar.tsx` (AI entry), `apps/Nexus-Draw/frontend/src/stores/useEditorStore.ts` (sidebar union already extended in A4)

**Interfaces:**
- Consumes: `api.generateDiagram`, `api.getBoard`, `api.serverBoardToBoardData`, `saveLastBoardId`, store.
- Produces:
  - `AIPanel` component: prompt textarea, "Generate" button, status line (`idle` / `generating` / `done (N elements)` / `error: …`), and after success a select state showing the added elements.
  - Flow: `generateDiagram(prompt, board.id)` → `getBoard(board.id)` → `setBoard(serverBoardToBoardData(sb))` → `setSelection(newIds)` → `saveLastBoardId`.
  - Non-destructive: generated elements append to the existing board (server route already appends); `setBoard` replaces store state with the server's (post-generate) copy so `board.elements` stays consistent for the panel.

- [x] **Step 1: Write the failing component test**

```tsx
// apps/Nexus-Draw/frontend/src/components/AIPanel.test.tsx
import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import AIPanel from "./AIPanel";
import * as api from "../utils/api";

const sb = { id: "b1", name: "N", description: "", width: 1920, height: 1080, background: "#fff", isPublic: false, defaultStyleMode: "clean" as const, gridSnap: false, elements: [{ id: "g1", elementType: "rectangle", data: { x: 0, y: 0 }, style: {}, transform: { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 }, order: 0, seed: 1 }], collaborators: [], createdAt: "x", updatedAt: "x" };

describe("AIPanel", () => {
  beforeEach(() => {
    vi.spyOn(api, "generateDiagram").mockResolvedValue({ elements: [sb.elements[0] as never], board_id: "b1" });
    vi.spyOn(api, "getBoard").mockResolvedValue(sb as never);
  });

  it("generates and reports N elements", async () => {
    render(<AIPanel boardId="b1" currentElements={sb.elements as never} />);
    fireEvent.change(screen.getByPlaceholderText("Describe a diagram… e.g. login flow with auth"), { target: { value: "login flow" } });
    fireEvent.click(screen.getByText("Generate"));
    await waitFor(() => expect(screen.getByText(/added 1 element/i)).toBeTruthy());
    expect(api.generateDiagram).toHaveBeenCalledWith("login flow", "b1");
  });

  it("shows an error when generation fails", async () => {
    vi.spyOn(api, "generateDiagram").mockRejectedValue(new Error("boom"));
    render(<AIPanel boardId="b1" currentElements={[]} />);
    fireEvent.change(screen.getByPlaceholderText("Describe a diagram… e.g. login flow with auth"), { target: { value: "x" } });
    fireEvent.click(screen.getByText("Generate"));
    await waitFor(() => expect(screen.getByText(/error/i)).toBeTruthy());
  });

  it("disables the button while generating", async () => {
    render(<AIPanel boardId="b1" currentElements={[]} />);
    fireEvent.change(screen.getByPlaceholderText("Describe a diagram… e.g. login flow with auth"), { target: { value: "x" } });
    fireEvent.click(screen.getByText("Generate"));
    expect((screen.getByText("Generating…") as HTMLButtonElement).disabled).toBe(true);
  });
});
```

- [x] **Step 2: Run to verify it fails** — `cd apps/Nexus-Draw/frontend && bun test src/components/AIPanel.test.tsx` → FAIL (module missing).

- [x] **Step 3: Implement `AIPanel.tsx`**

```tsx
import { useState } from "react";
import * as api from "../utils/api";
import { useEditorStore } from "../stores/useEditorStore";
import { saveLastBoardId } from "../utils/persistence";

type Status = "idle" | "generating" | "done" | "error";

export default function AIPanel({ boardId }: { boardId: string }) {
  const [prompt, setPrompt] = useState("");
  const [status, setStatus] = useState<Status>("idle");
  const [detail, setDetail] = useState("");

  const run = async () => {
    if (!prompt.trim() || status === "generating") return;
    setStatus("generating"); setDetail("");
    try {
      const { elements } = await api.generateDiagram(prompt.trim(), boardId);
      const sb = await api.getBoard(boardId);
      const store = useEditorStore.getState();
      store.setBoard(api.serverBoardToBoardData(sb));
      const newIds = (elements as Array<{ id: string }>).map((e) => e.id);
      store.setSelection(newIds);
      saveLastBoardId(boardId);
      setStatus("done");
      setDetail(`added ${newIds.length} element${newIds.length === 1 ? "" : "s"}`);
    } catch (e) {
      setStatus("error");
      setDetail(e instanceof Error ? e.message : "generation failed");
    }
  };

  return (
    <div className="flex flex-col h-full p-2 gap-2">
      <div className="text-xs font-semibold text-zinc-400 uppercase">AI Generate</div>
      <textarea
        value={prompt}
        onChange={(e) => setPrompt(e.target.value)}
        placeholder="Describe a diagram… e.g. login flow with auth"
        rows={5}
        className="flex-1 min-h-[80px] text-xs bg-zinc-800 rounded p-2 outline-none resize-none"
      />
      <button
        onClick={run}
        disabled={status === "generating" || !prompt.trim()}
        className="px-2 py-1.5 text-xs bg-blue-600 rounded hover:bg-blue-500 disabled:opacity-40"
      >
        {status === "generating" ? "Generating…" : "Generate"}
      </button>
      {status === "done" && <div className="text-xs text-emerald-400">{detail}</div>}
      {status === "error" && <div className="text-xs text-red-400">Error: {detail}</div>}
      {status === "idle" && <div className="text-[11px] text-zinc-600">Generates a diagram flow on the canvas.</div>}
    </div>
  );
}
```

- [x] **Step 4: Wire into App** — render `AIPanel` when `sidebar === "ai"` (extend the sidebar union + toggles as in A4); add an "AI" entry button in the bottom bar. Where `board` is null render nothing.

- [x] **Step 5: Run to verify it passes** — `cd apps/Nexus-Draw/frontend && bun test src/components/AIPanel.test.tsx && bun run check` → PASS / clean.

- [x] **Step 6: Commit** — `git add apps/Nexus-Draw/frontend/src && git commit -m "feat(draw): AI generation panel with apply-to-canvas"`

---

## Final verification

- [x] **Step 1** — `cd apps/Nexus-Draw && bun test && bun run check` → all green.
- [x] **Step 2** — `cd apps/Nexus-Draw/frontend && bun test && bun run check` → all green.
- [x] **Step 3** — `docker compose up -d` (Postgres/Redis/MinIO for the optional FastAPI AI path) and `cd apps/Nexus-Draw/backend && uvicorn app.main:app --port 3081` → health OK (backend untouched but still starts). Run `bun run dev` on the server and `cd frontend && bun run dev` → open two browser tabs on the same board and confirm live edits propagate; generate a diagram → elements appear on canvas.
- [x] **Step 4** — `git status` clean of stray files; commit any remaining formatting fixes: `git commit -m "chore(draw): post-integration cleanup"`.

---

## Self-Review (plan author)

- **Spec coverage:** server sync + board management → Phase A (engine A1, routes A2, client A3, panel+boot A4); real-time collab → Phase B (reconcile B1, Bun protocol server+persistence B2, provider+UI B3); AI UI → Phase C (synthesizer C1, endpoint C2, panel C3). Gameplan (inventory + roadmap) is this document + the git-log summary already gathered.
- **Placeholders:** all logic-carrying tasks carry real tests + implementation. The collab integration test references inline protocol helpers — the plan instructs the worker to build them with the same `lib0`/`y-protocols` primitives the server uses so the bytes match exactly; the authoritative assertion is the element landing in SQLite via the server while two live sockets exchange sync/update frames.
- **Type consistency:** `ServerBoard` (A3) consumed by A4 + C3 UI; `boardToServerBoard`/`serverBoardToBoardData` names stable; `AiElement` (C1) matches shape `AiElement[]` returned by the route and consumed by `AIPanel`; `CollabBinding`/`connectCollab` (B3) names match App wiring; store gains `collabActive` (B3) and sidebar gains `"boards"` (A4) + `"ai"` (C3).
- **Dependency order:** A1→A2→A3→A4 strictly; B1→B2 (server) and B1→B3 (frontend); C1→C2→C3. B phases come after A so boards exist server-side before collab; C comes after A (board persistence needed for the append flow). Fits subagent-per-task with review between.