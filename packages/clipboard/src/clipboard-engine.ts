import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface ClipboardItem {
  id: string;
  name: string;
  items: string;
  groups: string;
  tags: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class ClipboardEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, items TEXT, groups TEXT, tags TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, items?: string, groups?: string, tags?: string): ClipboardItem {
    const item: ClipboardItem = {
      id: randomUUID(),
      name,
      items: items || "",
      groups: groups || "",
      tags: tags || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, items, groups, tags, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.items,
        item.groups,
        item.tags,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): ClipboardItem[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): ClipboardItem | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
