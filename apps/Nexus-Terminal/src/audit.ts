import { Database } from "bun:sqlite";

/**
 * The record of who ran what.
 *
 * With an uncontained shell this is the only forensic surface there is: no
 * sandbox boundary was crossed, no permission was denied, nothing else will
 * have noticed. If something goes wrong on this host, this table is how anyone
 * finds out what happened.
 *
 * Input is recorded verbatim, including what a user types into a password
 * prompt — a shell has no way to tell the difference. That is a real cost of
 * auditing keystrokes and it belongs in the open rather than as a surprise.
 */
export class TerminalAudit {
  private db: Database;

  constructor(path = "data/terminal-audit.sqlite") {
    this.db = new Database(path, { create: true });
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS sessions (
        id          TEXT PRIMARY KEY,
        subject     TEXT NOT NULL,
        remote_ip   TEXT,
        started_at  TEXT NOT NULL,
        ended_at    TEXT,
        exit_code   INTEGER
      );
      CREATE TABLE IF NOT EXISTS keystrokes (
        id          INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id  TEXT NOT NULL,
        at          TEXT NOT NULL,
        data        TEXT NOT NULL
      );
      CREATE INDEX IF NOT EXISTS keystrokes_session_idx ON keystrokes (session_id, at);
    `);
  }

  begin(id: string, subject: string, remoteIp: string | null): void {
    this.db
      .query("INSERT INTO sessions (id, subject, remote_ip, started_at) VALUES (?, ?, ?, ?)")
      .run(id, subject, remoteIp, new Date().toISOString());
  }

  input(sessionId: string, data: string): void {
    this.db
      .query("INSERT INTO keystrokes (session_id, at, data) VALUES (?, ?, ?)")
      .run(sessionId, new Date().toISOString(), data);
  }

  end(id: string, exitCode: number): void {
    this.db
      .query("UPDATE sessions SET ended_at = ?, exit_code = ? WHERE id = ?")
      .run(new Date().toISOString(), exitCode, id);
  }

  /** Sessions, newest first. For an operator reviewing activity. */
  recent(limit = 50): unknown[] {
    return this.db
      .query("SELECT * FROM sessions ORDER BY started_at DESC LIMIT ?")
      .all(limit);
  }
}
