import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Support {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class SupportEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS supports (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createSupport(name: string, type = ""): Support {
    const d: Support = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO supports VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listSupports(): Support[] {
    return this.db.prepare("SELECT * FROM supports ORDER BY updated_at DESC").all() as Support[];
  }

  getSupport(id: string): Support | undefined {
    return this.db.prepare("SELECT * FROM supports WHERE id = ?").get(id) as Support | undefined;
  }
}
