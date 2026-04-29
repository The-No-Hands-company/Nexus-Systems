import Database from "better-sqlite3";

export interface RepositoryRecord {
  id: number;
  name: string;
  description?: string;
  vcs: "git" | "svn" | "hg" | "pijul";
  owner_id: number | null;
  created_at: string;
  updated_at: string;
}

interface ActivityRecord {
  id: number;
  repo_id: number;
  action: string;
  actor_id: number | null;
  details: string | null;
  created_at: string;
}

/**
 * SQLite database layer
 * Handles teams, permissions, repositories metadata, activity logs
 */

export class ForgeDB {
  private db: Database.Database | null = null;
  private inMemoryRepositories: RepositoryRecord[] = [];
  private inMemoryActivity: ActivityRecord[] = [];
  private nextRepoId = 1;
  private nextActivityId = 1;

  constructor(dbPath: string) {
    try {
      this.db = new Database(dbPath);
      this.initTables();
    } catch (error) {
      this.db = null;
      console.warn("[ForgeDB] SQLite native bindings unavailable, using in-memory storage fallback.");
      console.warn(error);
    }
  }

  private initTables(): void {
    if (!this.db) return;

    // Repositories table
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS repositories (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT UNIQUE NOT NULL,
        description TEXT,
        vcs TEXT NOT NULL,
        owner_id INTEGER,
        created_at TEXT DEFAULT CURRENT_TIMESTAMP,
        updated_at TEXT DEFAULT CURRENT_TIMESTAMP
      )
    `);

    // Teams table
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS teams (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT UNIQUE NOT NULL,
        description TEXT,
        created_at TEXT DEFAULT CURRENT_TIMESTAMP
      )
    `);

    // Team members table
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS team_members (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        team_id INTEGER NOT NULL,
        user_id INTEGER NOT NULL,
        role TEXT NOT NULL,
        FOREIGN KEY (team_id) REFERENCES teams(id)
      )
    `);

    // Repository permissions table
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS permissions (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        repo_id INTEGER NOT NULL,
        team_id INTEGER NOT NULL,
        access_level TEXT NOT NULL,
        FOREIGN KEY (repo_id) REFERENCES repositories(id),
        FOREIGN KEY (team_id) REFERENCES teams(id)
      )
    `);

    // Activity log table
    this.db.exec(`
      CREATE TABLE IF NOT EXISTS activity (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        repo_id INTEGER NOT NULL,
        action TEXT NOT NULL,
        actor_id INTEGER,
        details TEXT,
        created_at TEXT DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (repo_id) REFERENCES repositories(id)
      )
    `);
  }

  // Repository methods
  addRepository(name: string, vcs: string, description?: string, ownerId?: number): void {
    if (!this.db) {
      const now = new Date().toISOString();
      this.inMemoryRepositories.push({
        id: this.nextRepoId++,
        name,
        description,
        vcs: vcs as RepositoryRecord["vcs"],
        owner_id: ownerId || null,
        created_at: now,
        updated_at: now,
      });
      return;
    }

    const stmt = this.db.prepare(
      "INSERT INTO repositories (name, vcs, description, owner_id) VALUES (?, ?, ?, ?)"
    );
    stmt.run(name, vcs, description || null, ownerId || null);
  }

  getRepository(name: string): RepositoryRecord | undefined {
    if (!this.db) {
      return this.inMemoryRepositories.find((repo) => repo.name === name);
    }

    const stmt = this.db.prepare("SELECT * FROM repositories WHERE name = ?");
    return stmt.get(name) as RepositoryRecord | undefined;
  }

  listRepositories(limit = 50, skip = 0): RepositoryRecord[] {
    if (!this.db) {
      return this.inMemoryRepositories.slice(skip, skip + limit);
    }

    const stmt = this.db.prepare("SELECT * FROM repositories ORDER BY created_at DESC LIMIT ? OFFSET ?");
    return stmt.all(limit, skip) as RepositoryRecord[];
  }

  // Activity logging
  logActivity(repoId: number, action: string, actorId?: number, details?: string): void {
    if (!this.db) {
      this.inMemoryActivity.push({
        id: this.nextActivityId++,
        repo_id: repoId,
        action,
        actor_id: actorId || null,
        details: details || null,
        created_at: new Date().toISOString(),
      });
      return;
    }

    const stmt = this.db.prepare(
      "INSERT INTO activity (repo_id, action, actor_id, details) VALUES (?, ?, ?, ?)"
    );
    stmt.run(repoId, action, actorId || null, details || null);
  }

  getActivity(repoId: number, limit = 50) {
    if (!this.db) {
      return this.inMemoryActivity.filter((item) => item.repo_id === repoId).slice(0, limit);
    }

    const stmt = this.db.prepare(
      "SELECT * FROM activity WHERE repo_id = ? ORDER BY created_at DESC LIMIT ?"
    );
    return stmt.all(repoId, limit);
  }

  close(): void {
    if (this.db) this.db.close();
  }
}
