import type { ReactNode } from "react";
import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { Avatar } from "../../../../../packages/nexus-design/src/components/ui/avatar";
import { Kbd } from "../../../../../packages/nexus-design/src/components/ui/kbd";
import AppsDrawer from "./AppsDrawer";
import { useDensity } from "./useDensity";
import type { AppEntry } from "../api";

/**
 * The frame every app renders inside.
 *
 * Regions are named for the doctrine in docs/nexus-ui-intelligence-doctrine.md: app-header,
 * app-sidebar, app-content. The utility rail is specified there too and
 * deliberately not built — an empty named region beats an invented purpose.
 *
 * Layout only. It fetches nothing except the density preference (a synchronous
 * localStorage read via useDensity), so it can be rendered in a test without a
 * server and reasoned about without tracing data flow.
 *
 * The rail used to carry the full Launcher app list, duplicating the grid that
 * Home also rendered — half the apps on screen twice, more than half of them
 * offline. AppsDrawer replaces that list: a searchable overlay, opened from the
 * rail or ⌘K, that is the ONE place the app directory lives. The rail keeps
 * only the button that opens it and the report-a-problem link, which belong to
 * the ecosystem chrome rather than to any one app.
 */
export default function Shell({
  apps,
  children,
  user,
  utility,
}: {
  apps: AppEntry[];
  children: ReactNode;
  /**
   * Who is signed in, or absent while unknown.
   *
   * Passed in rather than fetched here, so this component stays layout-only
   * and renderable in a test without a server — the property its own comment
   * above claims and which a fetch would quietly break.
   */
  user?: { username: string; email: string; role?: string } | null;
  /**
   * The header's utility slot — currently the notification bell.
   *
   * A slot rather than the bell itself, because the bell fetches and this
   * component deliberately does not. Passing it in keeps Shell renderable in
   * a test without a server, which is the property the comment above claims.
   */
  utility?: ReactNode;
}) {
  // Stamps data-nexus-density on the document element as a side effect, so the
  // density-scoped tokens in nexus-tokens.css apply everywhere, not just below
  // this component.
  useDensity();

  const [drawerOpen, setDrawerOpen] = useState(false);

  // ⌘K / Ctrl+K opens the apps drawer from anywhere in the shell. preventDefault
  // is required, not decorative: without it Chrome and Firefox both still open
  // their own in-page find bar on top of the drawer.
  useEffect(() => {
    function onKeyDown(e: KeyboardEvent) {
      if ((e.metaKey || e.ctrlKey) && e.key === "k") {
        e.preventDefault();
        setDrawerOpen(true);
      }
    }
    document.addEventListener("keydown", onKeyDown);
    return () => document.removeEventListener("keydown", onKeyDown);
  }, []);

  return (
    <div className="flex h-screen flex-col bg-zinc-900 text-zinc-100">
      <header
        role="banner"
        className="flex h-14 shrink-0 items-center gap-2 border-b border-zinc-700 px-3 sm:gap-3 sm:px-4"
      >
        {/*
            The wordmark is the way home.

            It was a bare span, so once inside an app there was no route back to
            the grid except editing the URL. Making the mark itself the link is
            what every other product does, so it is what people try first.
          */}
        <Link to="/" className="font-semibold tracking-tight hover:text-accent" aria-label="Nexus home">
          Nexus
        </Link>

        {/*
            The search trigger. Hidden below sm — the rail's Apps button is the
            touch-friendly equivalent at narrow widths, and ⌘K still works
            everywhere regardless of whether this is visible.
          */}
        <button
          type="button"
          onClick={() => setDrawerOpen(true)}
          className="ml-2 hidden items-center gap-2 rounded-md border border-zinc-700 px-3 py-1.5 text-sm text-zinc-400 hover:border-zinc-600 hover:text-zinc-200 sm:flex"
        >
          <svg width="14" height="14" viewBox="0 0 20 20" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" aria-hidden="true">
            <circle cx="9" cy="9" r="6" />
            <path d="M14 14l4 4" />
          </svg>
          Search
          <Kbd>⌘K</Kbd>
        </button>

        {/*
            Who you are, and the way to your account.

            The header carried the wordmark and nothing else, so a signed-in
            user had no confirmation of which account they were using and no
            route to their password, sessions or recovery codes without already
            knowing /account existed. ml-auto rather than justify-between, so
            the wordmark keeps its place when this is absent.
          */}
        {utility && <div className="ml-auto">{utility}</div>}

        {/*
            The operator's way in. /admin existed and worked, but nothing
            linked to it — the only way in was typing the URL blind, so the
            approval queue and invites might as well not have existed. Lives
            in the header rather than the sidebar: it is about the ecosystem,
            not one of its apps.
          */}
        {user && (user.role === "founder" || user.role === "admin") && (
          <Link
            to="/admin"
            className={`${utility ? "" : "ml-auto"} rounded-md px-2 py-1 text-sm text-zinc-400 hover:bg-zinc-800 hover:text-zinc-100`}
          >
            Operator
          </Link>
        )}

        {user && (
          <Link
            to="/account"
            className={`${utility ? "" : "ml-auto"} flex items-center gap-2 rounded-md px-2 py-1 text-sm text-zinc-400 hover:bg-zinc-800 hover:text-zinc-100`}
            title={user.email}
          >
            <Avatar label={user.username || user.email} />
            <span className="hidden max-w-[10rem] truncate sm:inline">{user.username || user.email}</span>
          </Link>
        )}
      </header>

      <div className="flex min-h-0 flex-1">
        {/*
            The rail. It used to carry the full app list (Launcher); now it
            carries only what genuinely belongs to the ecosystem's chrome
            rather than to any one app — the way to open the drawer, and the
            way to report a problem. Always visible, at every width: unlike
            the old Launcher list, two icon buttons never need a mobile
            slide-out to avoid crowding the screen.
          */}
        <nav
          aria-label="Shell"
          className="flex w-14 shrink-0 flex-col items-center gap-1 border-r border-zinc-700 py-2 sm:w-16"
        >
          <button
            type="button"
            onClick={() => setDrawerOpen(true)}
            className="flex flex-col items-center gap-1 rounded-md px-2 py-2 text-[10px] text-zinc-400 hover:bg-zinc-800 hover:text-zinc-100"
          >
            <svg width="18" height="18" viewBox="0 0 20 20" fill="none" stroke="currentColor" strokeWidth="1.6" aria-hidden="true">
              <rect x="3" y="3" width="6" height="6" rx="1" />
              <rect x="11" y="3" width="6" height="6" rx="1" />
              <rect x="3" y="11" width="6" height="6" rx="1" />
              <rect x="11" y="11" width="6" height="6" rx="1" />
            </svg>
            Apps
          </button>

          {/*
            Reporting lives in the chrome rather than the app list: it is about
            the ecosystem, not one of its apps, and has to be reachable from
            wherever someone hits a problem.
          */}
          <div className="mt-auto w-full border-t border-zinc-700 pt-2">
            <Link
              to="/report"
              aria-label="Report a problem"
              title="Report a problem"
              className="flex flex-col items-center gap-1 rounded-md px-2 py-2 text-[10px] text-zinc-400 hover:bg-zinc-800 hover:text-zinc-100"
            >
              <svg width="18" height="18" viewBox="0 0 20 20" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
                <path d="M4 3v14" />
                <path d="M4 4h9l-2 3 2 3H4" />
              </svg>
              Report
            </Link>
          </div>
        </nav>

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

      <AppsDrawer apps={apps} open={drawerOpen} onClose={() => setDrawerOpen(false)} />
    </div>
  );
}
