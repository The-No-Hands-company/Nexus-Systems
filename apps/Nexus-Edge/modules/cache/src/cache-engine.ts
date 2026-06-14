import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface CacheEntry {
  id: string;
  name: string;
  entries: string;
  keys: string;
  ttl: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class CacheEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, entries TEXT, keys TEXT, ttl TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, entries?: string, keys?: string, ttl?: string): CacheEntry {
    const item: CacheEntry = {
      id: randomUUID(),
      name,
      entries: entries || "",
      keys: keys || "",
      ttl: ttl || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, entries, keys, ttl, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.entries,
        item.keys,
        item.ttl,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): CacheEntry[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): CacheEntry | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
