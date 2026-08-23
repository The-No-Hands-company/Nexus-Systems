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

interface AuditEntry {
  event: AuditEvent;
  userId?: string;
  actorId?: string;
  ip?: string;
  userAgent?: string;
  detail?: Record<string, unknown>;
}

const MAX_BUFFER = 1000;
const buffer: AuditEntry[] = [];
let flushFn: ((entries: AuditEntry[]) => Promise<void>) | null = null;
let initAttempted = false;

async function init(): Promise<void> {
  if (initAttempted) return;
  initAttempted = true;
  try {
    const { Pool } = await import("pg");
    const connectionString =
      process.env.NEXUS_AUTH_AUDIT_DATABASE_URL ||
      process.env.DATABASE_URL ||
      "postgresql://127.0.0.1:1/nexus-audit-unreachable";
    const pool = new Pool({ connectionString, max: 2, idleTimeoutMillis: 30_000 });
    pool.on("error", () => {});
    flushFn = async (entries) => {
      const client = await pool.connect();
      try {
        for (const e of entries) {
          await client.query(
            `INSERT INTO auth_audit_log (event, user_id, actor_id, ip, user_agent, detail)
             VALUES ($1, $2, $3, $4, $5, $6)`,
            [e.event, e.userId ?? null, e.actorId ?? null, e.ip ?? null, e.userAgent ?? null, JSON.stringify(e.detail ?? {})],
          );
        }
      } finally {
        client.release();
      }
    };
  } catch {
    // pg not available (test env) — audit is a no-op, which is correct there.
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
