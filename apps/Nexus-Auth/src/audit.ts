/**
 * Structured audit logging.
 *
 * Fire-and-forget: writes are async, never block a request, and never crash
 * Auth if the database is unreachable. Events buffer in memory (bounded) and
 * drain on reconnect.
 *
 * Uses dynamic import for pg so the module loads cleanly in test environments
 * where pg isn't installed — audit becomes a no-op rather than a crash.
 */
export type AuditEvent =
  | "login_success" | "login_failure" | "login_rate_limited"
  | "logout" | "password_change"
  | "role_change" | "status_change"
  | "session_revoked"
  | "recovery_codes_regenerated" | "recovery_code_used"
  | "api_key_created" | "api_key_revoked";

export interface AuditEntry {
  event: AuditEvent;
  userId?: string;
  actorId?: string;
  deviceId?: string;
  detail?: Record<string, unknown>;
}

const MAX_BUFFER = 1000;
const buffer: AuditEntry[] = [];
let flushFn: ((entries: AuditEntry[]) => Promise<void>) | null = null;
let queryFn: ((sql: string, params: unknown[]) => Promise<any[]>) | null = null;
let initAttempted = false;

let warned = false;
function warnOnce(err: unknown): void {
  if (warned) return;
  warned = true;
  // Technical line only: never the URL or any credential.
  console.error(`[audit] database unavailable: ${(err as Error)?.name ?? "Error"}`);
}

async function init(): Promise<void> {
  if (initAttempted) return;
  initAttempted = true;
  const url = process.env.NEXUS_AUTH_AUDIT_DATABASE_URL || process.env.DATABASE_URL;
  if (!url) return; // no database configured (tests, dev): audit is a no-op.
  try {
    // Bun's built-in Postgres client: no extra dependency to go missing.
    const { SQL } = await import("bun");
    const sql = new SQL(url, { max: 2, idleTimeout: 30, connectionTimeout: 5 });
    queryFn = async (text, params) => {
      try {
        return Array.from(await sql.unsafe(text, params as any[]));
      } catch (err) {
        warnOnce(err);
        throw err;
      }
    };
    flushFn = async (entries) => {
      try {
        for (const e of entries) {
          await sql`INSERT INTO auth_audit_log (event, user_id, actor_id, device_id, detail)
                    VALUES (${e.event}, ${e.userId ?? null}, ${e.actorId ?? null}, ${e.deviceId ?? null}, ${JSON.stringify(e.detail ?? {})}::jsonb)`;
        }
      } catch (err) {
        warnOnce(err);
        throw err;
      }
    };
  } catch (err) {
    warnOnce(err);
  }
}

export function audit(entry: AuditEntry): void {
  buffer.push(entry);
  if (buffer.length >= 10) void drain();
}

export async function drain(): Promise<void> {
  if (buffer.length === 0) return;
  await init();
  if (!flushFn) { buffer.length = 0; return; }
  const batch = buffer.splice(0);
  try {
    await flushFn(batch);
  } catch {
    buffer.unshift(...batch.slice(-MAX_BUFFER));
    if (buffer.length > MAX_BUFFER) buffer.splice(0, buffer.length - MAX_BUFFER);
  }
}

export async function closeAudit(): Promise<void> {
  await drain();
}

/** The caller's own events from the last 30 days, newest first. [] when no DB. */
export async function recentActivity(
  userId: string,
): Promise<{ event: string; deviceId: string | null; at: string }[]> {
  await init();
  if (!queryFn) return [];
  try {
    const rows = await queryFn(
      `SELECT event, device_id, created_at FROM auth_audit_log
       WHERE user_id = $1 AND created_at > now() - interval '30 days'
       ORDER BY created_at DESC LIMIT 200`,
      [userId],
    );
    return rows.map((r) => ({
      event: r.event,
      deviceId: r.device_id ?? null,
      at: new Date(r.created_at).toISOString(),
    }));
  } catch {
    return [];
  }
}

/** Retention: nothing older than 30 days is kept. */
export async function purgeOld(): Promise<void> {
  await init();
  if (!queryFn) return;
  try {
    await queryFn(`DELETE FROM auth_audit_log WHERE created_at < now() - interval '30 days'`, []);
  } catch {
    // DB unreachable — try again on the next tick.
  }
}

/** Test seam: inject in-memory flush/query so audit can be exercised without Postgres. */
export function setAuditBackendForTests(b: {
  flush: (entries: AuditEntry[]) => Promise<void>;
  query: (sql: string, params: unknown[]) => Promise<any[]>;
} | null): void {
  initAttempted = b !== null;
  flushFn = b?.flush ?? null;
  queryFn = b?.query ?? null;
}

void purgeOld();
// A lone event must reach the DB soon, not wait for nine more.
setInterval(() => void drain(), 5000).unref();
setInterval(() => void purgeOld(), 6 * 60 * 60 * 1000).unref();
