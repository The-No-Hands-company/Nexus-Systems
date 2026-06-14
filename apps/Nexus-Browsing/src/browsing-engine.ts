import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface Bookmark {
  id: string;
  name: string;
  bookmarks: string;
  history: string;
  collections: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class BrowsingEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, bookmarks TEXT, history TEXT, collections TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, bookmarks?: string, history?: string, collections?: string): Bookmark {
    const item: Bookmark = {
      id: randomUUID(),
      name,
      bookmarks: bookmarks || "",
      history: history || "",
      collections: collections || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, bookmarks, history, collections, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.bookmarks,
        item.history,
        item.collections,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): Bookmark[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): Bookmark | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
