import { authorize } from "../auth/access";
import { principalFromRequest } from "../auth/tokens";
import type { ForgeDB, RepositoryRecord } from "../storage/db";
import { RepositoryError, type RepositoryManager } from "../storage/repository";

/**
 * The JSON API. Every repository route resolves the caller, then asks
 * `authorize`; a repository the caller cannot read answers exactly like one
 * that does not exist.
 *
 * The ~150 placeholder route modules that used to be registered here
 * answered every request with canned success (including a commit-signature
 * "verify" that verified nothing). They are no longer reachable.
 */
export interface ApiOptions {
  db: ForgeDB;
  repos: RepositoryManager;
  publicUrl?: string;
}

const REPO_PATH = /^\/api\/repos\/([^/]+)(\/activity)?$/;

export async function handleApi(request: Request, options: ApiOptions): Promise<Response | null> {
  const url = new URL(request.url);
  if (!url.pathname.startsWith("/api/")) return null;
  const { db, repos } = options;
  const principal = principalFromRequest(db, request);

  if (url.pathname === "/api/auth/status" && request.method === "GET") {
    return json(200, { authenticated: principal !== null, user: principal });
  }

  if (url.pathname === "/api/repos" && request.method === "GET") {
    const list = db.listReadableRepositories(principal?.id ?? null).map(publicView);
    return json(200, { repos: list, total: list.length });
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
