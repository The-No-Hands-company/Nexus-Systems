/** Shared interpretation of the authenticated Dashboard caller. */
export type Caller = {
  subject: string;
  role: string | null;
};

/** Roles permitted to use operator-only Dashboard surfaces. */
export const ADMIN_ROLES = ["founder", "admin"] as const;

export function isAdminRole(role: string | null): boolean {
  return ADMIN_ROLES.some((adminRole) => adminRole === role);
}

function authInternalUrl(): string {
  return (process.env.NEXUS_AUTH_INTERNAL_URL || "http://127.0.0.1:4310").replace(/\/+$/, "");
}

/**
 * Asks Auth to validate the caller's session cookie. The role and stable
 * subject must always come from Auth, never browser-provided headers.
 */
export async function callerIdentity(req: Request): Promise<Caller | null> {
  try {
    const res = await fetch(`${authInternalUrl()}/api/v1/auth/me`, {
      headers: { cookie: req.headers.get("cookie") ?? "" },
      signal: AbortSignal.timeout(3000),
    });
    if (!res.ok) return null;

    const body = (await res.json().catch(() => null)) as {
      user?: { id?: unknown; role?: unknown };
    } | null;
    const id = body?.user?.id;
    if (typeof id !== "string" || !id) return null;

    const role = body?.user?.role;
    return { subject: id, role: typeof role === "string" ? role : null };
  } catch {
    return null;
  }
}
