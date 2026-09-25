import { badRequest } from "./http";
import { toDay } from "./schedule/calendar";

export type Body = Record<string, unknown>;
type Parse<T> = (value: unknown) => T;

/** A plain JSON object containing only `allowed` keys. Anything else is a 400. */
export function object(value: unknown, allowed: readonly string[]): Body {
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    throw badRequest("body must be a JSON object");
  }
  const body = value as Body;
  for (const key of Object.keys(body)) {
    if (!allowed.includes(key)) throw badRequest(`unknown field: ${key}`);
  }
  return body;
}

export function has(body: Body, key: string): boolean {
  return Object.hasOwn(body, key);
}

/** Absent → undefined; present → parsed (null is refused by most parsers). */
export function field<T>(body: Body, key: string, parse: Parse<T>): T | undefined {
  return has(body, key) ? parse(body[key]) : undefined;
}

/** Absent → undefined; null → null (clear it); otherwise parsed. */
export function nullable<T>(body: Body, key: string, parse: Parse<T>): T | null | undefined {
  if (!has(body, key)) return undefined;
  return body[key] === null ? null : parse(body[key]);
}

export function text(body: Body, key: string, max: number): string | undefined {
  return field(body, key, (value) => {
    if (typeof value !== "string") throw badRequest(`${key} must be a string`);
    if (value.length > max) throw badRequest(`${key} must be at most ${max} characters`);
    return value;
  });
}

export function requiredText(body: Body, key: string, max: number): string {
  const value = text(body, key, max)?.trim();
  if (!value) throw badRequest(`${key} is required`);
  return value;
}

export function dateValue(key: string): Parse<string> {
  return (value) => {
    if (typeof value !== "string") throw badRequest(`${key} must be a YYYY-MM-DD date`);
    try {
      toDay(value);
    } catch {
      throw badRequest(`${key} must be a real YYYY-MM-DD date`);
    }
    if (value < "2000-01-01" || value > "2199-12-31") throw badRequest(`${key} must be between 2000 and 2199`);
    return value;
  };
}

export function intValue(key: string, min: number, max: number): Parse<number> {
  return (value) => {
    if (typeof value !== "number" || !Number.isInteger(value) || value < min || value > max) {
      throw badRequest(`${key} must be an integer from ${min} to ${max}`);
    }
    return value;
  };
}

export function enumValue<T extends string>(key: string, values: readonly T[]): Parse<T> {
  return (value) => {
    if (typeof value !== "string" || !values.includes(value as T)) {
      throw badRequest(`${key} must be one of ${values.join(", ")}`);
    }
    return value as T;
  };
}

export function boolValue(key: string): Parse<boolean> {
  return (value) => {
    if (typeof value !== "boolean") throw badRequest(`${key} must be true or false`);
    return value;
  };
}

const SUBJECT = /^[A-Za-z0-9._:@-]{1,200}$/;

export function subjectValue(key: string): Parse<string> {
  return (value) => {
    if (typeof value !== "string" || !SUBJECT.test(value)) throw badRequest(`${key} must be a Nexus subject`);
    return value;
  };
}

const UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

export function idValue(key: string): Parse<string> {
  return (value) => {
    if (typeof value !== "string" || !UUID.test(value)) throw badRequest(`${key} must be an id`);
    return value;
  };
}
