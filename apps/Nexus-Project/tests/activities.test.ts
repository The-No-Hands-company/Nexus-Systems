import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import type { TaskActivity } from "../src/store/activities";
import type { WithRescheduled } from "../src/store/scheduling";
import type { Task } from "../src/store/tasks";
import { createProject, teamWithRoles } from "./support/fixtures";
import { startTestServer } from "./support/server";

let t: Awaited<ReturnType<typeof startTestServer>>;
let ws: string;
let prj: string;

beforeAll(async () => {
  t = await startTestServer();
  ws = await teamWithRoles(t);
  const p = await createProject(t.as("usr-member"), ws);
  prj = p.id;
});
afterAll(() => t.close());

const member = () => t.as("usr-member");
const viewer = () => t.as("usr-viewer");

describe("task activity and comment log", () => {
  it("automatically logs task creation activity", async () => {
    const res = await member().call<WithRescheduled<Task>>("POST", `/projects/${prj}/tasks`, {
      title: "Audited Task",
    });
    expect(res.status).toBe(201);
    const taskId = res.body.id;

    const actRes = await member().call<{ activities: TaskActivity[] }>(
      "GET",
      `/tasks/${taskId}/activities`,
    );
    expect(actRes.status).toBe(200);
    expect(actRes.body.activities.length).toBeGreaterThanOrEqual(1);
    const creation = actRes.body.activities[0];
    expect(creation).toBeDefined();
    expect(creation?.kind).toBe("created");
    expect(creation?.authorSubject).toBe("usr-member");
  });

  it("logs user comments on tasks", async () => {
    const res = await member().call<WithRescheduled<Task>>("POST", `/projects/${prj}/tasks`, {
      title: "Commentable Task",
    });
    const taskId = res.body.id;

    const commentRes = await member().call<{ activity: TaskActivity }>(
      "POST",
      `/tasks/${taskId}/comments`,
      {
        message: "Here is a progress update comment.",
      },
    );
    expect(commentRes.status).toBe(201);
    expect(commentRes.body.activity.message).toBe("Here is a progress update comment.");
    expect(commentRes.body.activity.kind).toBe("comment");

    const actRes = await member().call<{ activities: TaskActivity[] }>(
      "GET",
      `/tasks/${taskId}/activities`,
    );
    expect(actRes.status).toBe(200);
    const comments = actRes.body.activities.filter((a) => a.kind === "comment");
    expect(comments).toHaveLength(1);
    expect(comments[0]?.message).toBe("Here is a progress update comment.");
  });

  it("logs title and status changes", async () => {
    const res = await member().call<WithRescheduled<Task>>("POST", `/projects/${prj}/tasks`, {
      title: "Initial Title",
    });
    const task = res.body;

    const patchRes = await member().call<WithRescheduled<Task>>(
      "PATCH",
      `/tasks/${task.id}`,
      {
        title: "Renamed Title",
        progress: 50,
      },
      { "if-match": String(task.version) },
    );
    expect(patchRes.status).toBe(200);

    const actRes = await member().call<{ activities: TaskActivity[] }>(
      "GET",
      `/tasks/${task.id}/activities`,
    );
    const updates = actRes.body.activities.filter((a) => a.kind === "update");
    expect(updates.length).toBeGreaterThanOrEqual(1);
  });

  it("lets viewers read activities but forbids them from posting comments", async () => {
    const res = await member().call<WithRescheduled<Task>>("POST", `/projects/${prj}/tasks`, {
      title: "Viewer Task",
    });
    const taskId = res.body.id;

    const readRes = await viewer().call<{ activities: TaskActivity[] }>(
      "GET",
      `/tasks/${taskId}/activities`,
    );
    expect(readRes.status).toBe(200);

    const postRes = await viewer().call("POST", `/tasks/${taskId}/comments`, {
      message: "Forbidden viewer comment",
    });
    expect(postRes.status).toBe(403);
  });
});
