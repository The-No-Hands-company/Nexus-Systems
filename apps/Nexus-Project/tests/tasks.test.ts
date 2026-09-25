import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createProject, teamWithRoles } from "./support/fixtures";
import { type Client, startTestServer } from "./support/server";

let t: Awaited<ReturnType<typeof startTestServer>>;
let ws: string;
beforeAll(async () => {
  t = await startTestServer();
  ws = await teamWithRoles(t);
});
afterAll(() => t.close());

const member = () => t.as("usr-member");

async function newTask(client: Client, projectId: string, body: Record<string, unknown> = {}) {
  const res = await client.call("POST", `/projects/${projectId}/tasks`, { title: "Task", ...body });
  expect(res.status).toBe(201);
  return res.body;
}

async function patch(id: string, version: number, body: Record<string, unknown>) {
  return member().call("PATCH", `/tasks/${id}`, body, { "if-match": String(version) });
}

async function titles(projectId: string, query = "") {
  const res = await member().call("GET", `/projects/${projectId}/tasks${query}`);
  return res.body.tasks.map((task: { title: string }) => task.title);
}

describe("creating tasks", () => {
  it("numbers them per project and files them under Todo", async () => {
    const project = await createProject(member(), ws, { key: "APP" });
    const first = await newTask(member(), project.id, { title: "First" });
    const second = await newTask(member(), project.id, { title: "Second" });
    const statuses = (await member().call("GET", `/projects/${project.id}/statuses`)).body.statuses;
    expect(first).toMatchObject({ key: "APP-1", number: 1, version: 1, priority: "none", kind: "task", createdBy: "usr-member" });
    expect(second.key).toBe("APP-2");
    expect(first.statusId).toBe(statuses.find((s: { name: string }) => s.name === "Todo").id);
    expect(second.rank > first.rank).toBe(true);
  });

  it("is refused to viewers and invisible to strangers", async () => {
    const project = await createProject(member(), ws);
    expect((await t.as("usr-viewer").call("POST", `/projects/${project.id}/tasks`, { title: "x" })).status).toBe(403);
    expect((await t.as("usr-stranger").call("GET", `/projects/${project.id}/tasks`)).status).toBe(404);
  });

  it("validates its input", async () => {
    const project = await createProject(member(), ws);
    const post = (body: unknown) => member().call("POST", `/projects/${project.id}/tasks`, body);
    expect((await post({})).status).toBe(400);
    expect((await post({ title: "x".repeat(513) })).status).toBe(400);
    expect((await post({ title: "x", color: "red" })).status).toBe(400);
    expect((await post({ title: "x", progress: 101 })).status).toBe(400);
    const other = await createProject(member(), ws);
    const foreignStatus = (await member().call("GET", `/projects/${other.id}/statuses`)).body.statuses[0].id;
    expect((await post({ title: "x", statusId: foreignStatus })).status).toBe(400);
    const outsider = await post({ title: "x", assigneeSubject: "usr-stranger" });
    expect(outsider.status).toBe(422);
    expect(outsider.body.error).toBe("assignee_not_member");
  });
});

describe("dates", () => {
  it("moves a weekend start to Monday and derives the finish", async () => {
    const project = await createProject(member(), ws);
    const task = await newTask(member(), project.id, { startDate: "2026-09-05", durationDays: 3 });
    expect(task).toMatchObject({ startDate: "2026-09-07", durationDays: 3, finishDate: "2026-09-09" });
  });

  it("turns a finish date into a working-day duration", async () => {
    const project = await createProject(member(), ws);
    const task = await newTask(member(), project.id, { startDate: "2026-09-07", finishDate: "2026-09-14" });
    expect(task).toMatchObject({ durationDays: 6, finishDate: "2026-09-14" });
    const backwards = await member().call("POST", `/projects/${project.id}/tasks`, {
      title: "x",
      startDate: "2026-09-09",
      finishDate: "2026-09-07",
    });
    expect(backwards.body.error).toBe("invalid_dates");
    expect((await member().call("POST", `/projects/${project.id}/tasks`, { title: "x", finishDate: "2026-09-07" })).status).toBe(400);
  });

  it("keeps tasks at least a day long and milestones at none", async () => {
    const project = await createProject(member(), ws);
    const zero = await member().call("POST", `/projects/${project.id}/tasks`, { title: "x", durationDays: 0 });
    expect(zero.body.error).toBe("zero_duration");
    const long = await member().call("POST", `/projects/${project.id}/tasks`, { title: "x", kind: "milestone", durationDays: 2 });
    expect(long.body.error).toBe("milestone_duration");
    const milestone = await newTask(member(), project.id, { kind: "milestone", startDate: "2026-09-08" });
    expect(milestone).toMatchObject({ durationDays: 0, startDate: "2026-09-08", finishDate: "2026-09-08" });
    const back = await patch(milestone.id, milestone.version, { kind: "task" });
    expect(back.body).toMatchObject({ kind: "task", durationDays: null, finishDate: null });
  });

  it("requires a date for start-no-earlier-than and clears it for asap", async () => {
    const project = await createProject(member(), ws);
    expect(
      (await member().call("POST", `/projects/${project.id}/tasks`, { title: "x", constraintType: "start_no_earlier_than" })).status,
    ).toBe(400);
    const task = await newTask(member(), project.id, { constraintType: "start_no_earlier_than", constraintDate: "2026-09-10" });
    const cleared = await patch(task.id, task.version, { constraintType: "asap" });
    expect(cleared.body).toMatchObject({ constraintType: "asap", constraintDate: null });
  });

  it("turns a requested start into a constraint in auto mode", async () => {
    const project = await createProject(t.as("usr-admin"), ws, { scheduleMode: "auto" });
    const task = await newTask(member(), project.id, { startDate: "2026-09-09", durationDays: 2 });
    expect(task).toMatchObject({
      constraintType: "start_no_earlier_than",
      constraintDate: "2026-09-09",
      startDate: "2026-09-09",
      finishDate: "2026-09-10",
    });
  });
});

describe("work breakdown structure", () => {
  it("turns a parent into a summary whose schedule fields are derived", async () => {
    const project = await createProject(member(), ws);
    const phase = await newTask(member(), project.id, { title: "Phase", durationDays: 5 });
    await newTask(member(), project.id, { title: "Child", parentId: phase.id });
    const res = await patch(phase.id, phase.version, { durationDays: 2 });
    expect(res.status).toBe(422);
    expect(res.body.error).toBe("summary_fields");
    expect((await patch(phase.id, phase.version, { title: "Phase 1" })).status).toBe(200);
  });

  it("refuses parents from elsewhere, milestones, and cycles", async () => {
    const project = await createProject(member(), ws);
    const other = await createProject(member(), ws);
    const foreign = await newTask(member(), other.id);
    const milestone = await newTask(member(), project.id, { kind: "milestone" });
    const post = (body: Record<string, unknown>) => member().call("POST", `/projects/${project.id}/tasks`, { title: "x", ...body });
    expect((await post({ parentId: foreign.id })).body.error).toBe("invalid_parent");
    expect((await post({ parentId: milestone.id })).body.error).toBe("milestone_children");

    const parent = await newTask(member(), project.id, { title: "Parent" });
    const child = await newTask(member(), project.id, { title: "Child", parentId: parent.id });
    const grandchild = await newTask(member(), project.id, { title: "Grandchild", parentId: child.id });
    const current = (await member().call("GET", `/tasks/${parent.id}`)).body;
    const cycle = await member().call("POST", `/tasks/${parent.id}/move`, { parentId: grandchild.id }, { "if-match": String(current.version) });
    expect(cycle.body.error).toBe("wbs_cycle");
  });

  it("deletes a whole subtree", async () => {
    const project = await createProject(member(), ws);
    const parent = await newTask(member(), project.id, { title: "Parent" });
    const child = await newTask(member(), project.id, { title: "Child", parentId: parent.id });
    expect((await member().call("DELETE", `/tasks/${parent.id}`)).status).toBe(200);
    expect((await member().call("GET", `/tasks/${child.id}`)).status).toBe(404);
  });
});

describe("versions", () => {
  it("requires If-Match and refuses a stale version with the current task", async () => {
    const project = await createProject(member(), ws);
    const task = await newTask(member(), project.id);
    expect((await member().call("PATCH", `/tasks/${task.id}`, { title: "x" })).status).toBe(428);
    const first = await patch(task.id, task.version, { title: "Mine" });
    expect(first.status).toBe(200);
    expect(first.body.version).toBe(task.version + 1);
    const stale = await patch(task.id, task.version, { title: "Theirs" });
    expect(stale.status).toBe(409);
    expect(stale.body.error).toBe("version_conflict");
    expect(stale.body.task).toMatchObject({ title: "Mine", version: task.version + 1 });
  });
});

describe("listing and moving", () => {
  it("filters by status, assignee and parent", async () => {
    const project = await createProject(member(), ws);
    const statuses = (await member().call("GET", `/projects/${project.id}/statuses`)).body.statuses;
    const doing = statuses.find((s: { name: string }) => s.name === "In Progress").id;
    const parent = await newTask(member(), project.id, { title: "Parent" });
    await newTask(member(), project.id, { title: "Mine", assigneeSubject: "usr-member", statusId: doing });
    await newTask(member(), project.id, { title: "Child", parentId: parent.id });
    expect(await titles(project.id, `?status=${doing}`)).toEqual(["Mine"]);
    expect(await titles(project.id, "?assignee=usr-member")).toEqual(["Mine"]);
    expect(await titles(project.id, "?assignee=none")).toEqual(["Parent", "Child"]);
    expect(await titles(project.id, "?parent=root")).toEqual(["Parent", "Mine"]);
    expect(await titles(project.id, `?parent=${parent.id}`)).toEqual(["Child"]);
  });

  it("reorders, reparents and changes status", async () => {
    const project = await createProject(member(), ws);
    const a = await newTask(member(), project.id, { title: "A" });
    const b = await newTask(member(), project.id, { title: "B" });
    const c = await newTask(member(), project.id, { title: "C" });
    const move = async (id: string, body: Record<string, unknown>) => {
      const current = (await member().call("GET", `/tasks/${id}`)).body;
      return member().call("POST", `/tasks/${id}/move`, body, { "if-match": String(current.version) });
    };
    expect((await move(c.id, { afterId: null })).status).toBe(200);
    expect(await titles(project.id)).toEqual(["C", "A", "B"]);
    await move(c.id, { afterId: a.id });
    expect(await titles(project.id)).toEqual(["A", "C", "B"]);
    const done = (await member().call("GET", `/projects/${project.id}/statuses`)).body.statuses.find(
      (s: { name: string }) => s.name === "Done",
    ).id;
    const moved = await move(b.id, { parentId: a.id, statusId: done });
    expect(moved.body).toMatchObject({ parentId: a.id, statusId: done });
    expect((await move(b.id, { afterId: b.id })).body.error).toBe("invalid_after");
  });

  it("deleting a status in use needs somewhere to put its tasks", async () => {
    const project = await createProject(member(), ws);
    const statuses = (await member().call("GET", `/projects/${project.id}/statuses`)).body.statuses;
    const todo = statuses.find((s: { name: string }) => s.name === "Todo").id;
    const backlog = statuses.find((s: { name: string }) => s.name === "Backlog").id;
    const task = await newTask(member(), project.id);
    const refused = await t.as("usr-admin").call("DELETE", `/statuses/${todo}`);
    expect(refused.body.error).toBe("status_in_use");
    expect((await t.as("usr-admin").call("DELETE", `/statuses/${todo}?moveTasksTo=${backlog}`)).status).toBe(200);
    const after = (await member().call("GET", `/tasks/${task.id}`)).body;
    expect(after).toMatchObject({ statusId: backlog, version: task.version + 1 });
  });
});
