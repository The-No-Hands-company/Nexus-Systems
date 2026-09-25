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
    await t.as("usr-admin").call("POST", `/projects/${hidden.id}/tasks`, {
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

describe("summary tasks carry their roll-up", () => {
  const get = async (id: string) => (await member().call("GET", `/tasks/${id}`)).body;
  const patchTask = async (id: string, body: Record<string, unknown>) => {
    const current = await get(id);
    const res = await member().call("PATCH", `/tasks/${id}`, body, {
      "if-match": String(current.version),
    });
    expect(res.status).toBe(200);
    return res.body;
  };
  const rollUp = async (projectId: string, id: string) =>
    (await member().call("GET", `/projects/${projectId}/schedule`)).body.summaries.find(
      (s: { id: string }) => s.id === id,
    );
  const expectMatchesRollUp = async (projectId: string, id: string) => {
    const summary = await get(id);
    const expected = await rollUp(projectId, id);
    expect({
      start: summary.startDate,
      finish: summary.finishDate,
      progress: summary.progress,
    }).toEqual({ start: expected.start, finish: expected.finish, progress: expected.progress });
    return summary;
  };

  for (const scheduleMode of ["manual", "auto"] as const) {
    it(`${scheduleMode}: a new summary drops its own inputs and follows its children`, async () => {
      const project = await createProject(member(), ws, { scheduleMode });
      const phase = await post(project.id, {
        title: "Phase",
        startDate: "2026-09-21",
        durationDays: 9,
        progress: 40,
        constraintType: "start_no_earlier_than",
        constraintDate: "2026-09-21",
        deadline: "2026-10-30",
      });
      const a = await post(project.id, {
        title: "A",
        parentId: phase.id,
        startDate: "2026-09-08",
        durationDays: 2,
        progress: 100,
      });
      const b = await post(project.id, {
        title: "B",
        parentId: phase.id,
        startDate: "2026-09-14",
        durationDays: 2,
      });
      const summary = await expectMatchesRollUp(project.id, phase.id);
      expect(summary).toMatchObject({
        durationDays: null,
        constraintType: "asap",
        constraintDate: null,
        deadline: "2026-10-30",
        progress: 50,
      });
      expect(summary.startDate).not.toBeNull();

      await patchTask(b.id, { durationDays: 6, progress: 50 });
      const moved = await expectMatchesRollUp(project.id, phase.id);
      expect(moved.finishDate).not.toBe(summary.finishDate);
      expect(moved.progress).toBe(63); // (2 × 100 + 6 × 50) / 8 = 62.5
      expect(a.id).toBeDefined();
    });
  }

  it("moving a task under a leaf turns that leaf into a summary", async () => {
    const project = await createProject(member(), ws);
    const phase = await post(project.id, {
      title: "Phase",
      startDate: "2026-09-21",
      durationDays: 3,
    });
    const child = await post(project.id, {
      title: "Child",
      startDate: "2026-09-08",
      durationDays: 2,
    });
    const res = await member().call(
      "POST",
      `/tasks/${child.id}/move`,
      { parentId: phase.id },
      { "if-match": String(child.version) },
    );
    expect(res.status).toBe(200);
    const summary = await expectMatchesRollUp(project.id, phase.id);
    expect(summary).toMatchObject({
      durationDays: null,
      startDate: "2026-09-08",
      finishDate: "2026-09-09",
    });
    expect(res.body.rescheduled).toContainEqual({
      id: phase.id,
      version: summary.version,
      startDate: "2026-09-08",
      finishDate: "2026-09-09",
      progress: 0,
    });
  });
});

describe("a summary that loses its last child", () => {
  it("is an unscheduled leaf again, reported by the write that emptied it", async () => {
    const project = await createProject(member(), ws);
    const phase = await post(project.id, { title: "Phase" });
    const child = await post(project.id, {
      title: "Child",
      parentId: phase.id,
      startDate: "2026-09-08",
      durationDays: 2,
    });
    const res = await member().call(
      "POST",
      `/tasks/${child.id}/move`,
      { parentId: null },
      { "if-match": String(child.version) },
    );
    expect(res.status).toBe(200);
    const leaf = (await member().call("GET", `/tasks/${phase.id}`)).body;
    expect(leaf).toMatchObject({ startDate: null, finishDate: null, durationDays: null });
    expect(res.body.rescheduled).toEqual([
      { id: phase.id, version: leaf.version, startDate: null, finishDate: null, progress: 0 },
    ]);

    const again = await post(project.id, {
      title: "Second",
      parentId: phase.id,
      startDate: "2026-09-08",
      durationDays: 1,
    });
    const removed = await member().call("DELETE", `/tasks/${again.id}`);
    const emptied = (await member().call("GET", `/tasks/${phase.id}`)).body;
    expect(emptied).toMatchObject({ startDate: null, finishDate: null });
    expect(removed.body.rescheduled).toEqual([
      { id: phase.id, version: emptied.version, startDate: null, finishDate: null, progress: 0 },
    ]);
  });
});

describe("writes report the other tasks they moved", () => {
  it("auto mode: a PATCH returns the pushed successor, whose new version then works", async () => {
    const project = await createProject(member(), ws, { scheduleMode: "auto" });
    const a = await post(project.id, { title: "A", durationDays: 2 });
    const b = await post(project.id, { title: "B", durationDays: 1 });
    const link = await member().call("POST", `/projects/${project.id}/dependencies`, {
      predecessorId: a.id,
      successorId: b.id,
    });
    expect(link.status).toBe(201);
    const pushed = link.body.rescheduled.find((row: { id: string }) => row.id === b.id);
    expect(pushed).toMatchObject({ startDate: "2026-09-09", finishDate: "2026-09-09" });

    const current = (await member().call("GET", `/tasks/${a.id}`)).body;
    const res = await member().call(
      "PATCH",
      `/tasks/${a.id}`,
      { durationDays: 4 },
      { "if-match": String(current.version) },
    );
    expect(res.status).toBe(200);
    const moved = (await member().call("GET", `/tasks/${b.id}`)).body;
    expect(res.body.rescheduled).toEqual([
      {
        id: b.id,
        version: moved.version,
        startDate: "2026-09-11",
        finishDate: "2026-09-11",
        progress: 0,
      },
    ]);
    const follow = await member().call(
      "PATCH",
      `/tasks/${b.id}`,
      { title: "B renamed" },
      { "if-match": String(res.body.rescheduled[0].version) },
    );
    expect(follow.status).toBe(200);
    expect(follow.body.rescheduled).toEqual([]);
  });

  it("manual mode: a child edit returns its summary", async () => {
    const project = await createProject(member(), ws);
    const phase = await post(project.id, { title: "Phase" });
    const child = await post(project.id, {
      title: "Child",
      parentId: phase.id,
      startDate: "2026-09-07",
      durationDays: 1,
    });
    const res = await member().call(
      "PATCH",
      `/tasks/${child.id}`,
      { durationDays: 3 },
      { "if-match": String(child.version) },
    );
    expect(res.status).toBe(200);
    const summary = (await member().call("GET", `/tasks/${phase.id}`)).body;
    expect(res.body.rescheduled).toEqual([
      {
        id: phase.id,
        version: summary.version,
        startDate: "2026-09-07",
        finishDate: "2026-09-09",
        progress: 0,
      },
    ]);
  });

  it("every schedule-affecting route carries the field", async () => {
    const admin = t.as("usr-admin");
    const project = await createProject(admin, ws);
    const a = await post(project.id, { title: "A", durationDays: 1 });
    const b = await post(project.id, { title: "B", durationDays: 1 });
    const created = await member().call("POST", `/projects/${project.id}/tasks`, { title: "C" });
    expect(created.body.rescheduled).toEqual([]);
    const link = await member().call("POST", `/projects/${project.id}/dependencies`, {
      predecessorId: a.id,
      successorId: b.id,
    });
    expect(link.body.rescheduled).toEqual([]);
    const lag = await member().call("PATCH", `/dependencies/${link.body.id}`, { lagDays: 1 });
    expect(lag.body).toMatchObject({ lagDays: 1, rescheduled: [] });
    expect((await member().call("DELETE", `/dependencies/${link.body.id}`)).body).toEqual({
      deleted: true,
      rescheduled: [],
    });
    const calendar = await admin.call("PUT", `/projects/${project.id}/calendar`, {
      workingWeekdays: [1, 2, 3, 4, 5],
    });
    expect(calendar.body.rescheduled).toEqual([]);
    expect(
      (await admin.call("PATCH", `/projects/${project.id}`, { name: "Renamed" })).body.rescheduled,
    ).toEqual([]);
    const statuses = (await admin.call("GET", `/projects/${project.id}/statuses`)).body.statuses;
    const todo = statuses.find((s: { name: string }) => s.name === "Todo").id;
    const backlog = statuses.find((s: { name: string }) => s.name === "Backlog").id;
    expect(
      (await admin.call("PATCH", `/statuses/${todo}`, { name: "To do" })).body.rescheduled,
    ).toEqual([]);
    // Every task in the deleted status got a new status and a new version.
    const removed = (await admin.call("DELETE", `/statuses/${todo}?moveTasksTo=${backlog}`)).body;
    expect(removed.deleted).toBe(true);
    expect(removed.rescheduled.map((row: { id: string }) => row.id).sort()).toEqual(
      [a.id, b.id, created.body.id].sort(),
    );
    const current = (await member().call("GET", `/tasks/${a.id}`)).body;
    expect(
      (
        await member().call(
          "POST",
          `/tasks/${a.id}/move`,
          { afterId: null },
          { "if-match": String(current.version) },
        )
      ).body.rescheduled,
    ).toEqual([]);
    expect((await member().call("DELETE", `/tasks/${a.id}`)).body).toEqual({
      deleted: true,
      rescheduled: [],
    });
  });
});
