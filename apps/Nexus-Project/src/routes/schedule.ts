import { projectAccess } from "../access";
import { json } from "../http";
import { type Router, param } from "../router";
import type { Context } from "../server";
import { listAssignedTasks } from "../store/me";
import { computeSchedule } from "../store/scheduling";

export function registerScheduleRoutes(router: Router<Context>): void {
  router.add("GET", "/projects/:id/schedule", ({ db, subject }, params) => {
    const { project } = projectAccess(db, param(params, "id"), subject);
    return json(computeSchedule(db, project));
  });

  router.add("GET", "/me/tasks", ({ db, subject }) =>
    json({ tasks: listAssignedTasks(db, subject) }),
  );
}
