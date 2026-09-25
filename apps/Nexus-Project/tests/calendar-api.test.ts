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

const HOLIDAY = {
  workingWeekdays: [1, 2, 3, 4, 5],
  exceptions: [{ date: "2026-09-08", working: false }],
};

describe("project calendar", () => {
  it("defaults to Monday to Friday with no exceptions", async () => {
    const project = await createProject(t.as("usr-member"), ws);
    const res = await t.as("usr-viewer").call("GET", `/projects/${project.id}/calendar`);
    expect(res.body).toEqual({ workingWeekdays: [1, 2, 3, 4, 5], exceptions: [] });
  });

  it("is changed by admins only and re-derives finish dates", async () => {
    const project = await createProject(t.as("usr-member"), ws);
    const task = (
      await t.as("usr-member").call<Task>("POST", `/projects/${project.id}/tasks`, {
        title: "A",
        startDate: "2026-09-07",
        durationDays: 3,
      })
    ).body;
    expect(task.finishDate).toBe("2026-09-09");
    expect(
      (await t.as("usr-member").call("PUT", `/projects/${project.id}/calendar`, HOLIDAY)).status,
    ).toBe(403);
    const res = await t.as("usr-admin").call("PUT", `/projects/${project.id}/calendar`, HOLIDAY);
    expect(res.status).toBe(200);
    // The re-derived finish moved another task, so the response reports it.
    expect(res.body).toEqual({
      ...HOLIDAY,
      rescheduled: [
        {
          id: task.id,
          version: task.version + 1,
          startDate: "2026-09-07",
          finishDate: "2026-09-10",
          progress: 0,
        },
      ],
    });
    const after = (await t.as("usr-member").call<Task>("GET", `/tasks/${task.id}`)).body;
    expect(after).toMatchObject({
      finishDate: "2026-09-10",
      durationDays: 3,
      version: task.version + 1,
    });
  });

  it("reschedules an auto project", async () => {
    const project = await createProject(t.as("usr-member"), ws, { scheduleMode: "auto" });
    const post = (body: Record<string, unknown>) =>
      t.as("usr-member").call<Task>("POST", `/projects/${project.id}/tasks`, body);
    const a = (await post({ title: "A", durationDays: 3 })).body;
    const b = (await post({ title: "B", durationDays: 1 })).body;
    await t.as("usr-member").call("POST", `/projects/${project.id}/dependencies`, {
      predecessorId: a.id,
      successorId: b.id,
    });
    expect((await t.as("usr-member").call<Task>("GET", `/tasks/${b.id}`)).body.startDate).toBe(
      "2026-09-10",
    );
    await t.as("usr-admin").call("PUT", `/projects/${project.id}/calendar`, HOLIDAY);
    expect((await t.as("usr-member").call<Task>("GET", `/tasks/${b.id}`)).body.startDate).toBe(
      "2026-09-11",
    );
  });

  it("validates its input", async () => {
    const project = await createProject(t.as("usr-member"), ws);
    const put = (body: unknown) =>
      t.as("usr-admin").call("PUT", `/projects/${project.id}/calendar`, body);
    expect((await put({ workingWeekdays: [], exceptions: [] })).status).toBe(400);
    expect((await put({ workingWeekdays: [7], exceptions: [] })).status).toBe(400);
    expect((await put({ workingWeekdays: [1, 1], exceptions: [] })).status).toBe(400);
    expect(
      (
        await put({
          workingWeekdays: [1],
          exceptions: [
            { date: "2026-09-08", working: false },
            { date: "2026-09-08", working: true },
          ],
        })
      ).status,
    ).toBe(400);
    expect(
      (await put({ workingWeekdays: [1], exceptions: [{ date: "2026-02-30", working: false }] }))
        .status,
    ).toBe(400);
    expect((await put({ workingWeekdays: [1], exceptions: [{ date: "2026-09-08" }] })).status).toBe(
      400,
    );
    const many = Array.from({ length: 1001 }, (_, i) => ({
      date: new Date(Date.UTC(2026, 0, 1 + i)).toISOString().slice(0, 10),
      working: false,
    }));
    expect((await put({ workingWeekdays: [1], exceptions: many })).status).toBe(400);
  });
});
