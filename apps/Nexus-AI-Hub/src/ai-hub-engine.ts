import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface AIPrompt {
  id: string;
  name: string;
  ai_models: string;
  prompts: string;
  templates: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class AIHubEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, ai_models TEXT, prompts TEXT, templates TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, ai_models?: string, prompts?: string, templates?: string): AIPrompt {
    const item: AIPrompt = {
      id: randomUUID(),
      name,
      ai_models: ai_models || "",
      prompts: prompts || "",
      templates: templates || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, ai_models, prompts, templates, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.ai_models,
        item.prompts,
        item.templates,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): AIPrompt[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): AIPrompt | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
