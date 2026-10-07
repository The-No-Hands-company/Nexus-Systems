import { describe, expect, it } from "bun:test";
import { mkdtemp, rm } from "node:fs/promises";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { TerminalAudit } from "../src/audit";

describe("TerminalAudit", () => {
  it("finalizes a session only once across racing cleanup paths", async () => {
    const root = await mkdtemp(join(tmpdir(), "nexus-terminal-audit."));
    const path = join(root, "audit.sqlite");
    const audit = new TerminalAudit(path);

    try {
      audit.begin("session-1", "founder-1");
      audit.end("session-1", 15);
      audit.end("session-1", 9);

      const record = audit.recent(1)[0] as { exit_code?: number } | undefined;
      expect(record?.exit_code).toBe(15);
    } finally {
      (audit as unknown as { close?: () => void }).close?.();
      await rm(root, { recursive: true, force: true });
    }
  });
});

describe("TerminalAudit zero-retention migration", () => {
  it("drops the legacy remote_ip column and keeps the rows", async () => {
    const { Database } = await import("bun:sqlite");
    const root = await mkdtemp(join(tmpdir(), "nexus-terminal-audit."));
    const path = join(root, "audit.sqlite");
    try {
      const old = new Database(path, { create: true });
      old.exec("CREATE TABLE sessions (id TEXT PRIMARY KEY, subject TEXT NOT NULL, remote_ip TEXT, started_at TEXT NOT NULL, ended_at TEXT, exit_code INTEGER)");
      old.exec("INSERT INTO sessions (id, subject, remote_ip, started_at) VALUES ('s','u','203.0.113.9','t')");
      old.close();
      const audit = new TerminalAudit(path);
      audit.close();
      const db = new Database(path);
      const cols = (db.query("PRAGMA table_info(sessions)").all() as Array<{ name: string }>).map((c) => c.name);
      expect(cols).not.toContain("remote_ip");
      expect((db.query("SELECT count(*) AS n FROM sessions").get() as { n: number }).n).toBe(1);
      db.close();
    } finally {
      await rm(root, { recursive: true, force: true });
    }
  });
});
