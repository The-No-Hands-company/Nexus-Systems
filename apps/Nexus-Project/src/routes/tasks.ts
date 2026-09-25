import { HttpError, badRequest, json, readJson } from "../http";
import { type Router, param } from "../router";
import type { Context } from "../server";
import * as tasks from "../store/tasks";
import {
  type Body,
  dateValue,
  enumValue,
  field,
  has,
  idValue,
  intValue,
  nullable,
  object,
  requiredText,
  subjectValue,
  text,
} from "../validation";

const FIELDS = [
  "title",
  "description",
  "statusId",
  "priority",
  "assigneeSubject",
  "kind",
  "durationDays",
  "startDate",
  "finishDate",
  "constraintType",
  "constraintDate",
  "deadline",
  "progress",
] as const;

const KINDS = ["task", "milestone"] as const;
const CONSTRAINTS = ["asap", "start_no_earlier_than"] as const;

/** The version the client last saw, from If-Match. Required on every task write. */
export function expectedVersion(req: Request): number {
  const header = req.headers.get("if-match");
  if (header === null) throw new HttpError(428, "version_required", "send If-Match with the task's version");
  const match = /^"?(\d+)"?$/.exec(header.trim());
  if (!match) throw badRequest("If-Match must be the task's version number");
  return Number(match[1]);
}

function parseFields(body: Body): tasks.TaskFields {
  const fields: tasks.TaskFields = {};
  if (has(body, "title")) fields.title = requiredText(body, "title", 512);
  const description = text(body, "description", 50_000);
  if (description !== undefined) fields.description = description;
  const statusId = field(body, "statusId", idValue("statusId"));
  if (statusId !== undefined) fields.statusId = statusId;
  const priority = field(body, "priority", enumValue("priority", tasks.PRIORITIES));
  if (priority !== undefined) fields.priority = priority;
  const assignee = nullable(body, "assigneeSubject", subjectValue("assigneeSubject"));
  if (assignee !== undefined) fields.assigneeSubject = assignee;
  const kind = field(body, "kind", enumValue("kind", KINDS));
  if (kind !== undefined) fields.kind = kind;
  const duration = nullable(body, "durationDays", intValue("durationDays", 0, 3650));
  if (duration !== undefined) fields.durationDays = duration;
  const startDate = nullable(body, "startDate", dateValue("startDate"));
  if (startDate !== undefined) fields.startDate = startDate;
  const finishDate = nullable(body, "finishDate", dateValue("finishDate"));
  if (finishDate !== undefined) fields.finishDate = finishDate;
  const constraintType = field(body, "constraintType", enumValue("constraintType", CONSTRAINTS));
  if (constraintType !== undefined) fields.constraintType = constraintType;
  const constraintDate = nullable(body, "constraintDate", dateValue("constraintDate"));
  if (constraintDate !== undefined) fields.constraintDate = constraintDate;
  const deadline = nullable(body, "deadline", dateValue("deadline"));
  if (deadline !== undefined) fields.deadline = deadline;
  const progress = field(body, "progress", intValue("progress", 0, 100));
  if (progress !== undefined) fields.progress = progress;
  return fields;
}

export function registerTaskRoutes(router: Router<Context>): void {
  router.add("GET", "/projects/:id/tasks", ({ db, subject, url }, params) => {
    const filters: tasks.TaskFilters = {};
    const status = url.searchParams.get("status");
    if (status !== null) filters.statusId = idValue("status")(status);
    const assignee = url.searchParams.get("assignee");
    if (assignee !== null) filters.assignee = assignee === "none" ? null : subjectValue("assignee")(assignee);
    const parent = url.searchParams.get("parent");
    if (parent !== null) filters.parent = parent === "root" ? null : idValue("parent")(parent);
    return json({ tasks: tasks.listTasks(db, subject, param(params, "id"), filters) });
  });

  router.add("POST", "/projects/:id/tasks", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), [...FIELDS, "parentId"]);
    const fields = parseFields(body);
    if (fields.title === undefined) throw badRequest("title is required");
    const input: tasks.TaskCreate = { ...fields, title: fields.title };
    const parentId = nullable(body, "parentId", idValue("parentId"));
    if (parentId !== undefined) input.parentId = parentId;
    return json(tasks.createTask(db, subject, param(params, "id"), input), 201);
  });

  router.add("GET", "/tasks/:id", ({ db, subject }, params) => json(tasks.getTask(db, subject, param(params, "id"))));

  router.add("PATCH", "/tasks/:id", async ({ db, subject, req }, params) => {
    const version = expectedVersion(req);
    const body = object(await readJson(req), FIELDS);
    return json(tasks.updateTask(db, subject, param(params, "id"), version, parseFields(body)));
  });

  router.add("POST", "/tasks/:id/move", async ({ db, subject, req }, params) => {
    const version = expectedVersion(req);
    const body = object(await readJson(req), ["parentId", "statusId", "afterId"]);
    const move: tasks.TaskMove = {};
    const parentId = nullable(body, "parentId", idValue("parentId"));
    if (parentId !== undefined) move.parentId = parentId;
    const statusId = field(body, "statusId", idValue("statusId"));
    if (statusId !== undefined) move.statusId = statusId;
    const afterId = nullable(body, "afterId", idValue("afterId"));
    if (afterId !== undefined) move.afterId = afterId;
    return json(tasks.moveTask(db, subject, param(params, "id"), version, move));
  });

  router.add("DELETE", "/tasks/:id", ({ db, subject }, params) => {
    tasks.deleteTask(db, subject, param(params, "id"));
    return json({ deleted: true });
  });
}
