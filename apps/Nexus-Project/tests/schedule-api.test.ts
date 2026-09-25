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

const member = () => t.as("usr-member");
const post = async (projectId: string, body: Record<string, unknown>) => {
  const res = await member().call("POST", `/projects/${projectId}/tasks`, body);
  expect(res.status).toBe(201);
  return res.body;
};

describe("GET /projects/:id/schedule", () => {
  it("reports dates, the critical path, violations, roll-ups and warnings", async () => {
    const project = await createProject(member(), ws);
    const phase = await post(project.id, { title: "Phase" });
    const a = await post(project.id, {
      title: "A",
      parentId: phase.id,
      startDate: "2026-09-07",
      durationDays: 3,
    });
    const b = await post(project.id, {
      title: "B",
      parentId: phase.id,
      startDate: "2026-09-08",
      durationDays: 2,
    });
    const u = await post(project.id, { title: "Unestimated" });
    const ab = (
      await member().call("POST", `/projects/${project.id}/dependencies`, {
        predecessorId: a.id,
        successorId: b.id,
      })
    ).body;
    const bu = (
      await member().call("POST", `/projects/${project.id}/dependencies`, {
        predecessorId: b.id,
        successorId: u.id,
      })
    ).body;

    const res = await t.as("usr-viewer").call("GET", `/projects/${project.id}/schedule`);
    expect(res.status).toBe(200);
    expect(res.body.projectFinish).toBe("2026-09-11");
    expect(res.body.criticalPath).toEqual([a.id, b.id]);
    expect(res.body.violations).toEqual([
      { linkId: ab.id, predecessorId: a.id, successorId: b.id, type: "FS", gapDays: 2 },
    ]);
    expect(res.body.summaries).toEqual([
      { id: phase.id, start: "2026-09-07", finish: "2026-09-11", progress: 0 },
    ]);
    expect(res.body.warnings).toEqual([
      { code: "link_ignored", linkId: bu.id, taskId: u.id, reason: "unestimated" },
    ]);
    // Manual mode reports; it does not move the stored date.
    expect((await member().call("GET", `/tasks/${b.id}`)).body.startDate).toBe("2026-09-08");
  });

  it("is hidden from strangers", async () => {
    const project = await createProject(member(), ws);
    expect(
      (await t.as("usr-stranger").call("GET", `/projects/${project.id}/schedule`)).status,
    ).toBe(404);
  });
});

describe("GET /me/tasks", () => {
  it("lists open tasks assigned to me in projects I can see", async () => {
    const one = await createProject(member(), ws, { key: "ONE" });
    const two = await createProject(member(), ws, { key: "TWO" });
    const archived = await createProject(member(), ws, { key: "OLD" });
    const hidden = await createProject(t.as("usr-admin"), ws, {
      key: "HID",
      visibility: "restricted",
    });
    const statuses = (await member().call("GET", `/projects/${one.id}/statuses`)).body.statuses;
    const done = statuses.find((s: { name: string }) => s.name === "Done").id;

    await post(one.id, { title: "Later", assigneeSubject: "usr-member", deadline: "2026-10-01" });
    await post(two.id, { title: "Sooner", assigneeSubject: "usr-member", deadline: "2026-09-15" });
    await post(one.id, { title: "Whenever", assigneeSubject: "usr-member" });
    await post(one.id, { title: "Finished", assigneeSubject: "usr-member", statusId: done });
    await post(one.id, { title: "Someone else's", assigneeSubject: "usr-admin" });
    await post(archived.id, { title: "Archived", assigneeSubject: "usr-member" });
    await t.as("usr-admin").call("PATCH", `/projects/${archived.id}`, { archived: true });
    await t
      .as("usr-admin")
      .call("POST", `/projects/${hidden.id}/tasks`, {
        title: "Hidden",
        assigneeSubject: "usr-member",
      });

    const res = await member().call("GET", "/me/tasks");
    expect(res.status).toBe(200);
    expect(
      res.body.tasks.map((task: { title: string; projectKey: string }) => [
        task.projectKey,
        task.title,
      ]),
    ).toEqual([
      ["TWO", "Sooner"],
      ["ONE", "Later"],
      ["ONE", "Whenever"],
    ]);
  });
});
