/**
 * Allow-listed input parsing.
 *
 * Every mutation names the fields it accepts and refuses anything else, rather
 * than reading the ones it knows and ignoring the rest. Ignoring is safe right
 * up until some later code iterates the object, and "ownerSubject silently
 * dropped" is a far worse failure than "ownerSubject rejected with a 400".
 */

import type { EventCreate, EventPatch, EventRange } from "./calendar-engine";

export type ValidationResult<T> = { ok: true; value: T } | { ok: false; error: string };

const MAX_TITLE = 200;
const MAX_DESCRIPTION = 5_000;
const MAX_LOCATION = 300;
const MAX_RECURRENCE = 200;

/** The widest window one request may ask for. */
export const MAX_RANGE_DAYS = 366;

const DATE_ONLY = /^\d{4}-\d{2}-\d{2}$/;

const CREATE_FIELDS = ["title", "description", "location", "startTime", "endTime", "allDay", "recurrence"] as const;
type CreateField = (typeof CREATE_FIELDS)[number];

function fail<T>(error: string): ValidationResult<T> {
  return { ok: false, error };
}

/**
 * True only for a string a Date can actually resolve.
 *
 * `new Date("2026-13-45T99:99")` is Invalid Date, which is the check; the
 * regex first keeps obviously non-ISO input from depending on engine-specific
 * lenient parsing.
 */
function isTimestamp(value: unknown): value is string {
  if (typeof value !== "string" || !value.trim()) return false;
  if (!/^\d{4}-\d{2}-\d{2}([T ]\d{2}:\d{2}(:\d{2}(\.\d{1,3})?)?(Z|[+-]\d{2}:\d{2})?)?$/.test(value)) return false;
  return !Number.isNaN(new Date(value).getTime());
}

function bounded(value: unknown, max: number, field: string): ValidationResult<string | undefined> {
  if (value === undefined || value === null) return { ok: true, value: undefined };
  if (typeof value !== "string") return fail(`${field} must be a string`);
  if (value.length > max) return fail(`${field} exceeds ${max} characters`);
  return { ok: true, value };
}

function rejectUnknown(input: Record<string, unknown>, allowed: readonly string[]): string | null {
  const unknown = Object.keys(input).filter((k) => !allowed.includes(k));
  return unknown.length ? `unknown field(s): ${unknown.join(", ")}` : null;
}

function asObject(value: unknown): Record<string, unknown> | null {
  if (typeof value !== "object" || value === null || Array.isArray(value)) return null;
  return value as Record<string, unknown>;
}

/** Shared field parsing for create and patch; `require` drives which are mandatory. */
function parseFields(
  input: Record<string, unknown>,
  require: boolean,
): ValidationResult<Partial<EventCreate>> {
  const out: Partial<EventCreate> = {};

  if (require || "title" in input) {
    const { title } = input;
    if (typeof title !== "string" || !title.trim()) return fail("title must be a non-empty string");
    if (title.length > MAX_TITLE) return fail(`title exceeds ${MAX_TITLE} characters`);
    out.title = title.trim();
  }

  for (const [field, max] of [
    ["description", MAX_DESCRIPTION],
    ["location", MAX_LOCATION],
    ["recurrence", MAX_RECURRENCE],
  ] as const) {
    if (!(field in input)) continue;
    const r = bounded(input[field], max, field);
    if (!r.ok) return fail(r.error);
    out[field as "description" | "location" | "recurrence"] = r.value;
  }

  if ("allDay" in input) {
    if (typeof input.allDay !== "boolean") return fail("allDay must be a boolean");
    out.allDay = input.allDay;
  }

  for (const field of ["startTime", "endTime"] as const) {
    if (!require && !(field in input)) continue;
    if (!isTimestamp(input[field])) return fail(`${field} must be an ISO-8601 timestamp`);
    out[field] = input[field] as string;
  }

  // Only comparable when both ends are known. A patch moving one end is
  // re-checked against the stored event by the caller.
  if (out.startTime !== undefined && out.endTime !== undefined) {
    if (new Date(out.endTime).getTime() <= new Date(out.startTime).getTime()) {
      return fail("endTime must be after startTime");
    }
  }

  return { ok: true, value: out };
}

export function parseEventCreate(value: unknown): ValidationResult<EventCreate> {
  const input = asObject(value);
  if (!input) return fail("body must be a JSON object");

  const unknown = rejectUnknown(input, CREATE_FIELDS);
  if (unknown) return fail(unknown);

  const parsed = parseFields(input, true);
  if (!parsed.ok) return fail(parsed.error);
  return { ok: true, value: parsed.value as EventCreate };
}

export function parseEventPatch(value: unknown): ValidationResult<EventPatch> {
  const input = asObject(value);
  if (!input) return fail("body must be a JSON object");

  const unknown = rejectUnknown(input, CREATE_FIELDS);
  if (unknown) return fail(unknown);

  const present = CREATE_FIELDS.filter((f: CreateField) => f in input);
  if (present.length === 0) return fail("patch must change at least one field");

  const parsed = parseFields(input, false);
  if (!parsed.ok) return fail(parsed.error);
  return { ok: true, value: parsed.value };
}

/**
 * Bounds the query window.
 *
 * An unbounded range is one request that asks for the entire calendar, so the
 * span is capped and both ends must parse. Defaults mirror what the month view
 * asks for when it says nothing.
 */
export function parseRange(url: URL): ValidationResult<EventRange> {
  const today = new Date();
  const defaultFrom = today.toISOString().slice(0, 10);
  const defaultToDate = new Date(today);
  defaultToDate.setUTCDate(defaultToDate.getUTCDate() + 30);

  const from = url.searchParams.get("from") ?? defaultFrom;
  const to = url.searchParams.get("to") ?? defaultToDate.toISOString().slice(0, 10);

  if (!isTimestamp(from)) return fail("from must be an ISO-8601 date or timestamp");
  if (!isTimestamp(to)) return fail("to must be an ISO-8601 date or timestamp");

  const fromMs = new Date(DATE_ONLY.test(from) ? `${from}T00:00:00.000Z` : from).getTime();
  const toMs = new Date(DATE_ONLY.test(to) ? `${to}T00:00:00.000Z` : to).getTime();
  if (toMs < fromMs) return fail("to must not be before from");
  if (toMs - fromMs > MAX_RANGE_DAYS * 86_400_000) return fail(`range exceeds ${MAX_RANGE_DAYS} days`);

  return { ok: true, value: { from, to } };
}
