import { Database } from "bun:sqlite";
import { readFileSync, existsSync, mkdirSync } from "node:fs";
import { join } from "node:path";
import type { FileRecord, FileVersionRecord } from "./storage";

export class FilesDatabase {
  private db: Database;

  constructor(dbPath?: string) {
    const finalPath = dbPath || process.env.NEXUS_FILES_DB || join(process.cwd(), "data", "nexus-files.sqlite");
    const dir = join(finalPath, "..");
    if (!existsSync(dir)) {
      mkdirSync(dir, { recursive: true });
    }
    this.db = new Database(finalPath);
    this.migrate();
  }

  private migrate(): void {
    const schemaPath = join(import.meta.dir, "..", "db", "schema.sql");
    if (existsSync(schemaPath)) {
      const sql = readFileSync(schemaPath, "utf-8");
      this.db.exec(sql);
    }
  }

  upsertFile(file: FileRecord): void {
    const stmt = this.db.prepare(`
      INSERT INTO files (id, name, size, content_type, sha256, version, status, created_at, updated_at)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
      ON CONFLICT(id) DO UPDATE SET
        name = excluded.name,
        size = excluded.size,
        content_type = excluded.content_type,
        sha256 = excluded.sha256,
        version = excluded.version,
        status = excluded.status,
        updated_at = excluded.updated_at
    `);
    stmt.run(file.id, file.name, file.size, file.contentType, file.sha256, file.version, file.status, file.createdAt, file.updatedAt);
  }

  getFile(id: string): FileRecord | null {
    const stmt = this.db.prepare(`SELECT * FROM files WHERE id = ?`);
    const row = stmt.get(id) as Record<string, unknown> | null;
    if (!row) return null;
    return {
      id: String(row.id),
      name: String(row.name),
      size: Number(row.size),
      contentType: String(row.content_type),
      sha256: String(row.sha256),
      version: Number(row.version),
      status: row.status as "active" | "deleted",
      createdAt: String(row.created_at),
      updatedAt: String(row.updated_at),
    };
  }

  close(): void {
    this.db.close();
  }
}
