/**
 * IP ban middleware.
 *
 * Checks the requesting IP against the ip_bans table.
 * Supports both exact IP matches and CIDR-range subnet bans.
 * Blocked IPs receive a 403 with a minimal response (no stack traces).
 *
 * Bans are cached in-memory for 60 seconds. Cache is invalidated
 * immediately on ban/unban via invalidateBanCache().
 */

import { Request, Response, NextFunction } from "express";
import { db } from "@workspace/db";
import { ipBansTable } from "@workspace/db";
import { and, isNull, or, gt } from "drizzle-orm";

// Simple in-memory cache: ip → { banned: bool, scope: string, cachedAt: number }
const banCache = new Map<string, { banned: boolean; scope: string; cachedAt: number }>();
const CACHE_TTL_MS = 60_000;

export function getClientIp(req: Request): string {
  const forwarded = req.headers["x-forwarded-for"];
  if (typeof forwarded === "string") return forwarded.split(",")[0].trim();
  return req.socket?.remoteAddress ?? "unknown";
}

/** Convert IPv4 CIDR string to a numeric range check function. */
function makeCidrChecker(cidr: string): (ip: string) => boolean {
  try {
    const [base, prefixStr] = cidr.split("/");
    const prefix = parseInt(prefixStr ?? "32", 10);
    const baseNum = ipToNum(base!);
    const mask = prefix === 0 ? 0 : (~0 << (32 - prefix)) >>> 0;
    const network = (baseNum & mask) >>> 0;
    return (ip: string) => {
      try {
        return ((ipToNum(ip) & mask) >>> 0) === network;
      } catch {
        return false;
      }
    };
  } catch {
    return () => false;
  }
}

function ipToNum(ip: string): number {
  const parts = ip.split(".").map(Number);
  if (parts.length !== 4 || parts.some(isNaN)) throw new Error("Invalid IPv4");
  return ((parts[0]! << 24) | (parts[1]! << 16) | (parts[2]! << 8) | parts[3]!) >>> 0;
}

/** Fetch all active bans and check the given IP against exact + CIDR matches. */
async function isIpBanned(ip: string, scope: "api" | "sites"): Promise<boolean> {
  const cached = banCache.get(ip);
  if (cached && Date.now() - cached.cachedAt < CACHE_TTL_MS) {
    if (!cached.banned) return false;
    return cached.scope === "all" || cached.scope === scope;
  }

  const now = new Date();
  const activeBans = await db
    .select({ ipAddress: ipBansTable.ipAddress, cidrRange: ipBansTable.cidrRange, scope: ipBansTable.scope })
    .from(ipBansTable)
    .where(and(
      or(isNull(ipBansTable.expiresAt), gt(ipBansTable.expiresAt, now)),
    ));

  // Check exact match first, then CIDR
  let matchedScope: string | null = null;
  for (const ban of activeBans) {
    const scopeMatch = ban.scope === "all" || ban.scope === scope;
    if (!scopeMatch) continue;

    if (ban.ipAddress === ip) {
      matchedScope = ban.scope;
      break;
    }
    if (ban.cidrRange && makeCidrChecker(ban.cidrRange)(ip)) {
      matchedScope = ban.scope;
      break;
    }
  }

  const result = matchedScope
    ? { banned: true,  scope: matchedScope,  cachedAt: Date.now() }
    : { banned: false, scope: "",            cachedAt: Date.now() };

  banCache.set(ip, result);
  return result.banned;
}

/** Middleware: block banned IPs from the API. */
export function apiBanMiddleware(req: Request, res: Response, next: NextFunction): void {
  const ip = getClientIp(req);
  isIpBanned(ip, "api").then((banned) => {
    if (banned) {
      res.status(403).json({ error: "Access denied.", code: "IP_BANNED" });
    } else {
      next();
    }
  }).catch(() => next()); // Fail open on DB error
}

/** Middleware: block banned IPs from viewing hosted sites. */
export function siteBanMiddleware(req: Request, res: Response, next: NextFunction): void {
  const ip = getClientIp(req);
  isIpBanned(ip, "sites").then((banned) => {
    if (banned) {
      res.status(403).send("Access denied.");
    } else {
      next();
    }
  }).catch(() => next());
}

/** Invalidate the ban cache for a specific IP (call after ban/unban). */
export function invalidateBanCache(ip?: string): void {
  if (ip) {
    banCache.delete(ip);
  } else {
    banCache.clear(); // Full flush — used when a CIDR ban is added
  }
}
