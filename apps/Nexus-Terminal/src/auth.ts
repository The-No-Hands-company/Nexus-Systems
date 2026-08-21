/**
 * Who is asking.
 *
 * The terminal hands out a shell on this host, so "is this request
 * authenticated" is the only thing standing between the internet and the box
 * (see docs/TERMINAL-SECURITY.md). It is answered by asking Auth directly with
 * the caller's own cookies — never by trusting a header, a query parameter, or
 * anything else the browser chose to send.
 */

const AUTH_URL = () =>
  (process.env.NEXUS_AUTH_INTERNAL_URL || "http://127.0.0.1:4310").replace(/\/+$/, "");

export type Caller = { subject: string; role: string | null };

export async function callerIdentity(req: Request): Promise<Caller | null> {
  const cookie = req.headers.get("cookie");
  // No cookie is not an error worth a round trip: it cannot be authenticated.
  if (!cookie) return null;

  try {
    const res = await fetch(`${AUTH_URL()}/api/v1/auth/me`, {
      headers: { cookie },
      signal: AbortSignal.timeout(3000),
    });
    if (!res.ok) return null;
    const body = (await res.json().catch(() => null)) as
      | { user?: { id?: unknown; role?: unknown } }
      | null;
    const id = body?.user?.id;
    if (typeof id !== "string" || !id) return null;
    const role = body?.user?.role;
    return { subject: id, role: typeof role === "string" ? role : null };
  } catch {
    // Auth unreachable means we cannot establish who this is, which must fail
    // closed. A terminal that opens when the identity service is down is a
    // terminal that opens for anybody during an outage.
    return null;
  }
}

/**
 * Whether the terminal is switched on at all.
 *
 * Default is off. A feature that hands out host shells should require somebody
 * to have deliberately enabled it, not merely to have deployed the code.
 */
export function terminalEnabled(): boolean {
  return (process.env.NEXUS_TERMINAL_ENABLED || "").toLowerCase() === "true";
}
