import type { ReactNode } from "react";

/**
 * The frame every app renders inside.
 *
 * Regions are named for the doctrine in docs/noname.md: app-header,
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
    <div className="flex h-screen flex-col bg-bg-canvas text-text-primary">
      <header
        role="banner"
        className="flex h-14 shrink-0 items-center gap-3 border-b border-border-subtle px-4"
      >
        <span className="font-semibold tracking-tight">Nexus</span>
      </header>

      <div className="flex min-h-0 flex-1">
        <aside className="w-60 shrink-0 overflow-y-auto border-r border-border-subtle">
          {sidebar}
        </aside>

        {/* The app mounts here and nowhere else. */}
        <main role="main" className="min-w-0 flex-1 overflow-hidden">
          {children}
        </main>
      </div>
    </div>
  );
}
