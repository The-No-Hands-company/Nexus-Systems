import { Database } from "bun:sqlite";

export type Visibility = "public" | "private";
export type AccessLevel = "read" | "write" | "admin";

export interface UserRecord {
  id: number;
  username: string;
}

export interface RepositoryRecord {
  id: number;
  name: string;
  description: string | null;
  visibility: Visibility;
  owner_id: number;
  created_at: string;
}

export interface ActivityRecord {
  id: number;
  repo_id: number;
  action: string;
  actor_id: number | null;
  details: string | null;
  created_at: string;
}

const SCHEMA_VERSION = 1;

/**
 * Forge metadata: users, access tokens, repositories, per-repository grants
 * and the activity feed.
 *
 * There is no in-memory fallback. A forge that silently loses its grants on
 * restart is a forge whose access control resets to whatever the fallback
 * says, so a database that cannot be opened is a startup failure.
 */
export class ForgeDB {
  readonly db: Database;

  constructor(path: string) {
    this.db = new Database(path, { create: true, strict: true });
    this.db.exec("PRAGMA foreign_keys = ON");
    this.db.exec("PRAGMA journal_mode = WAL");
    this.migrate();
  }

  private migrate(): void {
    const { user_version: version } = this.db.query("PRAGMA user_version").get() as {
      user_version: number;
    };
    if (version === SCHEMA_VERSION) return;
    if (version > SCHEMA_VERSION) {
      throw new Error(
        `forge database schema v${version} is newer than this code (v${SCHEMA_VERSION})`,
      );
    }
    const tables = this.db.query("SELECT name FROM sqlite_master WHERE type = 'table'").all();
    if (tables.length > 0) {
      throw new Error("forge database has tables but no schema version; refusing to adopt it");
    }
    this.db.transaction(() => {
      this.db.exec(`
        CREATE TABLE users (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          username TEXT UNIQUE NOT NULL,
          created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE tokens (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
          token_hash TEXT UNIQUE NOT NULL,
          expires_at INTEGER NOT NULL,
          revoked INTEGER NOT NULL DEFAULT 0,
          created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE repositories (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          name TEXT UNIQUE NOT NULL,
          description TEXT,
          visibility TEXT NOT NULL CHECK (visibility IN ('public', 'private')),
          owner_id INTEGER NOT NULL REFERENCES users(id),
          created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE grants (
          repo_id INTEGER NOT NULL REFERENCES repositories(id) ON DELETE CASCADE,
          user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
          level TEXT NOT NULL CHECK (level IN ('read', 'write', 'admin')),
          PRIMARY KEY (repo_id, user_id)
        );
        CREATE TABLE activity (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          repo_id INTEGER NOT NULL REFERENCES repositories(id) ON DELETE CASCADE,
          action TEXT NOT NULL,
          actor_id INTEGER REFERENCES users(id),
          details TEXT,
          created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
      `);
      this.db.exec(`PRAGMA user_version = ${SCHEMA_VERSION}`);
    })();
  }

  addUser(username: string): UserRecord {
    return this.db
      .query("INSERT INTO users (username) VALUES (?) RETURNING id, username")
      .get(username) as UserRecord;
  }

  getUserByName(username: string): UserRecord | null {
    return this.db
      .query("SELECT id, username FROM users WHERE username = ?")
      .get(username) as UserRecord | null;
  }

  addToken(userId: number, tokenHash: string, expiresAt: number): void {
    this.db
      .query("INSERT INTO tokens (user_id, token_hash, expires_at) VALUES (?, ?, ?)")
      .run(userId, tokenHash, expiresAt);
  }

  /** The user a live (unexpired, unrevoked) token belongs to. */
  userForTokenHash(tokenHash: string, now: number): UserRecord | null {
    return this.db
      .query(
        `SELECT u.id, u.username FROM tokens t JOIN users u ON u.id = t.user_id
         WHERE t.token_hash = ? AND t.revoked = 0 AND t.expires_at > ?`,
      )
      .get(tokenHash, now) as UserRecord | null;
  }

  revokeTokensFor(userId: number): void {
    this.db.query("UPDATE tokens SET revoked = 1 WHERE user_id = ?").run(userId);
  }

  addRepository(
    name: string,
    visibility: Visibility,
    ownerId: number,
    description: string | null,
  ): RepositoryRecord {
    return this.db.transaction(() => {
      const repo = this.db
        .query(
          `INSERT INTO repositories (name, description, visibility, owner_id)
           VALUES (?, ?, ?, ?) RETURNING *`,
        )
        .get(name, description, visibility, ownerId) as RepositoryRecord;
      this.setGrant(repo.id, ownerId, "admin");
      return repo;
    })();
  }

  getRepository(name: string): RepositoryRecord | null {
    return this.db
      .query("SELECT * FROM repositories WHERE name = ?")
      .get(name) as RepositoryRecord | null;
  }

  /** Repositories the user may read: public ones plus those with a grant. */
  listReadableRepositories(userId: number | null, limit = 50, skip = 0): RepositoryRecord[] {
    return this.db
      .query(
        `SELECT r.* FROM repositories r
         WHERE r.visibility = 'public'
            OR EXISTS (SELECT 1 FROM grants g WHERE g.repo_id = r.id AND g.user_id = ?)
         ORDER BY r.created_at DESC, r.id DESC LIMIT ? OFFSET ?`,
      )
      .all(userId ?? -1, limit, skip) as RepositoryRecord[];
  }

  setGrant(repoId: number, userId: number, level: AccessLevel): void {
    this.db
      .query(
        `INSERT INTO grants (repo_id, user_id, level) VALUES (?, ?, ?)
         ON CONFLICT (repo_id, user_id) DO UPDATE SET level = excluded.level`,
      )
      .run(repoId, userId, level);
  }

  getGrant(repoId: number, userId: number): AccessLevel | null {
    const row = this.db
      .query("SELECT level FROM grants WHERE repo_id = ? AND user_id = ?")
      .get(repoId, userId) as { level: AccessLevel } | null;
    return row?.level ?? null;
  }

  logActivity(repoId: number, action: string, actorId: number | null, details: string): void {
    this.db
      .query("INSERT INTO activity (repo_id, action, actor_id, details) VALUES (?, ?, ?, ?)")
      .run(repoId, action, actorId, details);
  }

  getActivity(repoId: number, limit = 50): ActivityRecord[] {
    return this.db
      .query("SELECT * FROM activity WHERE repo_id = ? ORDER BY id DESC LIMIT ?")
      .all(repoId, limit) as ActivityRecord[];
  }

  close(): void {
    this.db.close();
  }
}
