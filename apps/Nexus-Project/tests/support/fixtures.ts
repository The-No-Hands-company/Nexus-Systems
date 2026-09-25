import { expect } from "bun:test";
import type { Project } from "../../src/store/projects";
import type { Client, startTestServer } from "./server";

type Server = Awaited<ReturnType<typeof startTestServer>>;

/** A team workspace owned by usr-owner with one member per other role. */
export async function teamWithRoles(t: Server, name = "Studio"): Promise<string> {
  const created = await t.as("usr-owner").call("POST", "/workspaces", { name });
  expect(created.status).toBe(201);
  const id = created.body.id as string;
  for (const [subject, role] of [
    ["usr-admin", "admin"],
    ["usr-member", "member"],
    ["usr-viewer", "viewer"],
  ] as const) {
    const res = await t
      .as("usr-owner")
      .call("PUT", `/workspaces/${id}/members/${subject}`, { role });
    expect(res.status).toBe(200);
  }
  return id;
}

/** A value the test knows is present; a clear failure instead of a TypeError if not. */
export function must<T>(value: T | null | undefined, what = "value"): T {
  if (value === null || value === undefined) throw new Error(`expected ${what} to be present`);
  return value;
}

let keyCounter = 0;

/** Creates a project and returns its body. Keys are unique per call. */
export async function createProject(
  client: Client,
  workspaceId: string,
  overrides: Record<string, unknown> = {},
) {
  keyCounter += 1;
  const res = await client.call<Project>("POST", `/workspaces/${workspaceId}/projects`, {
    key: `P${keyCounter}`,
    name: `Project ${keyCounter}`,
    startDate: "2026-09-07",
    ...overrides,
  });
  expect(res.status).toBe(201);
  return res.body;
}
