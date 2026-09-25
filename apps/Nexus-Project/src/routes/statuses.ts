import { json, readJson } from "../http";
import { type Router, param } from "../router";
import type { Context } from "../server";
import * as statuses from "../store/statuses";
import { enumValue, field, idValue, intValue, object, requiredText } from "../validation";

export function registerStatusRoutes(router: Router<Context>): void {
  router.add("GET", "/projects/:id/statuses", ({ db, subject }, params) =>
    json({ statuses: statuses.listStatuses(db, subject, param(params, "id")) }),
  );

  router.add("POST", "/projects/:id/statuses", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["name", "category"]);
    const name = requiredText(body, "name", 200);
    const category = enumValue("category", statuses.STATUS_CATEGORIES)(body.category);
    return json(statuses.createStatus(db, subject, param(params, "id"), name, category), 201);
  });

  router.add("PATCH", "/statuses/:id", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["name", "category", "position"]);
    const patch: statuses.StatusPatch = {};
    if (body.name !== undefined) patch.name = requiredText(body, "name", 200);
    const category = field(body, "category", enumValue("category", statuses.STATUS_CATEGORIES));
    if (category !== undefined) patch.category = category;
    const position = field(body, "position", intValue("position", 0, 1000));
    if (position !== undefined) patch.position = position;
    return json(statuses.updateStatus(db, subject, param(params, "id"), patch));
  });

  router.add("DELETE", "/statuses/:id", ({ db, subject, url }, params) => {
    const target = url.searchParams.get("moveTasksTo");
    statuses.deleteStatus(
      db,
      subject,
      param(params, "id"),
      target === null ? null : idValue("moveTasksTo")(target),
    );
    return json({ deleted: true });
  });
}
