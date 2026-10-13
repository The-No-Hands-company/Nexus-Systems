/**
 * Platform mail through Nexus-Email: invitations and approval notices.
 *
 * Auth holds no mail credentials and talks to no mail provider. It asks
 * Nexus-Email's loopback-only system-mail endpoint to send a fixed template,
 * and Nexus-Email delivers it through whatever egress that node uses.
 *
 * Email is an enhancement, never a dependency: every caller already has the
 * link it is sending, and shows it to the operator whatever the outcome here.
 * So this never throws; it reports what happened.
 */

export type SystemMailTemplate = "invite" | "access-approved";

/** sent: accepted for delivery. failed: Nexus-Email unreachable or erroring.
 *  rejected: Nexus-Email refused the message (bad recipient, daily cap).
 *  disabled: no system-mail token configured on this node. */
export type SystemMailDelivery = "sent" | "failed" | "rejected" | "disabled";

const DEFAULT_SYSTEM_MAIL_URL = "http://127.0.0.1:3140";
const SYSTEM_MAIL_TIMEOUT_MS = 5000;

export async function sendSystemMail(message: {
  template: SystemMailTemplate;
  to: string;
  link: string;
  expiresAt?: string;
}): Promise<SystemMailDelivery> {
  // Read at call time, so configuration and tests take effect without a restart.
  const token = process.env.NEXUS_EMAIL_SYSTEM_MAIL_TOKEN?.trim();
  if (!token) return "disabled";
  const base = process.env.NEXUS_EMAIL_SYSTEM_MAIL_URL?.trim() || DEFAULT_SYSTEM_MAIL_URL;

  try {
    const res = await fetch(`${base}/internal/v1/system-mail`, {
      method: "POST",
      headers: { authorization: `Bearer ${token}`, "content-type": "application/json" },
      body: JSON.stringify(message),
      signal: AbortSignal.timeout(SYSTEM_MAIL_TIMEOUT_MS),
    });
    await res.body?.cancel();
    if (res.status === 202) return "sent";
    if (res.status === 503) return "disabled";
    if (res.status === 400 || res.status === 422 || res.status === 429) return "rejected";
    return "failed";
  } catch {
    return "failed";
  }
}

/** Shape check only; Nexus-Email parses the address properly before sending. */
export function isPlausibleEmail(value: string): boolean {
  return value.length <= 254 && /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(value);
}
