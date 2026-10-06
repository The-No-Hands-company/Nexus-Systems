import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { me, listApps, type Me, type AppEntry } from "../api";
import Shell from "../shell/Shell";
import HealthStrip from "../shell/HealthStrip";
import NotificationBell from "../shell/NotificationBell";
import { Pill } from "../../../../../packages/nexus-design/src/components/ui/pill";
import Today from "./widgets/Today";
import Unread from "./widgets/Unread";
import Activity from "./widgets/Activity";
import Pinned from "./widgets/Pinned";

const AUTH_LOGIN_URL =
  import.meta.env.VITE_AUTH_LOGIN_URL ?? "https://auth.tnhc.dev/login";

/**
 * The root route decides what "home" means.
 *
 * Signed in, home is a dashboard, inside the same chrome as everywhere else.
 * Signed out, it is the way in — and this host stays public precisely so a
 * stranger can reach that. Gating it would leave nowhere to request an account
 * from.
 *
 * This used to render the full app grid — every registered app as a tile,
 * more than half of them offline — while the shell's own sidebar listed them
 * again. That doubled the same directory on one screen and buried the thing a
 * person actually opens Home to see: what changed since they left. The grid
 * still exists (Grid.tsx, reachable from the drawer's "see all" affordance)
 * but it is no longer what "/" renders. Home is now a health strip plus four
 * widgets — Today, Unread, Activity, Pinned — the way a dashboard for a
 * running system should look, with the app directory itself moved into the
 * drawer where it belongs.
 */
export default function Home() {
  const [user, setUser] = useState<Me | null | undefined>(undefined);
  const [apps, setApps] = useState<AppEntry[]>([]);

  useEffect(() => {
    let cancelled = false;
    void me().then((u) => { if (!cancelled) setUser(u); });
    return () => { cancelled = true; };
  }, []);

  useEffect(() => {
    let cancelled = false;
    // Best effort, same as SignedOut below: the health strip degrading to
    // empty is not a reason to keep the rest of the dashboard from rendering.
    void listApps()
      .then((a) => { if (!cancelled) setApps(a); })
      .catch(() => { if (!cancelled) setApps([]); });
    return () => { cancelled = true; };
  }, []);

  if (user === undefined) {
    return <section className="mx-auto max-w-4xl p-8 text-zinc-500">Loading…</section>;
  }

  // Chrome only when signed in. Wrapping the signed-out page would advertise a
  // launcher to someone with no session and nothing to launch. user goes too —
  // without it the home header had no identity chip and no Operator link, so
  // the founder landed here and /admin might as well not have existed.
  if (user) {
    return (
      <Shell apps={apps} user={user} utility={<NotificationBell />}>
        <div className="flex flex-col gap-4 p-4">
          <HealthStrip apps={apps} />
          <div className="grid grid-cols-1 gap-4 md:grid-cols-2 xl:grid-cols-3">
            <div className="xl:row-span-2"><Today /></div>
            <Unread />
            <Activity />
            <div className="md:col-span-2"><Pinned /></div>
          </div>
        </div>
      </Shell>
    );
  }

  return <SignedOut />;
}

/**
 * The front door.
 *
 * This was a max-w-xl section pinned to the top-left of an otherwise empty
 * black viewport: a heading, three buttons, one line of explanation, and about
 * eighty-five percent dead space. It carried no header, no footer and no
 * semantic landmarks at all — a screen reader found no header, no main and no
 * nav — and it offered no route back to tnhc.dev, so arriving here from the
 * marketing site was a one-way trip.
 *
 * It also asked a stranger to request an account without showing them a single
 * thing they would get. The ecosystem's own registry is public at /api/apps, so
 * there is no reason to describe the product in the abstract when it can list
 * what is actually running, right now, with live health.
 */
function SignedOut() {
  const [apps, setApps] = useState<AppEntry[] | null>(null);

  useEffect(() => {
    let cancelled = false;
    // Best effort. The door must open even if the registry is unreachable —
    // this is decoration around the sign-in, never a precondition for it.
    void listApps()
      .then((a) => { if (!cancelled) setApps(a); })
      .catch(() => { if (!cancelled) setApps([]); });
    return () => { cancelled = true; };
  }, []);

  const healthy = (apps ?? []).filter((a) => a.health === "healthy");

  return (
    <div className="flex min-h-screen flex-col bg-zinc-900 text-zinc-100">
      <header
        role="banner"
        className="flex h-14 shrink-0 items-center justify-between border-b border-zinc-700 px-6"
      >
        <span className="font-semibold tracking-tight">Nexus</span>
        {/*
          The way back. Someone arriving from tnhc.dev previously had no route
          home short of editing the URL, which is the specific complaint that
          started this rewrite.
        */}
        <a
          href="https://tnhc.dev"
          className="text-sm text-zinc-500 transition-colors hover:text-zinc-100"
        >
          tnhc.dev
        </a>
      </header>

      <main role="main" className="flex flex-1 items-center justify-center px-6 py-16">
        <div className="w-full max-w-3xl">
          <h1 className="text-4xl font-semibold tracking-tight md:text-5xl">
            One account for every app in the ecosystem.
          </h1>
          <p className="mt-5 max-w-2xl text-lg leading-relaxed text-zinc-500">
            Sign in once and every Nexus app knows who you are — mail, chat,
            drawing, hosting, the cloud console. No separate passwords, no app
            asking you to register again.
          </p>

          <div className="mt-9 flex flex-wrap gap-3">
            <a
              href={`${AUTH_LOGIN_URL}?redirect_uri=${encodeURIComponent(window.location.origin)}`}
              className="rounded-md bg-accent px-6 py-3 font-medium text-accent-foreground hover:bg-accent-hover active:bg-accent-active"
            >
              Sign in
            </a>
            <Link
              to="/request"
              className="rounded-md border border-zinc-700 px-6 py-3 hover:bg-zinc-800"
            >
              Request access
            </Link>
            <Link
              to="/claim"
              className="rounded-md border border-zinc-700 px-6 py-3 hover:bg-zinc-800"
            >
              Claim your account
            </Link>
          </div>

          <p className="mt-5 max-w-2xl text-sm leading-relaxed text-zinc-500">
            Access is invite-only for now. Request an account and you will be
            given a claim code to save — there is no confirmation email, by
            design: this node cannot send mail off-network, so nothing here
            depends on an inbox you would have to wait for.
          </p>

          {healthy.length > 0 && (
            <section aria-label="Apps running now" className="mt-14">
              <h2 className="font-mono text-[11px] uppercase tracking-[0.2em] text-zinc-500">
                Running right now
              </h2>
              <ul className="mt-4 grid gap-3 sm:grid-cols-2 lg:grid-cols-3">
                {healthy.map((app) => (
                  <li
                    key={app.id}
                    className="rounded-lg border border-zinc-700 bg-zinc-800/40 p-4"
                  >
                    <Pill tone="success">{app.name}</Pill>
                    <p className="mt-1.5 line-clamp-2 text-xs leading-relaxed text-zinc-500">
                      {app.description}
                    </p>
                  </li>
                ))}
              </ul>
              <p className="mt-4 text-xs text-zinc-500">
                Live health, read from the registry when this page loaded — not a
                list someone maintains by hand.
              </p>
            </section>
          )}
        </div>
      </main>

      <footer
        role="contentinfo"
        className="shrink-0 border-t border-zinc-700 px-6 py-5"
      >
        <nav aria-label="Elsewhere" className="flex flex-wrap gap-x-6 gap-y-2 text-sm text-zinc-500">
          <a href="https://tnhc.dev" className="hover:text-zinc-100">Home</a>
          <a href="https://tnhc.dev/apps" className="hover:text-zinc-100">All apps</a>
          <a href="https://tnhc.dev/api" className="hover:text-zinc-100">API</a>
          <a href="https://tnhc.dev/changelog" className="hover:text-zinc-100">Changelog</a>
          <a href="https://tnhc.dev/charter" className="hover:text-zinc-100">Charter</a>
          <a
            href="https://github.com/The-No-Hands-company/Nexus-Systems/issues"
            target="_blank"
            rel="noreferrer noopener"
            className="hover:text-zinc-100"
          >
            Report a problem
          </a>
        </nav>
      </footer>
    </div>
  );
}
