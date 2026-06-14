import { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";

export interface UserAccount {
  id: string;
  name: string;
  accounts: string;
  profiles: string;
  roles: string;
  properties: Record<string, unknown>;
  createdAt: string;
}

export class AccountEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, accounts TEXT, profiles TEXT, roles TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );
  }

  create(name: string, accounts?: string, profiles?: string, roles?: string): UserAccount {
    const item: UserAccount = {
      id: randomUUID(),
      name,
      accounts: accounts || "",
      profiles: profiles || "",
      roles: roles || "",
      properties: {},
      createdAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO items (id, name, accounts, profiles, roles, properties, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(
        item.id,
        item.name,
        item.accounts,
        item.profiles,
        item.roles,
        JSON.stringify(item.properties),
        item.createdAt
      );
    return item;
  }

  list(): UserAccount[] {
    return (this.db.prepare("SELECT * FROM items ORDER BY created_at DESC").all() as any[]).map(
      (r: any) => ({ ...r, properties: JSON.parse(r.properties) })
    );
  }

  get(id: string): UserAccount | undefined {
    const r = this.db.prepare("SELECT * FROM items WHERE id = ?").get(id) as any;
    return r ? { ...r, properties: JSON.parse(r.properties) } : undefined;
  }
}
