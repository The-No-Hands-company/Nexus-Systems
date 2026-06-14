import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface AgentConfig {
  id: string;
  name: string;
  agents: string;
  configs: string;
  workflows: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class AgentsEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, agents TEXT, configs TEXT, workflows TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, agents?: string, configs?: string, workflows?: string): AgentConfig {
    const item: AgentConfig = {
      id: randomUUID(),
      name,
      agents: agents || "",
      configs: configs || "",
      workflows: workflows || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, agents, configs, workflows, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.agents,
        item.configs,
        item.workflows,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): AgentConfig[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): AgentConfig | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
