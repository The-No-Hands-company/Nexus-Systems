import type { EventCreate, EventRange } from "./calendar-engine";

export type EventPatch = Partial<EventCreate>;
export type ValidationResult<T> = { ok: true; value: T } | { ok: false; error: string };

const EVENT_FIELDS = new Set(["title", "description", "location", "startTime", "endTime", "allDay", "recurrence"]);
const MAX_TITLE_LENGTH = 512;
const MAX_TEXT_LENGTH = 10_000;
const MAX_RANGE_MS = 366 * 24 * 60 * 60 * 1000;

function invalid<T>(error: string): ValidationResult<T> {
  return { ok: false, error };
}

function object(value: unknown): Record<string, unknown> | null {
  return value !== null && typeof value === "object" && !Array.isArray(value) ? value as Record<string, unknown> : null;
}

function validDate(value: unknown): value is string {
  return typeof value === "string" && value.length > 0 && !Number.isNaN(Date.parse(value));
}

function validateKnownFields(value: Record<string, unknown>): string | null {
  const unknown = Object.keys(value).find((key) => !EVENT_FIELDS.has(key));
  if (unknown) return `unknown field: ${unknown}`;
  return null;
}

function optionalText(value: unknown): string | undefined | null {
  if (value === undefined) return undefined;
  if (typeof value !== "string" || value.length > MAX_TEXT_LENGTH) return null;
  return value;
}

export function parseEventCreate(value: unknown): ValidationResult<EventCreate> {
  const input = object(value);
  if (!input) return invalid("event must be an object");
  const fieldError = validateKnownFields(input);
  if (fieldError) return invalid(fieldError);
  if (typeof input.title !== "string") return invalid("title is required");
  const title = input.title.trim();
  if (!title || title.length > MAX_TITLE_LENGTH) return invalid("title must be 1-512 characters");
  if (!validDate(input.startTime) || !validDate(input.endTime)) return invalid("startTime and endTime must be valid timestamps");
  if (Date.parse(input.endTime) <= Date.parse(input.startTime)) return invalid("endTime must be after startTime");
  if (input.allDay !== undefined && typeof input.allDay !== "boolean") return invalid("allDay must be boolean");

  const description = optionalText(input.description);
  const location = optionalText(input.location);
  const recurrence = optionalText(input.recurrence);
  if (description === null || location === null || recurrence === null) return invalid("event text is invalid or too long");
  return {
    ok: true,
    value: {
      title,
      startTime: input.startTime,
      endTime: input.endTime,
      ...(description === undefined ? {} : { description }),
      ...(location === undefined ? {} : { location }),
      ...(input.allDay === undefined ? {} : { allDay: input.allDay }),
      ...(recurrence === undefined ? {} : { recurrence }),
    },
  };
}

export function parseEventPatch(value: unknown): ValidationResult<EventPatch> {
  const input = object(value);
  if (!input) return invalid("event patch must be an object");
  const fieldError = validateKnownFields(input);
  if (fieldError) return invalid(fieldError);
  if (Object.keys(input).length === 0) return invalid("event patch must not be empty");
  const title = input.title === undefined ? undefined : typeof input.title === "string" ? input.title.trim() : null;
  if (title === null || (title !== undefined && (!title || title.length > MAX_TITLE_LENGTH))) return invalid("title must be 1-512 characters");
  if (input.startTime !== undefined && !validDate(input.startTime)) return invalid("startTime must be a valid timestamp");
  if (input.endTime !== undefined && !validDate(input.endTime)) return invalid("endTime must be a valid timestamp");
  if (input.allDay !== undefined && typeof input.allDay !== "boolean") return invalid("allDay must be boolean");
  const description = optionalText(input.description);
  const location = optionalText(input.location);
  const recurrence = optionalText(input.recurrence);
  if (description === null || location === null || recurrence === null) return invalid("event text is invalid or too long");
  return {
    ok: true,
    value: {
      ...(title === undefined ? {} : { title }),
      ...(input.startTime === undefined ? {} : { startTime: input.startTime }),
      ...(input.endTime === undefined ? {} : { endTime: input.endTime }),
      ...(description === undefined ? {} : { description }),
      ...(location === undefined ? {} : { location }),
      ...(input.allDay === undefined ? {} : { allDay: input.allDay }),
      ...(recurrence === undefined ? {} : { recurrence }),
    },
  };
}

export function parseRange(url: URL): ValidationResult<EventRange> {
  const from = url.searchParams.get("from");
  const to = url.searchParams.get("to");
  if (!validDate(from) || !validDate(to)) return invalid("from and to must be valid timestamps");
  const fromMs = Date.parse(from);
  const toMs = Date.parse(to);
  if (toMs <= fromMs) return invalid("to must be after from");
  if (toMs - fromMs > MAX_RANGE_MS) return invalid("date range is too large");
  return { ok: true, value: { from: new Date(fromMs).toISOString(), to: new Date(toMs).toISOString() } };
}

export function isEventId(value: string): boolean {
  return /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i.test(value);
}
