import type { ReactNode } from "react";
import { Link } from "react-router-dom";

/**
 * The frame every app renders inside.
 *
 * Regions are named for the doctrine in docs/nexus-ui-intelligence-doctrine.md: app-header,
 * app-sidebar, app-content. The utility rail is specified there too and
 * deliberately not built — an empty named region beats an invented purpose.
 *
 * Layout only. It fetches nothing, so it can be rendered in a test without a
 * server and reasoned about without tracing data flow.
 */
export default function Shell({
  sidebar,
  children,
}: {
  sidebar: ReactNode;
  children: ReactNode;
}) {
  return (
    <div className="flex h-screen flex-col bg-zinc-900 text-zinc-100">
      <header
        role="banner"
        className="flex h-14 shrink-0 items-center gap-3 border-b border-zinc-700 px-4"
      >
        <span className="font-semibold tracking-tight">Nexus</span>
      </header>

      <div className="flex min-h-0 flex-1">
        <aside
          aria-label="Applications"
          className="w-60 shrink-0 overflow-y-auto border-r border-zinc-700"
        >
          {sidebar}
        </aside>

        {/*
          The app mounts here and nowhere else.

          overflow-y-auto, not overflow-hidden. Hidden is right for a framed app
          — the iframe is h-full and scrolls internally — but every shell-native
          view (/cloud, /mail, /account, /admin) is an ordinary page, and hidden
          silently clipped them at the fold with no way to reach the rest. The
          Cloud console ended at "Nexus Edge offline" and looked complete.

          The iframe still fills exactly h-full, so this adds no scrollbar to a
          framed app; it only lets taller content scroll.
        */}
        <main role="main" className="min-w-0 flex-1 overflow-y-auto">
          {children}
        </main>
      </div>
    </div>
  );
}
