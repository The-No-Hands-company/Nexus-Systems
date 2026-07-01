// ─── Validation Helpers ───────────────────────────────────────────────────
// Reusable validators for Nexus app endpoints. Each returns null on success
// or an error string describing the first validation failure.

import type { ValidationResult } from "./types";

// ── Primitives ─────────────────────────────────────────────────────────────

export function isString(value: unknown, name: string): ValidationResult {
  if (typeof value !== "string") return `${name} must be a string`;
  return null;
}

export function isNonEmptyString(value: unknown, name: string): ValidationResult {
  const err = isString(value, name);
  if (err) return err;
  if ((value as string).trim() === "") return `${name} must be a non-empty string`;
  return null;
}

export function isNumber(value: unknown, name: string): ValidationResult {
  if (typeof value !== "number" || !Number.isFinite(value)) {
    return `${name} must be a finite number`;
  }
  return null;
}

export function isInteger(value: unknown, name: string): ValidationResult {
  const err = isNumber(value, name);
  if (err) return err;
  if (!Number.isInteger(value)) return `${name} must be an integer`;
  return null;
}

export function isPositiveInteger(value: unknown, name: string): ValidationResult {
  const err = isInteger(value, name);
  if (err) return err;
  if ((value as number) <= 0) return `${name} must be a positive integer`;
  return null;
}

export function isNonNegativeInteger(value: unknown, name: string): ValidationResult {
  const err = isInteger(value, name);
  if (err) return err;
  if ((value as number) < 0) return `${name} must be a non-negative integer`;
  return null;
}

export function isBoolean(value: unknown, name: string): ValidationResult {
  if (typeof value !== "boolean") return `${name} must be a boolean`;
  return null;
}

export function isIsoTimestamp(value: unknown, name: string): ValidationResult {
  const err = isString(value, name);
  if (err) return err;
  if (Number.isNaN(Date.parse(value as string))) {
    return `${name} must be an ISO 8601 timestamp`;
  }
  return null;
}

// ── Object Helpers ────────────────────────────────────────────────────────

export function isObject(value: unknown, name: string): ValidationResult {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return `${name} must be a JSON object`;
  }
  return null;
}

export function isStringArray(value: unknown, name: string): ValidationResult {
  if (!Array.isArray(value)) return `${name} must be an array`;
  if ((value as unknown[]).some((item) => typeof item !== "string")) {
    return `${name} must be an array of strings`;
  }
  return null;
}

// ── Enum Helpers ──────────────────────────────────────────────────────────

export function isOneOf<T extends string>(
  value: unknown,
  name: string,
  allowed: readonly T[],
): ValidationResult {
  const err = isString(value, name);
  if (err) return err;
  if (!allowed.includes(value as T)) {
    return `${name} must be one of: ${allowed.join(", ")}`;
  }
  return null;
}
