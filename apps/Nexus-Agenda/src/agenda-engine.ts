import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface AgendaItem {
  id: string;
  name: string;
  items: string;
  meetings: string;
  tasks: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class AgendaEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, items TEXT, meetings TEXT, tasks TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, items?: string, meetings?: string, tasks?: string): AgendaItem {
    const item: AgendaItem = {
      id: randomUUID(),
      name,
      items: items || "",
      meetings: meetings || "",
      tasks: tasks || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, items, meetings, tasks, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.items,
        item.meetings,
        item.tasks,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): AgendaItem[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): AgendaItem | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
