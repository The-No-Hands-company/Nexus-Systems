import { mock } from "bun:test";

// Cloud registration is fire-and-forget network traffic; tests never want it.
mock.module("../../src/cloud", () => ({ startHeartbeat: () => () => {} }));

export const TEST_SECRET = "project-test-hop-secret"; // pragma: allowlist secret

export interface ApiResponse<T = any> {
  status: number;
  body: T;
  headers: Headers;
}

export interface Client {
  call<T = any>(
    method: string,
    path: string,
    body?: unknown,
    headers?: Record<string, string>,
  ): Promise<ApiResponse<T>>;
}

/** Starts a server on a random port with a fresh in-memory database. */
export async function startTestServer() {
  process.env.NEXUS_PROJECT_DB = ":memory:";
  process.env.PORT = "0";
  process.env.NEXUS_PROJECT_DASHBOARD_SECRET = TEST_SECRET;
  const { createServer } = await import("../../src/server");
  const handle = await createServer();
  const base = `http://127.0.0.1:${handle.server.port}`;

  async function send(
    subject: string | null,
    method: string,
    path: string,
    body?: unknown,
    headers: Record<string, string> = {},
  ): Promise<ApiResponse> {
    const all: Record<string, string> = { ...headers };
    if (subject !== null) {
      all["x-nexus-subject"] = subject;
      all["x-nexus-dashboard-secret"] = TEST_SECRET;
    }
    if (body !== undefined) all["content-type"] = "application/json";
    const response = await fetch(`${base}${path}`, {
      method,
      headers: all,
      body: body === undefined ? null : JSON.stringify(body),
    });
    const text = await response.text();
    return { status: response.status, body: text ? JSON.parse(text) : null, headers: response.headers };
  }

  /** A client authenticated as `subject`; paths are relative to /api/v1/project. */
  const as = (subject: string): Client => ({
    call: (method, path, body, headers) => send(subject, method, `/api/v1/project${path}`, body, headers),
  });

  return { handle, base, as, raw: send, close: () => handle.close() };
}
