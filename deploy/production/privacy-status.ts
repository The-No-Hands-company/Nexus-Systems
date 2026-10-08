// Publishes the daily privacy check to anyone — but only the parts that are
// safe to publish. The raw result names the test address and where a finding
// was; this keeps the verdict, the time and how much was searched, nothing else.
import { readFile } from "node:fs/promises";

export type PublicPrivacyStatus = {
  status: "pass" | "fail" | "stale";
  checkedAt?: string;
  searched?: { databases: number; containers: number; logs: number; files: number; probes: number };
  findings: number;
};

const STALE_AFTER_MS = 36 * 60 * 60 * 1000;
const STALE: PublicPrivacyStatus = { status: "stale", findings: 0 };
const n = (v: unknown) => (typeof v === "number" && Number.isFinite(v) ? v : 0);

export async function privacyStatusBody(
  path: string = process.env.PRIVACY_CANARY_RESULT || "/tmp/nexus-production/privacy-canary.json",
  now: number = Date.now(),
): Promise<PublicPrivacyStatus> {
  let raw: any;
  try {
    raw = JSON.parse(await readFile(path, "utf8"));
  } catch {
    return STALE;
  }
  const at = typeof raw?.at === "string" ? Date.parse(raw.at) : NaN;
  if (!Number.isFinite(at) || at - now > 5 * 60 * 1000 || now - at > STALE_AFTER_MS) return STALE;
  if (raw.status !== "pass" && raw.status !== "fail") return STALE;
  const s = raw.sources ?? {};
  return {
    status: raw.status,
    checkedAt: new Date(at).toISOString().replace(".000Z", "Z"),
    searched: { databases: n(s.databases), containers: n(s.containers), logs: n(s.logs), files: n(s.files), probes: n(s.probes_answered) },
    // Entries starting "error:" mean a place could not be checked, not that the marker was found.
    findings: Array.isArray(raw.found) ? raw.found.filter((f: unknown) => !(typeof f === "string" && f.startsWith("error:"))).length : 0,
  };
}
