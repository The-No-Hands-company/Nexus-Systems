import { Database, type SQLQueryBindings } from "bun:sqlite";
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
    const fields: Record<string, SQLQueryBindings> = {};
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