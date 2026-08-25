import type { EventCreate, EventRange } from "./calendar-engine";

export type EventPatch = Partial<EventCreate>;
export type ValidationResult<T> = { ok: true; value: T } | { ok: false; error: string };

const EVENT_FIELDS = new Set(["title", "description", "location", "startTime", "endTime", "allDay", "recurrence"]);
const MAX_TITLE_LENGTH = 512;
const MAX_TEXT_LENGTH = 10_000;
const MAX_RANGE_MS = 366 * 24 * 60 * 60 * 1000;
const ISO_TIMESTAMP = /^(\d{4})-(\d{2})-(\d{2})(?:T(\d{2}):(\d{2})(?::(\d{2})(?:\.(\d{1,3}))?)?(Z|[+-]\d{2}:\d{2})?)?$/;

function invalid<T>(error: string): ValidationResult<T> {
  return { ok: false, error };
}

function object(value: unknown): Record<string, unknown> | null {
  return value !== null && typeof value === "object" && !Array.isArray(value) ? value as Record<string, unknown> : null;
}

export function canonicalTimestamp(value: unknown): string | null {
  if (typeof value !== "string") return null;
  const match = ISO_TIMESTAMP.exec(value);
  if (!match) return null;
  const [, yearText, monthText, dayText, hourText, minuteText, secondText, millisecondText, zone] = match;
  const year = Number(yearText);
  const month = Number(monthText);
  const day = Number(dayText);
  const hour = Number(hourText ?? "0");
  const minute = Number(minuteText ?? "0");
  const second = Number(secondText ?? "0");
  const millisecond = Number((millisecondText ?? "").padEnd(3, "0") || "0");
  if (month < 1 || month > 12 || hour > 23 || minute > 59 || second > 59) return null;
  const localInstant = new Date(Date.UTC(year, month - 1, day, hour, minute, second, millisecond));
  if (
    localInstant.getUTCFullYear() !== year ||
    localInstant.getUTCMonth() !== month - 1 ||
    localInstant.getUTCDate() !== day
  ) return null;
  if (zone && zone !== "Z") {
    const offsetHour = Number(zone.slice(1, 3));
    const offsetMinute = Number(zone.slice(4, 6));
    if (offsetHour > 23 || offsetMinute > 59) return null;
  }
  const timestamp = zone ? Date.parse(value) : localInstant.getTime();
  return Number.isNaN(timestamp) ? null : new Date(timestamp).toISOString();
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
  const startTime = canonicalTimestamp(input.startTime);
  const endTime = canonicalTimestamp(input.endTime);
  if (!startTime || !endTime) return invalid("startTime and endTime must be ISO timestamps");
  if (Date.parse(endTime) <= Date.parse(startTime)) return invalid("endTime must be after startTime");
  if (input.allDay !== undefined && typeof input.allDay !== "boolean") return invalid("allDay must be boolean");

  const description = optionalText(input.description);
  const location = optionalText(input.location);
  const recurrence = optionalText(input.recurrence);
  if (description === null || location === null || recurrence === null) return invalid("event text is invalid or too long");
  return {
    ok: true,
    value: {
      title,
      startTime,
      endTime,
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
  const startTime = input.startTime === undefined ? undefined : canonicalTimestamp(input.startTime);
  const endTime = input.endTime === undefined ? undefined : canonicalTimestamp(input.endTime);
  if (startTime === null) return invalid("startTime must be an ISO timestamp");
  if (endTime === null) return invalid("endTime must be an ISO timestamp");
  if (input.allDay !== undefined && typeof input.allDay !== "boolean") return invalid("allDay must be boolean");
  const description = optionalText(input.description);
  const location = optionalText(input.location);
  const recurrence = optionalText(input.recurrence);
  if (description === null || location === null || recurrence === null) return invalid("event text is invalid or too long");
  return {
    ok: true,
    value: {
      ...(title === undefined ? {} : { title }),
      ...(startTime === undefined ? {} : { startTime }),
      ...(endTime === undefined ? {} : { endTime }),
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
  const normalizedFrom = canonicalTimestamp(from);
  const normalizedTo = canonicalTimestamp(to);
  if (!normalizedFrom || !normalizedTo) return invalid("from and to must be ISO timestamps");
  const fromMs = Date.parse(normalizedFrom);
  const toMs = Date.parse(normalizedTo);
  if (toMs <= fromMs) return invalid("to must be after from");
  if (toMs - fromMs > MAX_RANGE_MS) return invalid("date range is too large");
  return { ok: true, value: { from: normalizedFrom, to: normalizedTo } };
}

export function isEventId(value: string): boolean {
  return /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i.test(value);
}
