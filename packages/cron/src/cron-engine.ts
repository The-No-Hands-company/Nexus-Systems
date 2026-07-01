import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface CronJob {
  id: string;
  name: string;
  jobs: string;
  schedules: string;
  executions: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class CronEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, jobs TEXT, schedules TEXT, executions TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, jobs?: string, schedules?: string, executions?: string): CronJob {
    const item: CronJob = {
      id: randomUUID(),
      name,
      jobs: jobs || "",
      schedules: schedules || "",
      executions: executions || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, jobs, schedules, executions, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.jobs,
        item.schedules,
        item.executions,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): CronJob[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): CronJob | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
