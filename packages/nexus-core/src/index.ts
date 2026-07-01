// ─── @nexus/core — Barrel Exports ─────────────────────────────────────────
// Every Nexus app imports from here: import { createNexusServer, ... } from "@nexus/core";

// Types
export type {
  LogLevel,
  LogEntry,
  ValidationResult,
  SecurityHeaders,
  ServerHandle,
  ServerOptions,
} from "./types";

// Logging
export { createLogger } from "./logging";
export type { Logger } from "./logging";

// HTTP helpers
export {
  SECURITY_HEADERS,
  sendJson,
  sendError,
  readJsonBody,
  readRawBody,
  parseMultipartBoundary,
  parseMultipart,
  detectContentType,
} from "./http";
export type { MultipartPart } from "./http";

// Validation
export {
  isString,
  isNonEmptyString,
  isNumber,
  isInteger,
  isPositiveInteger,
  isNonNegativeInteger,
  isBoolean,
  isIsoTimestamp,
  isObject,
  isStringArray,
  isOneOf,
} from "./validation";

// Server factory
export { createNexusServer } from "./server";
export type { NexusRequestContext, NexusRouteHandler, NexusServerHandle } from "./server";
