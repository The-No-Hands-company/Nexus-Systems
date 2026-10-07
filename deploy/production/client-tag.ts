// The proxy is the only place a visitor's address is ever read. It is turned
// into a tag that lets services count attempts without knowing who or where
// anyone is, and the secret behind the tag lives only in this process's memory
// and is replaced every 24 hours — after which no one, including TNHC, can map
// an old tag back to an address.
import { createHmac, randomBytes } from "node:crypto";

export const CLIENT_TAG_HEADER = "x-nexus-client-tag";
export const ADDRESS_HEADERS: readonly string[] = [
  "cf-connecting-ip", "cf-connecting-ipv6", "cf-pseudo-ipv4", "x-forwarded-for", "x-real-ip",
  "true-client-ip", "cf-ipcountry", "cf-ray", "forwarded", "x-client-ip", "cf-visitor",
  "cf-ew-via", "cdn-loop",
];
const ROTATE_MS = 24 * 60 * 60 * 1000;

let secret = randomBytes(32);
let rotatedAt = Date.now();

function current(now: number): Buffer {
  if (now - rotatedAt > ROTATE_MS) {
    secret = randomBytes(32);
    rotatedAt = now;
  }
  return secret;
}

export function clientTag(address: string | null, now: number = Date.now()): string {
  if (!address) return "unknown";
  return createHmac("sha256", current(now)).update(address).digest("base64url").slice(0, 22);
}

export function stripAddressHeaders(h: Headers): void {
  for (const name of ADDRESS_HEADERS) h.delete(name);
}

export function tagFromRequestHeaders(h: Headers): string {
  return clientTag(h.get("cf-connecting-ip"));
}

export function __rotateForTest(): void {
  secret = randomBytes(32);
  rotatedAt = Date.now();
}
