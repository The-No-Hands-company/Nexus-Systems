import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createProject, teamWithRoles } from "./support/fixtures";
import { startTestServer } from "./support/server";

let t: Awaited<ReturnType<typeof startTestServer>>;
beforeAll(async () => {
  t = await startTestServer();
});
afterAll(() => t.close());

describe("projects", () => {
  it("lets a member create one, seeded with a status per category", async () => {
    const ws = await teamWithRoles(t);
    const project = await createProject(t.as("usr-member"), ws, { key: "WEB", name: "Website" });
    expect(project).toMatchObject({
      key: "WEB",
      name: "Website",
      startDate: "2026-09-07",
      scheduleMode: "manual",
      visibility: "workspace",
      archived: false,
      role: "member",
    });
    const statuses = await t.as("usr-viewer").call("GET", `/projects/${project.id}/statuses`);
    expect(statuses.status).toBe(200);
    expect(statuses.body.statuses.map((s: { category: string }) => s.category)).toEqual([
      "backlog",
      "unstarted",
      "started",
      "completed",
      "canceled",
    ]);
  });

  it("refuses viewers and strangers", async () => {
    const ws = await teamWithRoles(t);
    const body = { key: "NOPE", name: "No", startDate: "2026-09-07" };
    expect((await t.as("usr-viewer").call("POST", `/workspaces/${ws}/projects`, body)).status).toBe(403);
    expect((await t.as("usr-stranger").call("POST", `/workspaces/${ws}/projects`, body)).status).toBe(404);
  });

  it("keeps keys well-formed and unique per workspace", async () => {
    const ws = await teamWithRoles(t);
    const other = await teamWithRoles(t, "Other");
    await createProject(t.as("usr-owner"), ws, { key: "OPS" });
    const dup = await t.as("usr-owner").call("POST", `/workspaces/${ws}/projects`, { key: "OPS", name: "x", startDate: "2026-09-07" });
    expect(dup.status).toBe(409);
    expect(dup.body.error).toBe("key_taken");
    await createProject(t.as("usr-owner"), other, { key: "OPS" });
    for (const key of ["ops", "O", "1OPS", "TOOLONGKEY1", "OP-S"]) {
      expect((await t.as("usr-owner").call("POST", `/workspaces/${ws}/projects`, { key, name: "x", startDate: "2026-09-07" })).status).toBe(400);
    }
  });

  it("hides a restricted project from members who are not listed", async () => {
    const ws = await teamWithRoles(t);
    const secret = await createProject(t.as("usr-member"), ws, { visibility: "restricted" });
    expect((await t.as("usr-member").call("GET", `/projects/${secret.id}`)).status).toBe(200);
    expect((await t.as("usr-admin").call("GET", `/projects/${secret.id}`)).status).toBe(200);
    expect((await t.as("usr-viewer").call("GET", `/projects/${secret.id}`)).status).toBe(404);
    const listed = await t.as("usr-viewer").call("GET", `/workspaces/${ws}/projects`);
    expect(listed.body.projects.some((p: { id: string }) => p.id === secret.id)).toBe(false);

    expect((await t.as("usr-member").call("PUT", `/projects/${secret.id}/members/usr-viewer`)).status).toBe(403);
    expect((await t.as("usr-admin").call("PUT", `/projects/${secret.id}/members/usr-viewer`)).status).toBe(200);
    expect((await t.as("usr-viewer").call("GET", `/projects/${secret.id}`)).status).toBe(200);
    const outsider = await t.as("usr-admin").call("PUT", `/projects/${secret.id}/members/usr-stranger`);
    expect(outsider.status).toBe(422);
    expect(outsider.body.error).toBe("not_workspace_member");
    expect((await t.as("usr-admin").call("DELETE", `/projects/${secret.id}/members/usr-viewer`)).status).toBe(200);
    expect((await t.as("usr-viewer").call("GET", `/projects/${secret.id}`)).status).toBe(404);
  });

  it("lets admins change settings and hides archived projects by default", async () => {
    const ws = await teamWithRoles(t);
    const project = await createProject(t.as("usr-member"), ws);
    expect((await t.as("usr-member").call("PATCH", `/projects/${project.id}`, { name: "x" })).status).toBe(403);
    const updated = await t.as("usr-admin").call("PATCH", `/projects/${project.id}`, {
      name: "Renamed",
      scheduleMode: "auto",
      startDate: "2026-10-01",
      archived: true,
    });
    expect(updated.status).toBe(200);
    expect(updated.body).toMatchObject({ name: "Renamed", scheduleMode: "auto", startDate: "2026-10-01", archived: true });
    const active = await t.as("usr-member").call("GET", `/workspaces/${ws}/projects`);
    expect(active.body.projects.some((p: { id: string }) => p.id === project.id)).toBe(false);
    const all = await t.as("usr-member").call("GET", `/workspaces/${ws}/projects?archived=true`);
    expect(all.body.projects.some((p: { id: string }) => p.id === project.id)).toBe(true);
  });

  it("moves a project between workspaces where the caller is admin in both", async () => {
    const from = await teamWithRoles(t, "From");
    const to = await teamWithRoles(t, "To");
    const project = await createProject(t.as("usr-owner"), from, { key: "MOVE" });

    // A member can see the project but may not move it.
    expect((await t.as("usr-member").call("POST", `/projects/${project.id}/move`, { workspaceId: to })).status).toBe(403);
    // A workspace the caller is not in does not exist, as far as they can tell.
    const elsewhere = (await t.as("usr-stranger").call("GET", "/workspaces")).body.workspaces[0].id;
    expect((await t.as("usr-owner").call("POST", `/projects/${project.id}/move`, { workspaceId: elsewhere })).status).toBe(404);
    // Admin in the source but only a member in the target is not enough.
    await t.as("usr-owner").call("PUT", `/workspaces/${to}/members/usr-admin`, { role: "member" });
    expect((await t.as("usr-admin").call("POST", `/projects/${project.id}/move`, { workspaceId: to })).status).toBe(403);

    await createProject(t.as("usr-owner"), to, { key: "MOVE" });
    const clash = await t.as("usr-owner").call("POST", `/projects/${project.id}/move`, { workspaceId: to });
    expect(clash.status).toBe(409);
    expect(clash.body.error).toBe("key_taken");

    const solo = await createProject(t.as("usr-owner"), from, { key: "SOLO" });
    const moved = await t.as("usr-owner").call("POST", `/projects/${solo.id}/move`, { workspaceId: to });
    expect(moved.status).toBe(200);
    expect(moved.body.workspaceId).toBe(to);
    expect((await t.as("usr-viewer").call("GET", `/workspaces/${to}/projects`)).body.projects.map((p: { key: string }) => p.key)).toContain("SOLO");
  });

  it("drops listed members who are not in the target workspace", async () => {
    const from = await teamWithRoles(t, "From2");
    const to = await teamWithRoles(t, "To2");
    await t.as("usr-owner").call("DELETE", `/workspaces/${to}/members/usr-member`);
    const project = await createProject(t.as("usr-owner"), from, { visibility: "restricted" });
    await t.as("usr-owner").call("PUT", `/projects/${project.id}/members/usr-member`);
    const moved = await t.as("usr-owner").call("POST", `/projects/${project.id}/move`, { workspaceId: to });
    expect(moved.status).toBe(200);
    const members = await t.as("usr-owner").call("GET", `/projects/${project.id}/members`);
    expect(members.body.members.map((m: { subject: string }) => m.subject)).toEqual(["usr-owner"]);
  });

  it("validates its input", async () => {
    const ws = await teamWithRoles(t);
    const post = (body: unknown) => t.as("usr-owner").call("POST", `/workspaces/${ws}/projects`, body);
    expect((await post({ key: "AB", name: "x" })).status).toBe(400);
    expect((await post({ key: "AB", name: "x", startDate: "2026-09-07", scheduleMode: "magic" })).status).toBe(400);
    expect((await post({ key: "AB", name: "x", startDate: "2026-09-07", owner: "me" })).status).toBe(400);
  });
});
