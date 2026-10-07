/**
 * Short-lived, in-memory client-tag bans (admin-only).
 *
 * Replaces the old `ip_bans` table. A ban is never persisted: it lives in this
 * process's memory, is capped at 24 hours (the proxy rotates tags well within
 * that window, so a longer ban would only ever hit an innocent successor), and
 * vanishes on restart. There is no CIDR/subnet notion because there is no
 * address to match.
 */
import { UNKNOWN_CLIENT_TAG } from "./clientTag";

export const MAX_BAN_MS = 24 * 60 * 60 * 1000;
const MAX_ENTRIES = 10_000;

interface TagBan { expiresAt: number; reason: string | null }
const bans = new Map<string, TagBan>();

function sweep(now: number): void {
  for (const [tag, ban] of bans) if (ban.expiresAt <= now) bans.delete(tag);
}

/** Ban a tag for at most 24 h. Returns the expiry (ms since epoch). */
export function banTag(tag: string, ttlMs: number = MAX_BAN_MS, reason: string | null = null): number {
  if (!tag || tag === UNKNOWN_CLIENT_TAG) {
    // "unknown" is every request that arrived without a tag; banning it would
    // lock out everyone, not one client.
    throw new Error("Refusing to ban the shared 'unknown' tag");
  }
  const now = Date.now();
  sweep(now);
  if (!bans.has(tag) && bans.size >= MAX_ENTRIES) throw new Error("Tag ban list is full");
  const ttl = Math.min(Math.max(1, ttlMs), MAX_BAN_MS);
  const expiresAt = now + ttl;
  bans.set(tag, { expiresAt, reason });
  return expiresAt;
}

export function unbanTag(tag: string): boolean {
  return bans.delete(tag);
}

export function isTagBanned(tag: string): boolean {
  const ban = bans.get(tag);
  if (!ban) return false;
  if (ban.expiresAt <= Date.now()) { bans.delete(tag); return false; }
  return true;
}

export function listTagBans(): Array<{ tag: string; reason: string | null; expiresAt: string }> {
  sweep(Date.now());
  return [...bans.entries()].map(([tag, b]) => ({ tag, reason: b.reason, expiresAt: new Date(b.expiresAt).toISOString() }));
}

/** Test hook. */
export function clearTagBans(): void {
  bans.clear();
}
