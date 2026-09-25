import { ROLES } from "../access";
import { badRequest, json, readJson } from "../http";
import { type Router, param } from "../router";
import type { Context } from "../server";
import * as workspaces from "../store/workspaces";
import { enumValue, field, object, requiredText, subjectValue } from "../validation";

export function registerWorkspaceRoutes(router: Router<Context>): void {
  router.add("GET", "/workspaces", ({ db, subject }) => {
    workspaces.ensurePersonalWorkspace(db, subject);
    return json({ workspaces: workspaces.listWorkspaces(db, subject) });
  });

  router.add("POST", "/workspaces", async ({ db, subject, req }) => {
    const body = object(await readJson(req), ["name"]);
    return json(workspaces.createTeamWorkspace(db, subject, requiredText(body, "name", 200)), 201);
  });

  router.add("PATCH", "/workspaces/:id", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["name"]);
    return json(workspaces.renameWorkspace(db, subject, param(params, "id"), requiredText(body, "name", 200)));
  });

  router.add("DELETE", "/workspaces/:id", ({ db, subject }, params) => {
    workspaces.deleteWorkspace(db, subject, param(params, "id"));
    return json({ deleted: true });
  });

  router.add("GET", "/workspaces/:id/members", ({ db, subject }, params) =>
    json({ members: workspaces.listMembers(db, subject, param(params, "id")) }),
  );

  router.add("PUT", "/workspaces/:id/members/:subject", async ({ db, subject, req }, params) => {
    const target = subjectValue("subject")(param(params, "subject"));
    const body = object(await readJson(req), ["role"]);
    const role = field(body, "role", enumValue("role", ROLES));
    if (role === undefined) throw badRequest("role is required");
    return json(workspaces.setMember(db, subject, param(params, "id"), target, role));
  });

  router.add("DELETE", "/workspaces/:id/members/:subject", ({ db, subject }, params) => {
    const target = subjectValue("subject")(param(params, "subject"));
    workspaces.removeMember(db, subject, param(params, "id"), target);
    return json({ deleted: true });
  });
}
