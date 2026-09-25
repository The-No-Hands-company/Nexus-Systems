import { badRequest, json, readJson } from "../http";
import { type Router, param } from "../router";
import type { Context } from "../server";
import * as projects from "../store/projects";
import { boolValue, dateValue, enumValue, field, idValue, object, requiredText, subjectValue, text } from "../validation";

const KEY = /^[A-Z][A-Z0-9]{1,9}$/;
const MODES = ["manual", "auto"] as const;
const VISIBILITY = ["workspace", "restricted"] as const;

function projectKey(value: unknown): string {
  if (typeof value !== "string" || !KEY.test(value)) {
    throw badRequest("key must be 2-10 characters: an uppercase letter, then uppercase letters or digits");
  }
  return value;
}

export function registerProjectRoutes(router: Router<Context>): void {
  router.add("GET", "/workspaces/:id/projects", ({ db, subject, url }, params) =>
    json({
      projects: projects.listProjects(db, subject, param(params, "id"), url.searchParams.get("archived") === "true"),
    }),
  );

  router.add("POST", "/workspaces/:id/projects", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["key", "name", "description", "startDate", "scheduleMode", "visibility"]);
    const startDate = field(body, "startDate", dateValue("startDate"));
    if (startDate === undefined) throw badRequest("startDate is required");
    const input: projects.ProjectInput = {
      key: projectKey(body.key),
      name: requiredText(body, "name", 200),
      description: text(body, "description", 50_000) ?? "",
      startDate,
      scheduleMode: field(body, "scheduleMode", enumValue("scheduleMode", MODES)) ?? "manual",
      visibility: field(body, "visibility", enumValue("visibility", VISIBILITY)) ?? "workspace",
    };
    return json(projects.createProject(db, subject, param(params, "id"), input), 201);
  });

  router.add("GET", "/projects/:id", ({ db, subject }, params) => json(projects.getProject(db, subject, param(params, "id"))));

  router.add("PATCH", "/projects/:id", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["name", "description", "startDate", "scheduleMode", "visibility", "archived"]);
    const patch: projects.ProjectPatch = {};
    if (body.name !== undefined) patch.name = requiredText(body, "name", 200);
    const description = text(body, "description", 50_000);
    if (description !== undefined) patch.description = description;
    const startDate = field(body, "startDate", dateValue("startDate"));
    if (startDate !== undefined) patch.startDate = startDate;
    const scheduleMode = field(body, "scheduleMode", enumValue("scheduleMode", MODES));
    if (scheduleMode !== undefined) patch.scheduleMode = scheduleMode;
    const visibility = field(body, "visibility", enumValue("visibility", VISIBILITY));
    if (visibility !== undefined) patch.visibility = visibility;
    const archived = field(body, "archived", boolValue("archived"));
    if (archived !== undefined) patch.archived = archived;
    return json(projects.updateProject(db, subject, param(params, "id"), patch));
  });

  router.add("POST", "/projects/:id/move", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["workspaceId"]);
    const workspaceId = field(body, "workspaceId", idValue("workspaceId"));
    if (workspaceId === undefined) throw badRequest("workspaceId is required");
    return json(projects.moveProject(db, subject, param(params, "id"), workspaceId));
  });

  router.add("GET", "/projects/:id/members", ({ db, subject }, params) =>
    json({ members: projects.listProjectMembers(db, subject, param(params, "id")) }),
  );

  router.add("PUT", "/projects/:id/members/:subject", ({ db, subject }, params) =>
    json(projects.addProjectMember(db, subject, param(params, "id"), subjectValue("subject")(param(params, "subject")))),
  );

  router.add("DELETE", "/projects/:id/members/:subject", ({ db, subject }, params) => {
    projects.removeProjectMember(db, subject, param(params, "id"), subjectValue("subject")(param(params, "subject")));
    return json({ deleted: true });
  });
}
