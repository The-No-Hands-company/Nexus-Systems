// Reads the warrant canary document and reports its freshness for the public
// /canary.json endpoint. The canary text lives in docs/privacy/warrant-canary.md
// so git history is the proof of currency; this module only exposes whether
// it has been updated within the staleness window.
import { readFile } from "node:fs/promises";
import { resolve } from "node:path";

export type PublicCanaryStatus = {
  status: "active" | "stale" | "unavailable";
  issuedAt?: string;
  /** Days until stale (negative means already stale). */
  daysRemaining?: number;
  scope: string;
};

const STALE_AFTER_DAYS = 90;
const SCOPE = "tnhc.dev Nexus Systems services";

function canaryPath(): string {
  return (
    process.env.WARRANT_CANARY_PATH ||
    resolve(import.meta.dir, "../../docs/privacy/warrant-canary.md")
  );
}

function parseIssuedAt(text: string): Date | null {
  // Matches "**Last updated:** YYYY-MM-DD" anywhere in the file.
  const m = text.match(/\*\*Last updated:\*\*\s+(\d{4}-\d{2}-\d{2})/);
  if (!m) return null;
  const d = new Date(m[1] + "T00:00:00Z");
  return Number.isFinite(d.getTime()) ? d : null;
}

export async function canaryStatusBody(
  path: string = canaryPath(),
  now: number = Date.now(),
): Promise<PublicCanaryStatus> {
  let text: string;
  try {
    text = await readFile(path, "utf8");
  } catch {
    return { status: "unavailable", scope: SCOPE };
  }

  const issuedAt = parseIssuedAt(text);
  if (!issuedAt) return { status: "unavailable", scope: SCOPE };

  const issuedMs = issuedAt.getTime();
  // Reject clocks that are more than 24 hours in the future.
  if (issuedMs > now + 24 * 60 * 60 * 1000) return { status: "unavailable", scope: SCOPE };

  const ageMs = now - issuedMs;
  const staleMs = STALE_AFTER_DAYS * 24 * 60 * 60 * 1000;
  const daysRemaining = Math.floor((staleMs - ageMs) / (24 * 60 * 60 * 1000));

  return {
    status: ageMs > staleMs ? "stale" : "active",
    issuedAt: issuedAt.toISOString().replace(".000Z", "Z"),
    daysRemaining,
    scope: SCOPE,
  };
}
