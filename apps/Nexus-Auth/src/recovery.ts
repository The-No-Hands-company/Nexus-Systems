import { createHash, randomBytes, timingSafeEqual } from "node:crypto";
import { mkdirSync, readFileSync, writeFileSync, renameSync, rmSync, dirname, resolve, join, fileURLToPath } from "./types";

/**
 * Single-use account recovery codes.
 *
 * With no outbound email there is no "reset link", so these are the only
 * self-service way back into an account. Ten are issued at claim time, shown
 * once, and stored as sha256 — 128 bits of entropy each, so a fast hash is
 * correct and scrypt would only slow down legitimate verification.
 */

const CODES_PER_ACCOUNT = 10;

type RecoveryRecord = { userId: string; codeHashes: string[] };
type PersistedRecovery = { version: number; records: RecoveryRecord[]; updatedAt: string };

const records = new Map<string, string[]>();

const stateFilePath = resolve(
  process.env.NEXUS_AUTH_RECOVERY_STORE_PATH ||
    join(dirname(fileURLToPath(import.meta.url)), "..", "data", "auth-recovery.json"),
);

function hashCode(code: string): string {
  return createHash("sha256").update(code).digest("hex");
}

function persist(): void {
  const payload: PersistedRecovery = {
    version: 1,
    records: Array.from(records.entries()).map(([userId, codeHashes]) => ({ userId, codeHashes })),
    updatedAt: new Date().toISOString(),
  };
  const tempPath = `${stateFilePath}.tmp`;
  try {
    mkdirSync(dirname(stateFilePath), { recursive: true });
    writeFileSync(tempPath, JSON.stringify(payload, null, 2), "utf-8");
    renameSync(tempPath, stateFilePath);
  } catch {
    try { rmSync(tempPath, { force: true }); } catch {}
  }
}

function hydrate(): void {
  try {
    const parsed = JSON.parse(readFileSync(stateFilePath, "utf-8")) as Partial<PersistedRecovery>;
    records.clear();
    for (const r of Array.isArray(parsed.records) ? parsed.records : []) {
      if (r && typeof r.userId === "string" && Array.isArray(r.codeHashes)) {
        records.set(r.userId, r.codeHashes);
      }
    }
  } catch {
    persist();
  }
}

/** Issues a fresh set of ten codes, discarding any that already existed. */
export function issueRecoveryCodes(userId: string): string[] {
  const codes = Array.from({ length: CODES_PER_ACCOUNT }, () => randomBytes(16).toString("hex"));
  records.set(userId, codes.map(hashCode));
  persist();
  return codes;
}

/** Verifies and burns one code. Returns false for unknown, wrong or spent codes. */
export function consumeRecoveryCode(userId: string, code: string): boolean {
  const hashes = records.get(userId);
  if (!hashes || hashes.length === 0) return false;

  const supplied = Buffer.from(hashCode(code));
  const index = hashes.findIndex((stored) => {
    try {
      return timingSafeEqual(supplied, Buffer.from(stored));
    } catch {
      return false;
    }
  });
  if (index === -1) return false;

  hashes.splice(index, 1);
  records.set(userId, hashes);
  persist();
  return true;
}

export function countRemainingRecoveryCodes(userId: string): number {
  return records.get(userId)?.length ?? 0;
}

export function clearRecoveryStore(): void {
  records.clear();
  persist();
}

hydrate();
