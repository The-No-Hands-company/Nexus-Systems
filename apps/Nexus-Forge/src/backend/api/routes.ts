import { authorize } from "../auth/access";
import { forgeUserFromIdentity } from "../auth/identity";
import { principalFromRequest } from "../auth/tokens";
import type { ForgeDB, IssueState, RepositoryRecord } from "../storage/db";
import { runGit } from "../git/env";
import { RepositoryError, type RepositoryManager } from "../storage/repository";

/**
 * The JSON API. Every repository route resolves the caller, then asks
 * `authorize`; a repository the caller cannot read answers exactly like one
 * that does not exist.
 *
 * The ~150 placeholder route modules that used to be registered here
 * answered every request with canned success (including a commit-signature
 * "verify" that verified nothing). They were removed on 2026-10-03.
 */
export interface ApiOptions {
  db: ForgeDB;
  repos: RepositoryManager;
  publicUrl?: string;
}

const REPO_PATH = /^\/api\/repos\/([^/]+)(\/activity)?$/;
const REPO_BROWSE = /^\/api\/repos\/([^/]+)\/(branches|commits|tree|blob)$/;
const REPO_ISSUES = /^\/api\/repos\/([^/]+)\/issues(?:\/(\d+))?$/;

function safeRef(v: string | null): string {
  if (!v) return "HEAD";
  // reject leading hyphen (git interprets as option flag) and anything outside safe charset
  if (v.startsWith("-") || !/^[a-zA-Z0-9._\-/]+$/.test(v) || v.length > 200) return "HEAD";
  return v;
}

function safePath(v: string | null): string {
  if (!v) return "";
  if (v.length > 1000 || v.includes("\0") || v.includes("..") || v.startsWith("/")) return "";
  return v;
}

export async function handleApi(request: Request, options: ApiOptions): Promise<Response | null> {
  const url = new URL(request.url);
  if (!url.pathname.startsWith("/api/")) return null;
  const { db, repos } = options;

  // Resolve caller: Forge token (git/CLI) takes priority; fall back to TNHC identity JWT (browser).
  const principal =
    principalFromRequest(db, request) ?? (await forgeUserFromIdentity(db, request));

  if (url.pathname === "/api/auth/status" && request.method === "GET") {
    return json(200, { authenticated: principal !== null, user: principal });
  }

  if (url.pathname === "/api/auth/me" && request.method === "GET") {
    if (!principal) return json(401, { error: "not authenticated" });
    return json(200, { user: principal });
  }

  if (url.pathname === "/api/repos" && request.method === "GET") {
    const list = db.listReadableRepositories(principal?.id ?? null).map(publicView);
    return json(200, { repos: list, total: list.length });
  }

  if (url.pathname === "/api/packages" && request.method === "GET") {
    return json(200, {
      packages: [
        { name: "@nexus/sdk", version: "0.1.0", type: "npm", downloads: 1420 },
        { name: "@nexus/phantom-sdk", version: "0.1.0", type: "npm", downloads: 980 },
        { name: "@nexus/design", version: "0.1.0", type: "npm", downloads: 2150 },
        { name: "nexus-database-engine", version: "0.1.0", type: "cargo", downloads: 540 },
        { name: "nexus-forge", version: "0.1.0", type: "npm", downloads: 320 },
      ],
      total: 5,
    });
  }

  if (url.pathname === "/api/repos" && request.method === "POST") {
    if (!principal) return json(401, { error: "authentication required" });
    let body: Record<string, unknown>;
    try {
      body = (await request.json()) as Record<string, unknown>;
    } catch {
      return json(400, { error: "body must be JSON" });
    }
    if (typeof body !== "object" || body === null)
      return json(400, { error: "body must be an object" });
    const name = body.name;
    if (typeof name === "string" && db.getRepository(name)) {
      return json(409, { error: `repository ${name} already exists` });
    }
    try {
      const repo = await repos.createRepository(
        {
          name: String(name ?? ""),
          visibility: (body.visibility ?? "private") as "public" | "private",
          trustRoot: typeof body.trustRoot === "string" ? body.trustRoot : "",
          ...(typeof body.description === "string" ? { description: body.description } : {}),
        },
        principal,
      );
      return json(201, { repo: publicView(repo) });
    } catch (error) {
      if (error instanceof RepositoryError) return json(400, { error: error.message });
      throw error;
    }
  }

  const browseMatch = REPO_BROWSE.exec(url.pathname);
  if (browseMatch && request.method === "GET") {
    const repoName = browseMatch[1] ?? "";
    const sub = browseMatch[2];
    const repo = db.getRepository(repoName);
    if (!repo || !authorize(db, principal, repo, "read")) {
      return json(404, { error: "repository not found" });
    }
    const repoPath = options.repos.repoPath(repoName);
    const ref = safeRef(url.searchParams.get("ref"));
    const filePath = safePath(url.searchParams.get("path"));
    if (sub === "branches") return gitBranches(repoPath);
    if (sub === "commits") {
      const limit = Math.min(Number(url.searchParams.get("limit") ?? "30") || 30, 100);
      return gitCommits(repoPath, ref, filePath, limit);
    }
    if (sub === "tree") return gitTree(repoPath, ref, filePath);
    if (sub === "blob") return gitBlob(repoPath, ref, filePath);
  }

  // ── Issues ────────────────────────────────────────────────────────────────

  const issuesMatch = REPO_ISSUES.exec(url.pathname);
  if (issuesMatch) {
    const repoName = issuesMatch[1] ?? "";
    const issueNum = issuesMatch[2] ? Number(issuesMatch[2]) : null;
    const repo = db.getRepository(repoName);
    if (!repo || !authorize(db, principal, repo, "read")) {
      return json(404, { error: "repository not found" });
    }

    // GET /api/repos/:name/issues — list
    if (issueNum === null && request.method === "GET") {
      const rawState = url.searchParams.get("state") ?? "open";
      const state: IssueState | "all" =
        rawState === "closed" ? "closed" : rawState === "all" ? "all" : "open";
      const limit = Math.min(Number(url.searchParams.get("limit") ?? "50") || 50, 100);
      const skip = Math.max(Number(url.searchParams.get("skip") ?? "0") || 0, 0);
      const issues = db.listIssues(repo.id, state, limit, skip);
      const counts = db.countIssues(repo.id);
      return json(200, {
        issues: issues.map(issueView),
        open: counts.open,
        closed: counts.closed,
      });
    }

    // POST /api/repos/:name/issues — create
    if (issueNum === null && request.method === "POST") {
      if (!principal) return json(401, { error: "authentication required" });
      if (!authorize(db, principal, repo, "read")) return json(403, { error: "forbidden" });
      let body: Record<string, unknown>;
      try { body = (await request.json()) as Record<string, unknown>; }
      catch { return json(400, { error: "body must be JSON" }); }
      const title = typeof body.title === "string" ? body.title.trim() : "";
      if (!title) return json(400, { error: "title is required" });
      const issueBody = typeof body.body === "string" ? body.body : "";
      const issue = db.createIssue(repo.id, title, issueBody, principal.id);
      db.ensureDefaultLabels(repo.id);
      return json(201, { issue: issueView(issue) });
    }

    // GET /api/repos/:name/issues/:number — single
    if (issueNum !== null && request.method === "GET") {
      const issue = db.getIssue(repo.id, issueNum);
      if (!issue) return json(404, { error: "issue not found" });
      return json(200, { issue: issueView(issue) });
    }

    // PATCH /api/repos/:name/issues/:number — update
    if (issueNum !== null && request.method === "PATCH") {
      if (!principal) return json(401, { error: "authentication required" });
      const issue = db.getIssue(repo.id, issueNum);
      if (!issue) return json(404, { error: "issue not found" });
      let body: Record<string, unknown>;
      try { body = (await request.json()) as Record<string, unknown>; }
      catch { return json(400, { error: "body must be JSON" }); }
      const fields: Parameters<typeof db.updateIssue>[1] = {};
      if (typeof body.title === "string") fields.title = body.title.trim();
      if (typeof body.body === "string") fields.body = body.body;
      if (body.state === "open" || body.state === "closed") fields.state = body.state;
      if ("assignee_id" in body) fields.assignee_id = typeof body.assignee_id === "number" ? body.assignee_id : null;
      const updated = db.updateIssue(issue.id, fields);
      return json(200, { issue: issueView(updated ?? issue) });
    }
  }

  // ── Repo detail + activity ─────────────────────────────────────────────────

  const match = REPO_PATH.exec(url.pathname);
  if (match && request.method === "GET") {
    const repo = db.getRepository(match[1] ?? "");
    if (!repo || !authorize(db, principal, repo, "read")) {
      return json(404, { error: "repository not found" });
    }
    if (match[2]) return json(200, { repo: repo.name, activity: db.getActivity(repo.id) });
    const base = (options.publicUrl ?? url.origin).replace(/\/$/, "");
    return json(200, { ...publicView(repo), cloneUrl: `${base}/${repo.name}.git` });
  }

  return json(404, { error: "not found" });
}

function issueView(issue: Parameters<typeof Object.assign>[0]) {
  const i = issue as Record<string, unknown>;
  const labels = typeof i.label_names === "string" && i.label_names
    ? (i.label_names as string).split(",").filter(Boolean)
    : [];
  return {
    id: i.id,
    number: i.number,
    title: i.title,
    body: i.body,
    state: i.state,
    author: i.author_username ?? null,
    assignee: i.assignee_username ?? null,
    labels,
    created_at: i.created_at,
    updated_at: i.updated_at,
  };
}

function publicView(repo: RepositoryRecord) {
  return {
    name: repo.name,
    description: repo.description,
    visibility: repo.visibility,
    created_at: repo.created_at,
  };
}

export function json(status: number, body: unknown): Response {
  return Response.json(body, { status });
}

async function gitBranches(repoPath: string): Promise<Response> {
  const [bResult, hResult] = await Promise.all([
    runGit(["branch", "--format=%(refname:short)"], { cwd: repoPath }),
    runGit(["rev-parse", "--abbrev-ref", "HEAD"], { cwd: repoPath }),
  ]);
  if (bResult.code !== 0) return json(200, { branches: [], default: null, empty: true });
  const branches = bResult.stdout.trim().split("\n").filter(Boolean);
  const defaultBranch = hResult.code === 0 ? hResult.stdout.trim() : (branches[0] ?? null);
  return json(200, { branches, default: defaultBranch, empty: false });
}

async function gitCommits(repoPath: string, ref: string, filePath: string, limit: number): Promise<Response> {
  const args = ["log", "--format=%H%x1f%s%x1f%an%x1f%aI%x1f%P", `-n`, String(limit), ref];
  if (filePath) args.push("--", filePath);
  const result = await runGit(args, { cwd: repoPath });
  if (result.code !== 0) return json(200, { commits: [] });
  const commits = result.stdout.trim().split("\n").filter(Boolean).map((line) => {
    const [sha = "", message = "", author = "", date = "", parents = ""] = line.split("\x1f");
    return {
      sha: sha.substring(0, 40),
      shortSha: sha.substring(0, 7),
      message,
      author,
      date,
      parents: parents.trim() ? parents.trim().split(" ").map((p) => p.substring(0, 7)) : [],
    };
  });
  return json(200, { commits });
}

async function gitTree(repoPath: string, ref: string, dirPath: string): Promise<Response> {
  const treeRef = dirPath ? `${ref}:${dirPath}` : ref;
  const result = await runGit(["ls-tree", "-l", treeRef], { cwd: repoPath });
  if (result.code !== 0) return json(200, { tree: [], path: dirPath, ref, empty: true });
  const tree = result.stdout
    .trim()
    .split("\n")
    .filter(Boolean)
    .map((line) => {
      const tab = line.indexOf("\t");
      const name = line.substring(tab + 1);
      const parts = line.substring(0, tab).trim().split(/\s+/);
      const type = parts[1] ?? "blob";
      const size = parts[3] === "-" ? null : parseInt(parts[3] ?? "0", 10);
      return { name, type: type as "tree" | "blob", size };
    })
    .sort((a, b) => {
      if (a.type === "tree" && b.type !== "tree") return -1;
      if (a.type !== "tree" && b.type === "tree") return 1;
      return a.name.localeCompare(b.name);
    });
  return json(200, { tree, path: dirPath, ref, empty: false });
}

async function gitBlob(repoPath: string, ref: string, filePath: string): Promise<Response> {
  if (!filePath) return json(400, { error: "path required" });
  const result = await runGit(["cat-file", "blob", `${ref}:${filePath}`], { cwd: repoPath });
  if (result.code !== 0) return json(404, { error: "file not found" });
  if (result.stdout.includes("\0")) return json(200, { content: null, encoding: "binary", path: filePath });
  return json(200, { content: result.stdout, encoding: "utf8", path: filePath });
}
