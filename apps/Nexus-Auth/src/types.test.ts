import { expect, test } from "bun:test";
import type { IdentityRole, Permission } from "./types";
import { createUser, deleteUser, userHasPermission } from "./users";

test("operator remains an identity role without catalog approval authority", () => {
  const role: IdentityRole = "operator";
  const permission: Permission = "platform:catalog:approve";
  const user = createUser({ username: "catalog-operator-contract", email: "catalog-operator@test.local", password: "test-password-long", role }); // pragma: allowlist secret
  try {
    expect(userHasPermission(user.id, "auth:read")).toBe(true);
    expect(userHasPermission(user.id, permission)).toBe(false);
  } finally { deleteUser(user.id); }
});
