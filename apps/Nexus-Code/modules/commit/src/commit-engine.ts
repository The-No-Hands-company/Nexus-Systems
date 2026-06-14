import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface CommitRecord {
  id: string;
  name: string;
  commits: string;
  reviews: string;
  patches: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class CommitEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, commits TEXT, reviews TEXT, patches TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, commits?: string, reviews?: string, patches?: string): CommitRecord {
    const item: CommitRecord = {
      id: randomUUID(),
      name,
      commits: commits || "",
      reviews: reviews || "",
      patches: patches || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, commits, reviews, patches, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.commits,
        item.reviews,
        item.patches,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): CommitRecord[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): CommitRecord | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
