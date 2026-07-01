import { Database } from "bun:sqlite"; import { randomUUID } from "node:crypto";

export interface Portal {
  id: string;
  name: string;
  description: string;
  type: string;
  createdAt: string;
  updatedAt: string;
}

export class PortalEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS portals (id TEXT PRIMARY KEY, name TEXT, description TEXT, type TEXT, created_at TEXT, updated_at TEXT)",
    );
  }

  createPortal(name: string, type = ""): Portal {
    const d: Portal = {
      id: randomUUID(),
      name,
      description: "",
      type: type,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO portals VALUES (?,?,?,?,?,?)")
      .run(d.id, d.name, d.description, d.type, d.createdAt, d.updatedAt);
    return d;
  }

  listPortals(): Portal[] {
    return this.db.prepare("SELECT * FROM portals ORDER BY updated_at DESC").all() as Portal[];
  }

  getPortal(id: string): Portal | undefined {
    return this.db.prepare("SELECT * FROM portals WHERE id = ?").get(id) as Portal | undefined;
  }
}
