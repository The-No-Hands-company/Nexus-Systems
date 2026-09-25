import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { startTestServer } from "./support/server";

let t: Awaited<ReturnType<typeof startTestServer>>;
beforeAll(async () => {
  t = await startTestServer();
});
afterAll(() => t.close());

const owner = () => t.as("usr-owner");
const admin = () => t.as("usr-admin");
const member = () => t.as("usr-member");
const viewer = () => t.as("usr-viewer");
const stranger = () => t.as("usr-stranger");

async function team(name = "Studio"): Promise<string> {
  const res = await owner().call("POST", "/workspaces", { name });
  expect(res.status).toBe(201);
  const id = res.body.id as string;
  for (const [subject, role] of [["usr-admin", "admin"], ["usr-member", "member"], ["usr-viewer", "viewer"]] as const) {
    expect((await owner().call("PUT", `/workspaces/${id}/members/${subject}`, { role })).status).toBe(200);
  }
  return id;
}

describe("personal workspace", () => {
  it("is created once, on first use, with the caller as its only owner", async () => {
    const first = await t.as("usr-solo").call("GET", "/workspaces");
    const second = await t.as("usr-solo").call("GET", "/workspaces");
    expect(first.status).toBe(200);
    expect(first.body.workspaces).toEqual(second.body.workspaces);
    expect(first.body.workspaces).toHaveLength(1);
    expect(first.body.workspaces[0]).toMatchObject({ name: "Personal", kind: "personal", role: "owner" });
    const members = await t.as("usr-solo").call("GET", `/workspaces/${first.body.workspaces[0].id}/members`);
    expect(members.body.members.map((m: { subject: string }) => m.subject)).toEqual(["usr-solo"]);
  });

  it("cannot gain members or be deleted", async () => {
    const id = (await t.as("usr-solo").call("GET", "/workspaces")).body.workspaces[0].id;
    const add = await t.as("usr-solo").call("PUT", `/workspaces/${id}/members/usr-friend`, { role: "member" });
    expect(add.status).toBe(422);
    expect(add.body.error).toBe("personal_workspace");
    expect((await t.as("usr-solo").call("DELETE", `/workspaces/${id}`)).body.error).toBe("personal_workspace");
  });
});

describe("team workspace", () => {
  it("lists the personal workspace first and the team with the caller's role", async () => {
    const id = await team("Alpha");
    const list = (await member().call("GET", "/workspaces")).body.workspaces;
    expect(list[0].kind).toBe("personal");
    expect(list.find((w: { id: string }) => w.id === id)).toMatchObject({ name: "Alpha", kind: "team", role: "member" });
  });

  it("hides itself from non-members", async () => {
    const id = await team();
    expect((await stranger().call("GET", `/workspaces/${id}/members`)).status).toBe(404);
    expect((await stranger().call("PATCH", `/workspaces/${id}`, { name: "Mine" })).status).toBe(404);
  });

  it("lets admins rename but not members or viewers", async () => {
    const id = await team();
    expect((await viewer().call("PATCH", `/workspaces/${id}`, { name: "X" })).status).toBe(403);
    expect((await member().call("PATCH", `/workspaces/${id}`, { name: "X" })).status).toBe(403);
    const renamed = await admin().call("PATCH", `/workspaces/${id}`, { name: "Renamed" });
    expect(renamed.status).toBe(200);
    expect(renamed.body.name).toBe("Renamed");
  });

  it("lets admins manage members below owner", async () => {
    const id = await team();
    expect((await member().call("PUT", `/workspaces/${id}/members/usr-new`, { role: "member" })).status).toBe(403);
    expect((await admin().call("PUT", `/workspaces/${id}/members/usr-new`, { role: "member" })).status).toBe(200);
    expect((await admin().call("PUT", `/workspaces/${id}/members/usr-new`, { role: "owner" })).status).toBe(403);
    expect((await admin().call("PUT", `/workspaces/${id}/members/usr-owner`, { role: "member" })).status).toBe(403);
    expect((await admin().call("DELETE", `/workspaces/${id}/members/usr-new`)).status).toBe(200);
  });

  it("always keeps an owner", async () => {
    const id = await team();
    expect((await owner().call("PUT", `/workspaces/${id}/members/usr-owner`, { role: "admin" })).body.error).toBe("last_owner");
    expect((await owner().call("DELETE", `/workspaces/${id}/members/usr-owner`)).body.error).toBe("last_owner");
    expect((await owner().call("PUT", `/workspaces/${id}/members/usr-admin`, { role: "owner" })).status).toBe(200);
    expect((await owner().call("DELETE", `/workspaces/${id}/members/usr-owner`)).status).toBe(200);
  });

  it("lets anyone leave but not remove others without admin", async () => {
    const id = await team();
    expect((await member().call("DELETE", `/workspaces/${id}/members/usr-viewer`)).status).toBe(403);
    expect((await viewer().call("DELETE", `/workspaces/${id}/members/usr-viewer`)).status).toBe(200);
    expect((await viewer().call("GET", `/workspaces/${id}/members`)).status).toBe(404);
  });

  it("can only be deleted by an owner", async () => {
    const id = await team();
    expect((await admin().call("DELETE", `/workspaces/${id}`)).status).toBe(403);
    expect((await owner().call("DELETE", `/workspaces/${id}`)).status).toBe(200);
    expect((await owner().call("GET", `/workspaces/${id}/members`)).status).toBe(404);
  });

  it("validates its input", async () => {
    const id = await team();
    expect((await owner().call("POST", "/workspaces", { name: " " })).status).toBe(400);
    expect((await owner().call("POST", "/workspaces", { name: "A", kind: "personal" })).status).toBe(400);
    expect((await owner().call("PUT", `/workspaces/${id}/members/usr-x`, { role: "god" })).status).toBe(400);
    expect((await owner().call("PUT", `/workspaces/${id}/members/usr-x`, {})).status).toBe(400);
    expect((await owner().call("PUT", `/workspaces/${id}/members/${encodeURIComponent("usr x")}`, { role: "member" })).status).toBe(400);
  });
});
