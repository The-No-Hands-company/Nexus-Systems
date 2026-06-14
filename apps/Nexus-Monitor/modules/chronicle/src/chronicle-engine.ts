import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface ChronicleEvent {
  id: string;
  name: string;
  events: string;
  sessions: string;
  traces: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class ChronicleEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, events TEXT, sessions TEXT, traces TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, events?: string, sessions?: string, traces?: string): ChronicleEvent {
    const item: ChronicleEvent = {
      id: randomUUID(),
      name,
      events: events || "",
      sessions: sessions || "",
      traces: traces || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, events, sessions, traces, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.events,
        item.sessions,
        item.traces,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): ChronicleEvent[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): ChronicleEvent | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
