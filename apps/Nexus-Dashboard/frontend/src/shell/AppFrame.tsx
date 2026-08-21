import type { AppEntry } from "../api";
import { appById } from "./apps";

/**
 * Add the embed flag to an app's URL.
 *
 * URL rather than string concatenation so an app that already carries a query
 * keeps it, and so asking twice is harmless — the frame re-renders on
 * navigation and must not accumulate parameters.
 */
export function embedUrl(url: string): string {
  const u = new URL(url);
  u.searchParams.set("embed", "1");
  return u.toString();
}

/**
 * Mounts an app inside the shell.
 *
 * No sandbox attribute: these are first-party apps that need scripts, forms,
 * storage and their own origin, so sandboxing would remove nothing an attacker
 * has and break everything the app needs. What actually constrains framing is
 * frame-ancestors on the app's own host.
 *
 * Authentication needs no work here — the session cookie is .tnhc.dev-scoped
 * and the frame is same-site, so the proxy gates and identifies the framed
 * request exactly as it does a direct one.
 */
export default function AppFrame({ apps, appId }: { apps: AppEntry[]; appId: string }) {
  const app = appById(apps, appId);

  if (!app) {
    return (
      <div className="flex h-full flex-col items-center justify-center gap-2 p-8 text-center">
        <p className="text-zinc-100">App not found.</p>
        <p className="text-sm text-zinc-500">
          No app is registered as <code>{appId}</code>.
        </p>
      </div>
    );
  }

  return (
    <iframe
      key={app.id}
      title={app.name}
      src={embedUrl(app.url)}
      className="h-full w-full border-0"
      allow="clipboard-read; clipboard-write; fullscreen"
    />
  );
}
