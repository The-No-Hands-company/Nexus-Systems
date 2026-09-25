import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createProject, teamWithRoles } from "./support/fixtures";
import { type Client, startTestServer } from "./support/server";

/**
 * The authorization table for a restricted project, route by route: a
 * workspace member who is not listed on the project cannot tell it exists
 * (404); once listed, a member may do what the member role allows and is
 * refused the rest (403); a listed viewer may read and is refused every write.
 */
let t: Awaited<ReturnType<typeof startTestServer>>;
let ws: string;
let projectId: string;
const owner = () => t.as("usr-owner");

beforeAll(async () => {
  t = await startTestServer();
  ws = await teamWithRoles(t);
  projectId = (await createProject(owner(), ws, { visibility: "restricted" })).id;
});
afterAll(() => t.close());

type Minimum = "viewer" | "member" | "admin";

interface Request {
  method: string;
  path: string;
  body?: unknown;
  headers?: Record<string, string>;
}

interface Route {
  name: string;
  /** The least role that may use the route. */
  minimum: Minimum;
  /** Builds fresh fixtures (as the owner) so every call has something to act on. */
  prepare: () => Promise<Request>;
}

async function ok<T = Record<string, unknown>>(
  client: Client,
  method: string,
  path: string,
  body?: unknown,
) {
  const res = await client.call<T>(method, path, body);
  expect(res.status).toBeLessThan(300);
  return res.body;
}

const newTask = (fields: Record<string, unknown> = {}) =>
  ok<{ id: string; version: number }>(owner(), "POST", `/projects/${projectId}/tasks`, {
    title: "Fixture",
    ...fields,
  });
const newStatus = () =>
  ok<{ id: string }>(owner(), "POST", `/projects/${projectId}/statuses`, {
    name: `Status ${crypto.randomUUID().slice(0, 8)}`,
    category: "started",
  });
const newLink = async () => {
  const a = await newTask({ durationDays: 1 });
  const b = await newTask({ durationDays: 1 });
  return ok<{ id: string }>(owner(), "POST", `/projects/${projectId}/dependencies`, {
    predecessorId: a.id,
    successorId: b.id,
  });
};
const ifMatch = (version: number) => ({ "if-match": String(version) });

const ROUTES: Route[] = [
  {
    name: "GET tasks",
    minimum: "viewer",
    prepare: async () => ({ method: "GET", path: `/projects/${projectId}/tasks` }),
  },
  {
    name: "POST tasks",
    minimum: "member",
    prepare: async () => ({
      method: "POST",
      path: `/projects/${projectId}/tasks`,
      body: { title: "New" },
    }),
  },
  {
    name: "GET task",
    minimum: "viewer",
    prepare: async () => ({ method: "GET", path: `/tasks/${(await newTask()).id}` }),
  },
  {
    name: "PATCH task",
    minimum: "member",
    prepare: async () => {
      const task = await newTask();
      return {
        method: "PATCH",
        path: `/tasks/${task.id}`,
        body: { title: "Renamed" },
        headers: ifMatch(task.version),
      };
    },
  },
  {
    name: "POST task move",
    minimum: "member",
    prepare: async () => {
      const task = await newTask();
      return {
        method: "POST",
        path: `/tasks/${task.id}/move`,
        body: { afterId: null },
        headers: ifMatch(task.version),
      };
    },
  },
  {
    name: "DELETE task",
    minimum: "member",
    prepare: async () => ({ method: "DELETE", path: `/tasks/${(await newTask()).id}` }),
  },
  {
    name: "GET statuses",
    minimum: "viewer",
    prepare: async () => ({ method: "GET", path: `/projects/${projectId}/statuses` }),
  },
  {
    name: "POST statuses",
    minimum: "admin",
    prepare: async () => ({
      method: "POST",
      path: `/projects/${projectId}/statuses`,
      body: { name: `Added ${crypto.randomUUID().slice(0, 8)}`, category: "started" },
    }),
  },
  {
    name: "PATCH status",
    minimum: "admin",
    prepare: async () => ({
      method: "PATCH",
      path: `/statuses/${(await newStatus()).id}`,
      body: { position: 0 },
    }),
  },
  {
    name: "DELETE status",
    minimum: "admin",
    prepare: async () => ({ method: "DELETE", path: `/statuses/${(await newStatus()).id}` }),
  },
  {
    name: "GET dependencies",
    minimum: "viewer",
    prepare: async () => ({ method: "GET", path: `/projects/${projectId}/dependencies` }),
  },
  {
    name: "POST dependencies",
    minimum: "member",
    prepare: async () => {
      const a = await newTask({ durationDays: 1 });
      const b = await newTask({ durationDays: 1 });
      return {
        method: "POST",
        path: `/projects/${projectId}/dependencies`,
        body: { predecessorId: a.id, successorId: b.id },
      };
    },
  },
  {
    name: "PATCH dependency",
    minimum: "member",
    prepare: async () => ({
      method: "PATCH",
      path: `/dependencies/${(await newLink()).id}`,
      body: { lagDays: 2 },
    }),
  },
  {
    name: "DELETE dependency",
    minimum: "member",
    prepare: async () => ({ method: "DELETE", path: `/dependencies/${(await newLink()).id}` }),
  },
  {
    name: "GET calendar",
    minimum: "viewer",
    prepare: async () => ({ method: "GET", path: `/projects/${projectId}/calendar` }),
  },
  {
    name: "PUT calendar",
    minimum: "admin",
    prepare: async () => ({
      method: "PUT",
      path: `/projects/${projectId}/calendar`,
      body: { workingWeekdays: [1, 2, 3, 4, 5] },
    }),
  },
  {
    name: "GET schedule",
    minimum: "viewer",
    prepare: async () => ({ method: "GET", path: `/projects/${projectId}/schedule` }),
  },
  {
    name: "GET project members",
    minimum: "viewer",
    prepare: async () => ({ method: "GET", path: `/projects/${projectId}/members` }),
  },
  {
    name: "PUT project member",
    minimum: "admin",
    prepare: async () => ({ method: "PUT", path: `/projects/${projectId}/members/usr-admin` }),
  },
  {
    name: "DELETE project member",
    minimum: "admin",
    prepare: async () => {
      await ok(owner(), "PUT", `/projects/${projectId}/members/usr-admin`);
      return { method: "DELETE", path: `/projects/${projectId}/members/usr-admin` };
    },
  },
];

async function send(subject: string, route: Route): Promise<number> {
  const request = await route.prepare();
  const res = await t.as(subject).call(request.method, request.path, request.body, request.headers);
  return res.status;
}

const RANK: Record<Minimum, number> = { viewer: 0, member: 1, admin: 2 };

describe("a restricted project, route by route", () => {
  describe("unlisted workspace members cannot see it", () => {
    for (const route of ROUTES) {
      it(`${route.name}: 404 for an unlisted member and viewer`, async () => {
        expect(await send("usr-member", route)).toBe(404);
        expect(await send("usr-viewer", route)).toBe(404);
      });
    }
  });

  describe("once listed", () => {
    beforeAll(async () => {
      await ok(owner(), "PUT", `/projects/${projectId}/members/usr-member`);
      await ok(owner(), "PUT", `/projects/${projectId}/members/usr-viewer`);
    });

    for (const route of ROUTES) {
      const memberMay = RANK[route.minimum] <= RANK.member;
      it(`${route.name}: a listed member ${memberMay ? "succeeds" : "is refused (403)"}`, async () => {
        const status = await send("usr-member", route);
        if (memberMay) {
          expect(status).toBeGreaterThanOrEqual(200);
          expect(status).toBeLessThan(300);
        } else {
          expect(status).toBe(403);
        }
      });

      const viewerMay = route.minimum === "viewer";
      it(`${route.name}: a listed viewer ${viewerMay ? "reads it" : "is refused (403)"}`, async () => {
        const status = await send("usr-viewer", route);
        if (viewerMay) expect(status).toBe(200);
        else expect(status).toBe(403);
      });
    }
  });
});
