import { describe, expect, it } from "bun:test";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { openDatabase, transaction } from "../src/store/db";

const TABLES = [
  "calendar_exceptions",
  "dependencies",
  "project_members",
  "projects",
  "statuses",
  "tasks",
  "workspace_members",
  "workspaces",
];

describe("database", () => {
  it("creates every table at schema version 1 with foreign keys enforced", () => {
    const db = openDatabase(":memory:");
    expect((db.query("PRAGMA user_version").get() as { user_version: number }).user_version).toBe(
      1,
    );
    expect((db.query("PRAGMA foreign_keys").get() as { foreign_keys: number }).foreign_keys).toBe(
      1,
    );
    const tables = (
      db.query("SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name").all() as {
        name: string;
      }[]
    ).map((t) => t.name);
    expect(tables).toEqual(TABLES);
    expect(() =>
      db
        .query("INSERT INTO workspace_members VALUES ('missing', 's', 'owner', 'x', 'x', 'x', 'x')")
        .run(),
    ).toThrow();
    db.close();
  });

  it("keeps data and does not re-run migrations when reopened", () => {
    const dir = mkdtempSync(join(tmpdir(), "nexus-project-db-"));
    try {
      const path = join(dir, "project.sqlite");
      const first = openDatabase(path);
      first
        .query("INSERT INTO workspaces VALUES ('w1', 'Team', 'team', NULL, 'x', 'x', 's', 's')")
        .run();
      first.close();
      const second = openDatabase(path);
      expect(second.query("SELECT name FROM workspaces").all()).toEqual([{ name: "Team" }]);
      second.close();
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it("rolls back a transaction that throws", () => {
    const db = openDatabase(":memory:");
    expect(() =>
      transaction(db, () => {
        db.query(
          "INSERT INTO workspaces VALUES ('w1', 'Team', 'team', NULL, 'x', 'x', 's', 's')",
        ).run();
        throw new Error("boom");
      }),
    ).toThrow("boom");
    expect(db.query("SELECT COUNT(*) AS n FROM workspaces").get()).toEqual({ n: 0 });
    db.close();
  });
});
