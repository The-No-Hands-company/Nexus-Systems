import { Database } from "bun:sqlite";
import { randomUUID, createHash } from "node:crypto";

export interface Repository {
  id: string;
  name: string;
  description: string;
  defaultBranch: string;
  visibility: "public" | "private";
  createdAt: string;
  updatedAt: string;
}

export interface CommitRecord {
  id: string;
  repoId: string;
  hash: string;
  branch: string;
  message: string;
  author: string;
  createdAt: string;
}

export interface RepoFile {
  id: string;
  repoId: string;
  branch: string;
  path: string;
  type: "file" | "dir";
  content: string;
  size: number;
  updatedAt: string;
}

export interface Snippet {
  id: string;
  title: string;
  language: string;
  code: string;
  tags: string;
  createdAt: string;
  updatedAt: string;
}

export class CodeEngine {
  db: Database;

  constructor(p = ":memory:") {
    this.db = new Database(p);
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS repos (id TEXT PRIMARY KEY, name TEXT UNIQUE, description TEXT, default_branch TEXT, visibility TEXT, created_at TEXT, updated_at TEXT)"
    );
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS commits (id TEXT PRIMARY KEY, repo_id TEXT, hash TEXT, branch TEXT, message TEXT, author TEXT, created_at TEXT)"
    );
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS files (id TEXT PRIMARY KEY, repo_id TEXT, branch TEXT, path TEXT, type TEXT, content TEXT, size INTEGER, updated_at TEXT, UNIQUE(repo_id, branch, path))"
    );
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS snippets (id TEXT PRIMARY KEY, title TEXT, language TEXT, code TEXT, tags TEXT, created_at TEXT, updated_at TEXT)"
    );
    // Legacy items table compatibility
    this.db.exec(
      "CREATE TABLE IF NOT EXISTS items (id TEXT PRIMARY KEY, name TEXT, snippets TEXT, repos TEXT, files TEXT, properties TEXT DEFAULT '{}', created_at TEXT)"
    );

    this.seedDefaultData();
  }

  private seedDefaultData() {
    const existing = this.db.prepare("SELECT count(*) as count FROM repos").get() as { count: number };
    if (existing && existing.count > 0) return;

    // Seed sample repository
    const repo = this.createRepo("nexus-core", "Core architecture, primitives, and sovereign security specifications", "main");
    this.saveFile(repo.id, "README.md", "# Nexus Core\n\nSovereign platform engine and modular application mesh.\n\n## Architecture\n- Zero-trust capability model\n- Local-first sqlite stores\n- Quantum-safe did:phantom cryptographic identity\n", "Initial commit", "zajferx", "main");
    this.saveFile(repo.id, "src/index.ts", "export const VERSION = '2.0.0';\nexport function bootstrap() {\n  console.log('[nexus] Bootstrapping sovereign cluster...');\n}\n", "Add core entrypoint", "zajferx", "main");
    this.saveFile(repo.id, "docs/SPEC.md", "# Specification\n\nEvery application in the ecosystem must be decoupled, independently startable, and gated by the Nexus Gate Proxy.\n", "Add initial specifications", "zajferx", "main");
  }

  // ── Repositories ──

  createRepo(name: string, description = "", defaultBranch = "main", visibility: "public" | "private" = "public"): Repository {
    const existing = this.getRepo(name);
    if (existing) return existing;

    const repo: Repository = {
      id: randomUUID(),
      name,
      description,
      defaultBranch,
      visibility,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare(
        "INSERT INTO repos VALUES (?,?,?,?,?,?,?)"
      )
      .run(repo.id, repo.name, repo.description, repo.defaultBranch, repo.visibility, repo.createdAt, repo.updatedAt);
    return repo;
  }

  listRepos(): Repository[] {
    return this.db
      .prepare("SELECT id, name, description, default_branch as defaultBranch, visibility, created_at as createdAt, updated_at as updatedAt FROM repos ORDER BY updated_at DESC")
      .all() as Repository[];
  }

  getRepo(idOrName: string): Repository | undefined {
    return this.db
      .prepare(
        "SELECT id, name, description, default_branch as defaultBranch, visibility, created_at as createdAt, updated_at as updatedAt FROM repos WHERE id = ? OR name = ?"
      )
      .get(idOrName, idOrName) as Repository | undefined;
  }

  // ── Files & Commits ──

  saveFile(
    repoId: string,
    path: string,
    content: string,
    commitMessage = "Update file",
    author = "operator",
    branch = "main"
  ): RepoFile {
    const size = Buffer.byteLength(content, "utf8");
    const now = new Date().toISOString();
    const id = randomUUID();

    // Upsert file
    this.db
      .prepare(
        `INSERT INTO files (id, repo_id, branch, path, type, content, size, updated_at)
         VALUES (?, ?, ?, ?, 'file', ?, ?, ?)
         ON CONFLICT(repo_id, branch, path) DO UPDATE SET content = excluded.content, size = excluded.size, updated_at = excluded.updated_at`
      )
      .run(id, repoId, branch, path, content, size, now);

    // Create commit
    const hash = createHash("sha1").update(`${repoId}:${branch}:${path}:${content}:${now}`).digest("hex");
    this.db
      .prepare(
        "INSERT INTO commits (id, repo_id, hash, branch, message, author, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
      )
      .run(randomUUID(), repoId, hash, branch, `${commitMessage} (${path})`, author, now);

    // Update repo timestamp
    this.db.prepare("UPDATE repos SET updated_at = ? WHERE id = ?").run(now, repoId);

    return {
      id,
      repoId,
      branch,
      path,
      type: "file",
      content,
      size,
      updatedAt: now,
    };
  }

  listFiles(repoId: string, branch = "main"): RepoFile[] {
    return this.db
      .prepare(
        "SELECT id, repo_id as repoId, branch, path, type, content, size, updated_at as updatedAt FROM files WHERE repo_id = ? AND branch = ? ORDER BY path ASC"
      )
      .all(repoId, branch) as RepoFile[];
  }

  getFile(repoId: string, path: string, branch = "main"): RepoFile | undefined {
    return this.db
      .prepare(
        "SELECT id, repo_id as repoId, branch, path, type, content, size, updated_at as updatedAt FROM files WHERE repo_id = ? AND path = ? AND branch = ?"
      )
      .get(repoId, path, branch) as RepoFile | undefined;
  }

  listCommits(repoId: string, branch = "main"): CommitRecord[] {
    return this.db
      .prepare(
        "SELECT id, repo_id as repoId, hash, branch, message, author, created_at as createdAt FROM commits WHERE repo_id = ? AND branch = ? ORDER BY created_at DESC"
      )
      .all(repoId, branch) as CommitRecord[];
  }

  searchCode(repoId: string, query: string, branch = "main"): Array<{ path: string; line: number; text: string }> {
    const files = this.listFiles(repoId, branch);
    const results: Array<{ path: string; line: number; text: string }> = [];
    const q = query.toLowerCase();

    for (const file of files) {
      if (file.path.toLowerCase().includes(q)) {
        results.push({ path: file.path, line: 1, text: `[Filename match: ${file.path}]` });
      }
      const lines = file.content.split("\n");
      for (let i = 0; i < lines.length; i++) {
        if (lines[i].toLowerCase().includes(q)) {
          results.push({
            path: file.path,
            line: i + 1,
            text: lines[i].trim(),
          });
        }
      }
    }
    return results;
  }

  // ── Snippets ──

  createSnippet(title: string, language: string, code: string, tags = ""): Snippet {
    const s: Snippet = {
      id: randomUUID(),
      title,
      language,
      code,
      tags,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.db
      .prepare("INSERT INTO snippets VALUES (?, ?, ?, ?, ?, ?, ?)")
      .run(s.id, s.title, s.language, s.code, s.tags, s.createdAt, s.updatedAt);
    return s;
  }

  listSnippets(): Snippet[] {
    return this.db
      .prepare("SELECT id, title, language, code, tags, created_at as createdAt, updated_at as updatedAt FROM snippets ORDER BY updated_at DESC")
      .all() as Snippet[];
  }

  getSnippet(id: string): Snippet | undefined {
    return this.db
      .prepare("SELECT id, title, language, code, tags, created_at as createdAt, updated_at as updatedAt FROM snippets WHERE id = ?")
      .get(id) as Snippet | undefined;
  }

  deleteSnippet(id: string): boolean {
    const r = this.db.prepare("DELETE FROM snippets WHERE id = ?").run(id);
    return r.changes > 0;
  }

  // ── Legacy Compatibility ──

  list(): any[] {
    return this.listRepos().map((r) => ({
      id: r.id,
      name: r.name,
      snippets: "",
      repos: r.name,
      files: "",
      properties: { description: r.description },
      createdAt: r.createdAt,
    }));
  }

  create(name: string, snippets = "", repos = "", files = ""): any {
    const repo = this.createRepo(name, snippets || repos);
    return {
      id: repo.id,
      name: repo.name,
      snippets,
      repos,
      files,
      properties: {},
      createdAt: repo.createdAt,
    };
  }

  get(id: string): any {
    const r = this.getRepo(id);
    if (!r) return undefined;
    return {
      id: r.id,
      name: r.name,
      snippets: "",
      repos: r.name,
      files: "",
      properties: { description: r.description },
      createdAt: r.createdAt,
    };
  }
}
