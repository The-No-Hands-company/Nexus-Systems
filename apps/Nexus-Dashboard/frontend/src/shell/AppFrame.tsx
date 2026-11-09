import { Navigate } from "react-router-dom";
import type { AppEntry } from "../api";
import { appById } from "./apps";
import GuidedTutorial from "./GuidedTutorial";

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
 * When an app is opened, AppFrame presents the comprehensive App Launchpad
 * and Interactive Guided Tutorial. Users immediately see:
 * 1. What the app does and why/when to use it
 * 2. 3-step interactive quick start walkthrough
 * 3. Key capabilities and architecture badges
 * 4. Keyboard shortcuts and pro-tips
 * 5. Direct launch action to open the standalone app in a separate tab
 * 6. Deep link to complete documentation in the Platform Docs Hub
 */
export default function AppFrame({ apps, appId }: { apps: AppEntry[]; appId: string }) {
  if (appId === "nexus-docs-hub" || appId === "docs") {
    return <Navigate to="/docs" replace />;
  }

  const app = appById(apps, appId);

  if (app?.path === "/docs") {
    return <Navigate to="/docs" replace />;
  }

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

  // Apps with a publicUrl open their full Launchpad & Guided Tutorial
  if (app.publicUrl) {
    return (
      <div className="h-full overflow-y-auto">
        <GuidedTutorial app={app} />
      </div>
    );
  }

  // Shell-native apps without a dedicated public URL show status badge
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
