import type { ReactNode } from "react";
import { useState } from "react";
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
  user,
  utility,
}: {
  sidebar: ReactNode;
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
  const [mobileNavOpen, setMobileNavOpen] = useState(false);
    return (
      <div className="flex h-screen flex-col bg-[#030303] text-white">
        <header
          role="banner"
          className="flex h-14 shrink-0 items-center gap-3 border-b border-white/10 px-4"
        >

        {/*
            The wordmark is the way home.

            It was a bare span, so once inside an app there was no route back to
            the grid except editing the URL. Making the mark itself the link is
            what every other product does, so it is what people try first.
          */}
            <Link to="/" className="font-semibold tracking-tight hover:text-[#ccff00]" aria-label="Nexus home">

          <button type="button" onClick={() => setMobileNavOpen(!mobileNavOpen)} aria-label={mobileNavOpen ? "Close menu" : "Open menu"} className="md:hidden rounded p-1.5 text-zinc-400 hover:bg-white/10 hover:text-white"><svg width="20" height="20" viewBox="0 0 20 20" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round">{mobileNavOpen ? <path d="M5 5l10 10M15 5L5 15"/> : <path d="M3 5h14M3 10h14M3 15h14"/>}</svg></button>
              Nexus
            </Link>


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
              className={`${utility ? "" : "ml-auto"} rounded-md px-2 py-1 text-sm text-white/60 hover:bg-white/5 hover:text-white`}
            >
              Operator
            </Link>
          )}

          {user && (
            <Link
              to="/account"
                className={`${utility ? "" : "ml-auto"} flex items-center gap-2 rounded-md px-2 py-1 text-sm text-white/60 hover:bg-white/5 hover:text-white`}

              title={user.email}
            >
              <span
                aria-hidden="true"
                                className="flex h-6 w-6 items-center justify-center rounded-full bg-white/10 text-xs font-medium text-white"

              >
                {(user.username || user.email || "?").slice(0, 1).toUpperCase()}
              </span>
              <span className="max-w-[14rem] truncate">{user.username || user.email}</span>
            </Link>
          )}
      </header>

      <div className="flex min-h-0 flex-1">
        {mobileNavOpen && (
          <div className="fixed inset-0 z-30 bg-black/60 md:hidden" onClick={() => setMobileNavOpen(false)} />
        )}
        <aside
          aria-label="Applications"
          className={
            mobileNavOpen
              ? "fixed inset-y-0 left-0 z-40 w-64 bg-[#0a0a0a] block overflow-y-auto border-r border-white/10"
              : "hidden md:block w-60 shrink-0 overflow-y-auto border-r border-white/10"
          }
          onClick={(e) => { const t = e.target as HTMLElement; if (t.closest("a")) setMobileNavOpen(false); }}
        >
          {sidebar}

          {/*
            Reporting lives in the chrome rather than the app list: it is about
            the ecosystem, not one of its apps, and has to be reachable from
            wherever someone hits a problem.
          */}
            <div className="mt-2 border-t border-white/10 p-2">
              <Link
                to="/report"
                className="block rounded-md px-3 py-2 text-sm text-white/60 hover:bg-white/5 hover:text-white"
              >
                Report a problem
              </Link>
            </div>

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
