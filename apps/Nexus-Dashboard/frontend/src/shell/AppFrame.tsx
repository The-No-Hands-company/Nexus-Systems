import { Link } from "react-router-dom";
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
 * External apps (Chat, Deploy, Hosting, Draw, Project, Calendar) are standalone
 * SPAs on their own origins. They are launched in independent browser tabs
 * rather than in an iframe to avoid duplicate renderer threads, GPU context
 * exhaustion, and memory spikes on low-end hardware.
 */
export default function AppFrame({ apps, appId }: { apps: AppEntry[]; appId: string }) {
  const app = appById(apps, appId);

  if (!app) {
    return (
      <div className="flex h-full flex-col items-center justify-center gap-2 p-8 text-center">
        <p className="text-zinc-100">App not found.</p>
        <p className="text-sm text-zinc-500">
          No app is registered as <code>{appId || "(unknown)"}</code>.
        </p>
      </div>
    );
  }

  // Standalone external apps run on their own origin and should never be
  // trapped inside an iframe. Show a dedicated launch screen.
  if (app.delivery === "external" && app.publicUrl) {
    return (
      <div className="flex h-full flex-col items-center justify-center gap-4 p-8 text-center">
        <div className="flex h-16 w-16 items-center justify-center rounded-2xl bg-zinc-800/80 border border-zinc-700/60 shadow-xl">
          <span className="text-xl font-bold text-zinc-100">{app.name.slice(0, 2).toUpperCase()}</span>
        </div>
        <div>
          <h1 className="text-2xl font-semibold text-zinc-100">{app.name}</h1>
          {app.description && (
            <p className="mt-1.5 max-w-md text-sm text-zinc-400">{app.description}</p>
          )}
        </div>
        <div className="mt-2 flex items-center gap-3">
          <a
            href={app.publicUrl}
            target="_blank"
            rel="noopener noreferrer"
            className="flex items-center gap-2 rounded-lg bg-accent px-5 py-2.5 text-sm font-semibold text-accent-foreground shadow-md transition-transform hover:scale-105 active:scale-95 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-accent"
          >
            Launch {app.name} ↗
          </a>
          <Link
            to="/"
            className="rounded-lg border border-zinc-700 px-4 py-2.5 text-sm font-medium text-zinc-300 hover:bg-zinc-800 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-accent"
          >
            Dashboard
          </Link>
        </div>
      </div>
    );
  }

  // Shell-native apps live as React components inside this SPA, not as
  // standalone pages — iframing their own Dashboard path loops back into the
  // shell. Show a status page instead.
  if (app.delivery === "shell-native" && !app.publicUrl) {
    return (
      <div className="flex h-full flex-col items-center justify-center gap-3 p-8 text-center">
        <p className="text-lg font-semibold text-zinc-100">{app.name}</p>
        {app.description && (
          <p className="max-w-sm text-sm text-zinc-500">{app.description}</p>
        )}
        <span
          className={`mt-1 inline-flex items-center gap-1.5 rounded-full px-3 py-1 text-xs font-medium ${
            app.health === "healthy"
              ? "bg-emerald-950 text-emerald-400"
              : "bg-zinc-800 text-zinc-500"
          }`}
        >
          <span
            className={`h-1.5 w-1.5 rounded-full ${
              app.health === "healthy" ? "bg-emerald-400" : "bg-zinc-600"
            }`}
          />
          {app.health === "healthy" ? "Live — opening soon" : "Coming soon"}
        </span>
      </div>
    );
  }

  return (
    <iframe
      key={app.id}
      title={app.name}
      src={embedUrl(app.publicUrl ?? app.path)}
      className="h-full w-full border-0"
      allow="clipboard-read; clipboard-write; fullscreen"
    />
  );
}
