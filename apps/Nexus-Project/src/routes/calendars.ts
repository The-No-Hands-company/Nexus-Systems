import { badRequest, json, readJson } from "../http";
import { type Router, param } from "../router";
import type { Context } from "../server";
import * as calendars from "../store/calendars";
import { boolValue, dateValue, intValue, object } from "../validation";

const WEEKDAY = intValue("workingWeekdays[]", 0, 6);
const DATE = dateValue("exceptions[].date");
const WORKING = boolValue("exceptions[].working");

function parseCalendar(value: unknown): calendars.ProjectCalendar {
  const body = object(value, ["workingWeekdays", "exceptions"]);
  if (!Array.isArray(body.workingWeekdays) || body.workingWeekdays.length === 0) {
    throw badRequest("workingWeekdays must list at least one day (0 = Sunday … 6 = Saturday)");
  }
  const workingWeekdays = body.workingWeekdays.map(WEEKDAY);
  if (new Set(workingWeekdays).size !== workingWeekdays.length) throw badRequest("workingWeekdays must not repeat a day");
  const rawExceptions = body.exceptions ?? [];
  if (!Array.isArray(rawExceptions) || rawExceptions.length > 1000) throw badRequest("exceptions must be a list of at most 1000 dates");
  const exceptions = rawExceptions.map((raw) => {
    const entry = object(raw, ["date", "working"]);
    if (entry.working === undefined) throw badRequest("each exception needs date and working");
    return { date: DATE(entry.date), working: WORKING(entry.working) };
  });
  if (new Set(exceptions.map((e) => e.date)).size !== exceptions.length) throw badRequest("exception dates must be unique");
  return { workingWeekdays: [...workingWeekdays].sort((a, b) => a - b), exceptions };
}

export function registerCalendarRoutes(router: Router<Context>): void {
  router.add("GET", "/projects/:id/calendar", ({ db, subject }, params) =>
    json(calendars.getCalendar(db, subject, param(params, "id"))),
  );

  router.add("PUT", "/projects/:id/calendar", async ({ db, subject, req }, params) =>
    json(calendars.setCalendar(db, subject, param(params, "id"), parseCalendar(await readJson(req)))),
  );
}
