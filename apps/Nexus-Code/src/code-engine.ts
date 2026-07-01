import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface CodeSnippet {
  id: string;
  name: string;
  snippets: string;
  repos: string;
  files: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class CodeEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, snippets TEXT, repos TEXT, files TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, snippets?: string, repos?: string, files?: string): CodeSnippet {
    const item: CodeSnippet = {
      id: randomUUID(),
      name,
      snippets: snippets || "",
      repos: repos || "",
      files: files || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, snippets, repos, files, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.snippets,
        item.repos,
        item.files,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): CodeSnippet[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): CodeSnippet | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
