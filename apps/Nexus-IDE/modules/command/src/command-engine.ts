import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface CommandEntry {
  id: string;
  name: string;
  commands: string;
  palettes: string;
  bindings: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class CommandEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, commands TEXT, palettes TEXT, bindings TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, commands?: string, palettes?: string, bindings?: string): CommandEntry {
    const item: CommandEntry = {
      id: randomUUID(),
      name,
      commands: commands || "",
      palettes: palettes || "",
      bindings: bindings || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, commands, palettes, bindings, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.commands,
        item.palettes,
        item.bindings,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): CommandEntry[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): CommandEntry | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
