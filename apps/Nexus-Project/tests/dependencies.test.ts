import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import type { Task } from "../src/store/tasks";
import { createProject, teamWithRoles } from "./support/fixtures";
import { startTestServer } from "./support/server";

let t: Awaited<ReturnType<typeof startTestServer>>;
let ws: string;
beforeAll(async () => {
  t = await startTestServer();
  ws = await teamWithRoles(t);
});
afterAll(() => t.close());

const member = () => t.as("usr-member");

async function newTask(projectId: string, body: Record<string, unknown> = {}) {
  const res = await member().call("POST", `/projects/${projectId}/tasks`, {
    title: "Task",
    durationDays: 1,
    ...body,
  });
  expect(res.status).toBe(201);
  return res.body as { id: string; key: string };
}

async function linkTasks(
  projectId: string,
  predecessorId: string,
  successorId: string,
  extra: Record<string, unknown> = {},
) {
  return member().call("POST", `/projects/${projectId}/dependencies`, {
    predecessorId,
    successorId,
    ...extra,
  });
}

async function startOf(taskId: string): Promise<string | null> {
  return (await member().call<Task>("GET", `/tasks/${taskId}`)).body.startDate;
}

describe("dependencies", () => {
  it("links two tasks, defaulting to finish-to-start", async () => {
    const project = await createProject(member(), ws);
    const a = await newTask(project.id);
    const b = await newTask(project.id);
    const res = await linkTasks(project.id, a.id, b.id);
    expect(res.status).toBe(201);
    expect(res.body).toMatchObject({
      predecessorId: a.id,
      successorId: b.id,
      type: "FS",
      lagDays: 0,
    });
    const list = await t.as("usr-viewer").call("GET", `/projects/${project.id}/dependencies`);
    expect(list.body.dependencies).toHaveLength(1);
    const viewerTry = await t
      .as("usr-viewer")
      .call("POST", `/projects/${project.id}/dependencies`, {
        predecessorId: b.id,
        successorId: a.id,
      });
    expect(viewerTry.status).toBe(403);
  });

  it("refuses self links, summary links and duplicates", async () => {
    const project = await createProject(member(), ws);
    const a = await newTask(project.id);
    const b = await newTask(project.id);
    const parent = await newTask(project.id, { durationDays: null });
    await newTask(project.id, { parentId: parent.id });
    expect((await linkTasks(project.id, a.id, a.id)).body.error).toBe("self_link");
    expect((await linkTasks(project.id, a.id, parent.id)).body.error).toBe("summary_link");
    expect((await linkTasks(project.id, a.id, b.id)).status).toBe(201);
    const dup = await linkTasks(project.id, a.id, b.id, { type: "SS" });
    expect(dup.status).toBe(409);
    expect(dup.body.error).toBe("duplicate_link");
  });

  it("refuses links across projects, and hides tasks the caller cannot see", async () => {
    const project = await createProject(member(), ws);
    const other = await createProject(member(), ws);
    const hidden = await createProject(t.as("usr-admin"), ws, { visibility: "restricted" });
    const a = await newTask(project.id);
    const foreign = await newTask(other.id);
    const secret = await t
      .as("usr-admin")
      .call<Task>("POST", `/projects/${hidden.id}/tasks`, { title: "Secret" });
    expect((await linkTasks(project.id, a.id, foreign.id)).body.error).toBe("cross_project_link");
    expect((await linkTasks(project.id, a.id, secret.body.id)).status).toBe(404);
  });

  it("names the cycle it refuses", async () => {
    const project = await createProject(member(), ws, { key: "CYC" });
    const a = await newTask(project.id);
    const b = await newTask(project.id);
    const c = await newTask(project.id);
    await linkTasks(project.id, a.id, b.id);
    await linkTasks(project.id, b.id, c.id);
    const res = await linkTasks(project.id, c.id, a.id);
    expect(res.status).toBe(422);
    expect(res.body.error).toBe("dependency_cycle");
    expect(res.body.cycle).toEqual(["CYC-1", "CYC-2", "CYC-3", "CYC-1"]);
  });

  it("stops a linked task from becoming a summary", async () => {
    const project = await createProject(member(), ws);
    const a = await newTask(project.id);
    const b = await newTask(project.id);
    await linkTasks(project.id, a.id, b.id);
    const res = await member().call("POST", `/projects/${project.id}/tasks`, {
      title: "Child",
      parentId: a.id,
    });
    expect(res.body.error).toBe("summary_link");
  });

  it("moves successors in auto mode and restores them when the link goes", async () => {
    const project = await createProject(member(), ws, { scheduleMode: "auto" });
    const a = await newTask(project.id, { durationDays: 3 });
    const b = await newTask(project.id, { durationDays: 2 });
    expect(await startOf(b.id)).toBe("2026-09-07");
    const link = (await linkTasks(project.id, a.id, b.id)).body;
    expect(await startOf(b.id)).toBe("2026-09-10");
    expect((await member().call("PATCH", `/dependencies/${link.id}`, { lagDays: 2 })).status).toBe(
      200,
    );
    expect(await startOf(b.id)).toBe("2026-09-14");
    expect((await member().call("DELETE", `/dependencies/${link.id}`)).status).toBe(200);
    expect(await startOf(b.id)).toBe("2026-09-07");
  });

  it("disappears with a deleted task", async () => {
    const project = await createProject(member(), ws);
    const a = await newTask(project.id);
    const b = await newTask(project.id);
    await linkTasks(project.id, a.id, b.id);
    await member().call("DELETE", `/tasks/${a.id}`);
    expect(
      (await member().call("GET", `/projects/${project.id}/dependencies`)).body.dependencies,
    ).toEqual([]);
  });

  it("validates its input", async () => {
    const project = await createProject(member(), ws);
    const a = await newTask(project.id);
    const b = await newTask(project.id);
    expect((await linkTasks(project.id, a.id, b.id, { type: "XX" })).status).toBe(400);
    expect((await linkTasks(project.id, a.id, b.id, { lagDays: 99_999 })).status).toBe(400);
    expect((await linkTasks(project.id, a.id, b.id, { weight: 1 })).status).toBe(400);
    expect(
      (await member().call("POST", `/projects/${project.id}/dependencies`, { predecessorId: a.id }))
        .status,
    ).toBe(400);
  });
});
