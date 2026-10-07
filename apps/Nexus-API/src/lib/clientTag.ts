/**
 * The only thing this service knows about who is calling.
 *
 * The ecosystem proxy strips every address header before a request reaches us
 * and sets `x-nexus-client-tag`: an opaque 22-character tag that is only good
 * for telling concurrent clients apart (rate limits, short-lived bans). It is
 * not an address, cannot be reversed to one, and is never written to disk by
 * this service. Requests that arrive without one share the "unknown" bucket.
 */
import type { Request } from "express";

export const UNKNOWN_CLIENT_TAG = "unknown";
const TAG_RE = /^[A-Za-z0-9_-]{22}$/;

export function clientTag(req: Pick<Request, "headers">): string {
  const raw = req.headers["x-nexus-client-tag"];
  const value = Array.isArray(raw) ? raw[0] : raw;
  return typeof value === "string" && TAG_RE.test(value) ? value : UNKNOWN_CLIENT_TAG;
}

/** express-rate-limit keyGenerator: one bucket per client tag. */
export function clientTagKey(req: Request): string {
  return clientTag(req);
}
