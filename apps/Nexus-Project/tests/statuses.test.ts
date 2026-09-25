import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createProject, teamWithRoles } from "./support/fixtures";
import { startTestServer } from "./support/server";

let t: Awaited<ReturnType<typeof startTestServer>>;
let ws: string;
beforeAll(async () => {
  t = await startTestServer();
  ws = await teamWithRoles(t);
});
afterAll(() => t.close());

async function statuses(projectId: string) {
  return (await t.as("usr-viewer").call("GET", `/projects/${projectId}/statuses`)).body
    .statuses as {
    id: string;
    name: string;
    position: number;
  }[];
}

describe("statuses", () => {
  it("are ordered by position and read by viewers", async () => {
    const project = await createProject(t.as("usr-owner"), ws);
    const list = await statuses(project.id);
    expect(list.map((s) => s.name)).toEqual(["Backlog", "Todo", "In Progress", "Done", "Canceled"]);
    expect(list.map((s) => s.position)).toEqual([0, 1, 2, 3, 4]);
  });

  it("are managed by admins only", async () => {
    const project = await createProject(t.as("usr-owner"), ws);
    const body = { name: "Review", category: "started" };
    expect(
      (await t.as("usr-member").call("POST", `/projects/${project.id}/statuses`, body)).status,
    ).toBe(403);
    const created = await t.as("usr-admin").call("POST", `/projects/${project.id}/statuses`, body);
    expect(created.status).toBe(201);
    expect(created.body).toMatchObject({ name: "Review", category: "started", position: 5 });
    const dup = await t.as("usr-admin").call("POST", `/projects/${project.id}/statuses`, body);
    expect(dup.status).toBe(409);
    expect(dup.body.error).toBe("status_name_taken");
  });

  it("reorders by moving one status and renumbering the rest", async () => {
    const project = await createProject(t.as("usr-owner"), ws);
    const done = (await statuses(project.id)).find((s) => s.name === "Done") as { id: string };
    const moved = await t
      .as("usr-admin")
      .call("PATCH", `/statuses/${done.id}`, { position: 0, name: "Shipped" });
    expect(moved.status).toBe(200);
    const list = await statuses(project.id);
    expect(list.map((s) => s.name)).toEqual([
      "Shipped",
      "Backlog",
      "Todo",
      "In Progress",
      "Canceled",
    ]);
    expect(list.map((s) => s.position)).toEqual([0, 1, 2, 3, 4]);
  });

  it("keeps at least one status", async () => {
    const project = await createProject(t.as("usr-owner"), ws);
    const list = await statuses(project.id);
    for (const status of list.slice(1)) {
      expect((await t.as("usr-admin").call("DELETE", `/statuses/${status.id}`)).status).toBe(200);
    }
    const last = await t
      .as("usr-admin")
      .call("DELETE", `/statuses/${(list[0] as { id: string }).id}`);
    expect(last.status).toBe(422);
    expect(last.body.error).toBe("last_status");
    expect((await statuses(project.id)).map((s) => s.position)).toEqual([0]);
  });

  it("refuses a moveTasksTo from another project", async () => {
    const a = await createProject(t.as("usr-owner"), ws);
    const b = await createProject(t.as("usr-owner"), ws);
    const target = (await statuses(b.id))[0] as { id: string };
    const source = (await statuses(a.id))[0] as { id: string };
    const res = await t
      .as("usr-admin")
      .call("DELETE", `/statuses/${source.id}?moveTasksTo=${target.id}`);
    expect(res.status).toBe(400);
  });

  it("validates its input and hides other workspaces' statuses", async () => {
    const project = await createProject(t.as("usr-owner"), ws);
    expect(
      (
        await t
          .as("usr-admin")
          .call("POST", `/projects/${project.id}/statuses`, { name: "X", category: "doing" })
      ).status,
    ).toBe(400);
    const id = ((await statuses(project.id))[0] as { id: string }).id;
    expect(
      (await t.as("usr-stranger").call("PATCH", `/statuses/${id}`, { name: "Mine" })).status,
    ).toBe(404);
    expect(
      (await t.as("usr-admin").call("PATCH", `/statuses/${id}`, { position: 99 })).status,
    ).toBe(400);
  });
});
