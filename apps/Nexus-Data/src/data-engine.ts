import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface DataRecord {
  id: string;
  name: string;
  records: string;
  collections: string;
  schemas: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class DataEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, records TEXT, collections TEXT, schemas TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, records?: string, collections?: string, schemas?: string): DataRecord {
    const item: DataRecord = {
      id: randomUUID(),
      name,
      records: records || "",
      collections: collections || "",
      schemas: schemas || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, records, collections, schemas, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.records,
        item.collections,
        item.schemas,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): DataRecord[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): DataRecord | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
