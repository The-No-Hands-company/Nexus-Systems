import path from "node:path";
import { type Action, authorize } from "../auth/access";
import { principalFromRequest } from "../auth/tokens";
import type { ForgeDB, UserRecord } from "../storage/db";
import { REF_LOG_FILE, type RepositoryManager } from "../storage/repository";
import { gitEnv } from "./env";
import { hooksExecutable } from "./hooks";
import { isValidRepoName } from "./names";

export interface SmartHttpOptions {
  db: ForgeDB;
  repos: RepositoryManager;
  /** Directory holding the forge's pre-receive / post-receive hooks. */
  hooksDir: string;
  /** Largest request body accepted (a push pack), in bytes. */
  maxBodyBytes: number;
}

type Service = "git-upload-pack" | "git-receive-pack";

type Endpoint =
  | { kind: "git"; service: Service; action: Action }
  | { kind: "ref-log"; action: "read" };

const ACTION: Record<Service, Action> = {
  "git-upload-pack": "read",
  "git-receive-pack": "write",
};

const GIT_PATH = /^\/([^/]+)\.git\/(.+)$/;

/**
 * Git's smart HTTP protocol, handed to `git http-backend`, plus the
 * forge's ref log.
 *
 * The forge does not parse packfiles; git does. What the forge owns is the
 * decision of whether the request may reach git at all, so this is an
 * allowlist of endpoints, each mapped to the action it needs:
 *
 *   GET  /<repo>.git/info/refs?service=git-upload-pack   read
 *   POST /<repo>.git/git-upload-pack                     read
 *   GET  /<repo>.git/info/refs?service=git-receive-pack  write
 *   POST /<repo>.git/git-receive-pack                    write
 *   GET  /<repo>.git/nexus/ref-log                       read
 *
 * Everything else under `/<repo>.git/`, including the whole dumb protocol
 * (HEAD, objects/, config), is 404. Returns `null` for paths that are not
 * git paths at all so the caller can route them elsewhere.
 */
export async function handleSmartHttp(
  request: Request,
  options: SmartHttpOptions,
): Promise<Response | null> {
  const url = new URL(request.url);
  const match = GIT_PATH.exec(url.pathname);
  if (!match) return null;
  const [, name = "", tail = ""] = match;
  if (!isValidRepoName(name)) return notFound();

  const endpoint = resolveEndpoint(request.method, tail, url.searchParams);
  if (endpoint instanceof Response) return endpoint;

  const principal = principalFromRequest(options.db, request);
  const repo = options.db.getRepository(name);
  if (!repo || !authorize(options.db, principal, repo, endpoint.action)) {
    if (!principal) {
      return new Response("authentication required\n", {
        status: 401,
        headers: { "www-authenticate": 'Basic realm="Nexus Forge"', "content-type": "text/plain" },
      });
    }
    if (repo && authorize(options.db, principal, repo, "read")) {
      return text(403, "write access required\n");
    }
    return notFound();
  }

  if (endpoint.kind === "ref-log") {
    const file = Bun.file(options.repos.metaPath(name, REF_LOG_FILE));
    const body = (await file.exists()) ? await file.text() : "";
    return new Response(body, {
      headers: { "content-type": "application/x-ndjson", "cache-control": "no-store" },
    });
  }

  // git skips a hook it cannot execute and carries on with the push, so a
  // hook that stopped being runnable after startup must stop pushes here.
  if (endpoint.service === "git-receive-pack" && !hooksExecutable(options.hooksDir)) {
    console.error(`[forge] hooks in ${options.hooksDir} are not executable; refusing push`);
    return text(503, "push policy unavailable; push refused\n");
  }

  let body: Uint8Array | null = null;
  if (request.method === "POST") {
    body = await readLimited(request, options.maxBodyBytes);
    if (!body) return text(413, "request body too large\n");
  }

  return runHttpBackend({
    request,
    name,
    tail,
    service: endpoint.service,
    canWrite: endpoint.action === "write",
    principal,
    body,
    options,
  });
}

function resolveEndpoint(
  method: string,
  tail: string,
  query: URLSearchParams,
): Endpoint | Response {
  if (tail === "nexus/ref-log" && method === "GET" && query.size === 0) {
    return { kind: "ref-log", action: "read" };
  }
  if (tail === "info/refs" && method === "GET") {
    const requested = query.getAll("service");
    if (requested.length > 1) return text(400, "exactly one service parameter is allowed\n");
    const only = requested[0];
    if (only !== "git-upload-pack" && only !== "git-receive-pack") return notFound();
    return { kind: "git", service: only, action: ACTION[only] };
  }
  if ((tail === "git-upload-pack" || tail === "git-receive-pack") && method === "POST") {
    return { kind: "git", service: tail, action: ACTION[tail] };
  }
  return notFound();
}

async function runHttpBackend(args: {
  request: Request;
  name: string;
  tail: string;
  service: Service;
  canWrite: boolean;
  principal: UserRecord | null;
  body: Uint8Array | null;
  options: SmartHttpOptions;
}): Promise<Response> {
  const { request, name, tail, service, canWrite, principal, body, options } = args;
  const cgi: Record<string, string> = {
    GIT_PROJECT_ROOT: path.resolve(options.repos.storageRoot),
    GIT_HTTP_EXPORT_ALL: "1",
    PATH_INFO: `/${name}.git/${tail}`,
    REQUEST_METHOD: request.method,
    // Rebuilt rather than forwarded: http-backend must see the one service
    // that was authorised, whatever else the client put in the query.
    QUERY_STRING: tail === "info/refs" ? `service=${service}` : "",
    CONTENT_TYPE: request.headers.get("content-type") ?? "",
    NEXUS_FORGE_REPO: name,
    NEXUS_FORGE_PUSHER: principal?.username ?? "",
    NEXUS_FORGE_BUN: process.execPath,
    NEXUS_FORGE_META: options.repos.metaPath(name, ""),
  };
  if (body) cgi.CONTENT_LENGTH = String(body.byteLength);
  if (principal) cgi.REMOTE_USER = principal.username;
  const encoding = request.headers.get("content-encoding");
  if (encoding === "gzip") cgi.HTTP_CONTENT_ENCODING = "gzip";
  const protocol = request.headers.get("git-protocol");
  if (protocol && /^[A-Za-z0-9=:._-]{1,64}$/.test(protocol)) cgi.GIT_PROTOCOL = protocol;

  const config = {
    "core.hooksPath": options.hooksDir,
    "http.uploadpack": "true",
    "http.receivepack": canWrite ? "true" : "false",
    "http.getanyfile": "false",
    "receive.fsckObjects": "true",
    "receive.denyNonFastForwards": "true",
    "receive.denyDeletes": "true",
  };

  const proc = Bun.spawn(["git", "http-backend"], {
    env: gitEnv(config, cgi),
    stdin: body ?? "ignore",
    stdout: "pipe",
    stderr: "pipe",
  });
  new Response(proc.stderr).text().then((stderr) => {
    if (stderr.trim()) console.warn(`[forge] http-backend ${name}: ${stderr.trim()}`);
  });
  return cgiResponse(proc.stdout);
}

/** Split CGI output into a status, headers and a streamed body. */
async function cgiResponse(stdout: ReadableStream<Uint8Array>): Promise<Response> {
  const reader = stdout.getReader();
  let buffered: Uint8Array<ArrayBuffer> = new Uint8Array(0);
  let split = -1;
  let sepLength = 0;
  while (split < 0) {
    const { value, done } = await reader.read();
    if (done) break;
    buffered = concat(buffered, value);
    [split, sepLength] = findHeaderEnd(buffered);
  }
  if (split < 0) return text(502, "git backend produced no response\n");

  const head = new TextDecoder().decode(buffered.subarray(0, split));
  const rest = buffered.subarray(split + sepLength);
  const headers = new Headers();
  let status = 200;
  for (const line of head.split(/\r?\n/)) {
    const colon = line.indexOf(":");
    if (colon < 0) continue;
    const key = line.slice(0, colon).trim();
    const value = line.slice(colon + 1).trim();
    if (key.toLowerCase() === "status") status = Number.parseInt(value, 10) || 500;
    else headers.append(key, value);
  }
  const body = new ReadableStream<Uint8Array>({
    start(controller) {
      if (rest.byteLength > 0) controller.enqueue(rest);
    },
    async pull(controller) {
      const { value, done } = await reader.read();
      if (done) controller.close();
      else controller.enqueue(value);
    },
    cancel() {
      reader.cancel();
    },
  });
  return new Response(body, { status, headers });
}

function findHeaderEnd(bytes: Uint8Array): [number, number] {
  for (let i = 0; i < bytes.length - 1; i++) {
    if (bytes[i] === 10 && bytes[i + 1] === 10) return [i, 2];
    if (bytes[i] === 13 && bytes[i + 1] === 10 && bytes[i + 2] === 13 && bytes[i + 3] === 10) {
      return [i, 4];
    }
  }
  return [-1, 0];
}

function concat(a: Uint8Array, b: Uint8Array): Uint8Array<ArrayBuffer> {
  const out = new Uint8Array(a.byteLength + b.byteLength);
  out.set(a, 0);
  out.set(b, a.byteLength);
  return out;
}

async function readLimited(request: Request, max: number): Promise<Uint8Array | null> {
  const declared = Number(request.headers.get("content-length") ?? "0");
  if (declared > max) return null;
  if (!request.body) return new Uint8Array(0);
  const chunks: Uint8Array[] = [];
  let total = 0;
  const reader = request.body.getReader();
  for (;;) {
    const { value: chunk, done } = await reader.read();
    if (done) break;
    total += chunk.byteLength;
    if (total > max) {
      await reader.cancel();
      return null;
    }
    chunks.push(chunk);
  }
  const out = new Uint8Array(total);
  let offset = 0;
  for (const chunk of chunks) {
    out.set(chunk, offset);
    offset += chunk.byteLength;
  }
  return out;
}

function text(status: number, body: string): Response {
  return new Response(body, { status, headers: { "content-type": "text/plain" } });
}

function notFound(): Response {
  return text(404, "not found\n");
}
