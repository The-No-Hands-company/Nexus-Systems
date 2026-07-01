import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface ContextState {
  id: string;
  name: string;
  states: string;
  sessions: string;
  keys: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class ContextEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, states TEXT, sessions TEXT, keys TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, states?: string, sessions?: string, keys?: string): ContextState {
    const item: ContextState = {
      id: randomUUID(),
      name,
      states: states || "",
      sessions: sessions || "",
      keys: keys || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, states, sessions, keys, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.states,
        item.sessions,
        item.keys,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): ContextState[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): ContextState | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
