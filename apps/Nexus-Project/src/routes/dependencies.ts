import { badRequest, json, readJson } from "../http";
import { type Router, param } from "../router";
import type { Context } from "../server";
import * as dependencies from "../store/dependencies";
import { enumValue, field, idValue, intValue, object } from "../validation";

const LAG = intValue("lagDays", -3650, 3650);

export function registerDependencyRoutes(router: Router<Context>): void {
  router.add("GET", "/projects/:id/dependencies", ({ db, subject }, params) =>
    json({ dependencies: dependencies.listDependencies(db, subject, param(params, "id")) }),
  );

  router.add("POST", "/projects/:id/dependencies", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["predecessorId", "successorId", "type", "lagDays"]);
    const predecessorId = field(body, "predecessorId", idValue("predecessorId"));
    const successorId = field(body, "successorId", idValue("successorId"));
    if (predecessorId === undefined || successorId === undefined) throw badRequest("predecessorId and successorId are required");
    const input: dependencies.DependencyInput = {
      predecessorId,
      successorId,
      type: field(body, "type", enumValue("type", dependencies.LINK_TYPES)) ?? "FS",
      lagDays: field(body, "lagDays", LAG) ?? 0,
    };
    return json(dependencies.createDependency(db, subject, param(params, "id"), input), 201);
  });

  router.add("PATCH", "/dependencies/:id", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["type", "lagDays"]);
    const patch: dependencies.DependencyPatch = {};
    const type = field(body, "type", enumValue("type", dependencies.LINK_TYPES));
    if (type !== undefined) patch.type = type;
    const lagDays = field(body, "lagDays", LAG);
    if (lagDays !== undefined) patch.lagDays = lagDays;
    return json(dependencies.updateDependency(db, subject, param(params, "id"), patch));
  });

  router.add("DELETE", "/dependencies/:id", ({ db, subject }, params) => {
    dependencies.deleteDependency(db, subject, param(params, "id"));
    return json({ deleted: true });
  });
}
