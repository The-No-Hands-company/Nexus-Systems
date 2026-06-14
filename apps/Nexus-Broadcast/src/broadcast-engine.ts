import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface BroadcastSession {
  id: string;
  name: string;
  sessions: string;
  streams: string;
  recordings: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class BroadcastEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, sessions TEXT, streams TEXT, recordings TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, sessions?: string, streams?: string, recordings?: string): BroadcastSession {
    const item: BroadcastSession = {
      id: randomUUID(),
      name,
      sessions: sessions || "",
      streams: streams || "",
      recordings: recordings || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, sessions, streams, recordings, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.sessions,
        item.streams,
        item.recordings,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): BroadcastSession[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): BroadcastSession | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
