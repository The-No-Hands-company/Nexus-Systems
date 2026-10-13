/**
 * Where the ways into the platform live.
 *
 * Requesting access, claiming an approved account and redeeming an invite are
 * served by Nexus-Auth (auth.tnhc.dev/request, /claim, /join), not by this app.
 * This host sits behind the sign-in gate, so a person without an account could
 * never reach a page here; the identity host is the one the gate keeps public.
 */

const AUTH_LOGIN_URL: string = import.meta.env.VITE_AUTH_LOGIN_URL ?? "https://auth.tnhc.dev/login";

export const AUTH_ORIGIN = new URL(AUTH_LOGIN_URL).origin;

export type AuthPagePath = "/request" | "/claim" | "/join";

export function buildAuthPageUrl(path: AuthPagePath, params: Record<string, string> = {}): string {
  const url = new URL(path, AUTH_ORIGIN);
  for (const [key, value] of Object.entries(params)) url.searchParams.set(key, value);
  return url.toString();
}
