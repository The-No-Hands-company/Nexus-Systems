import { beforeEach, describe, expect, test } from "bun:test";
import { clearUsers, createUser, updateUser, userHasPermission } from "../src/users";
import { createSession } from "../src/sessions";
import { createApiKey } from "../src/api-keys";
import { handleRequest } from "../src/server";
import type { IdentityRole, Permission } from "../src/types";

const permission: Permission = "platform:catalog:approve";
beforeEach(() => clearUsers());
function user(role: IdentityRole) {
  return createUser({ username: role, email: `${role}@test.local`, password: "test-password-long", role }); // pragma: allowlist secret
}
async function check(token?: string) {
  return handleRequest(new Request("http://localhost/api/v1/auth/check", { headers: {
    ...(token ? { authorization: `Bearer ${token}` } : {}),
    "x-permissions": permission, "x-role": "founder",
  } }));
}

describe("catalog approval permissions", () => {
  test.each([ ["founder", true], ["admin", true], ["operator", false], ["user", false], ["viewer", false] ] as const)("%s permission is %s", (role, allowed) => {
    expect(userHasPermission(user(role).id, permission)).toBe(allowed);
  });
  test.each(["founder", "admin", "operator", "user", "viewer"] as const)("check exposes server-derived permissions for %s sessions", async (role) => {
    const owner = user(role);
    const response = await check(createSession({ userId: owner.id }).token);
    expect(response.status).toBe(200);
    const body = await response.json();
    expect(body.userId).toBe(owner.id);
    expect(Array.isArray(body.permissions)).toBe(true);
    expect(body.permissions.includes(permission)).toBe(role === "founder" || role === "admin");
  });
  test("API keys inherit their owner's permissions, never caller scopes", async () => {
    const owner = user("operator");
    const { rawKey } = createApiKey({ userId: owner.id, name: "scoped", scopes: [permission] });
    expect((await (await check(rawKey)).json()).permissions).not.toContain(permission);
    updateUser(owner.id, { role: "admin" });
    expect((await (await check(rawKey)).json()).permissions).toContain(permission);
  });
  test.each([undefined, "invalid"])("unauthorized check omits permissions (%s)", async (token) => {
    const response = await check(token);
    expect(response.status).toBe(401);
    expect((await response.json()).permissions).toBeUndefined();
  });
  test("inactive owners cannot advertise approval permission", async () => {
    const owner = user("founder");
    const token = createSession({ userId: owner.id }).token;
    updateUser(owner.id, { status: "suspended" });
    expect((await check(token)).status).toBe(401);
    expect(userHasPermission(owner.id, permission)).toBe(false);
  });
});
