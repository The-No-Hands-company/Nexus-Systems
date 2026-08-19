import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { listApps, type AppEntry } from "../api";

type State =
  | { kind: "loading" }
  | { kind: "ready"; apps: AppEntry[] }
  | { kind: "error" };

/**
 * The signed-in home: every app the ecosystem can actually reach.
 *
 * Tiles come from Cloud's registry via the dashboard server, never a hardcoded
 * list — a new app appears by registering itself, and a second node's apps
 * appear with no change here.
 *
 * Clicking through arrives already signed in: the session cookie is scoped to
 * the parent domain, so it travels to every subdomain. That is the whole point
 * of the grid.
 */
export default function Grid() {
  const [state, setState] = useState<State>({ kind: "loading" });

  useEffect(() => {
    let cancelled = false;
    listApps()
      .then((apps) => { if (!cancelled) setState({ kind: "ready", apps }); })
      .catch(() => { if (!cancelled) setState({ kind: "error" }); });
    return () => { cancelled = true; };
  }, []);

  if (state.kind === "loading") {
    return <section className="mx-auto max-w-4xl p-8 text-zinc-500">Loading…</section>;
  }

  // Distinct from an empty grid on purpose: "you have no apps" and "we could
  // not ask" are different facts and must not look the same.
  if (state.kind === "error") {
    return (
      <section className="mx-auto max-w-4xl p-8">
        <h1 className="text-2xl font-semibold">Your apps</h1>
        <p role="alert" className="mt-4 text-red-400">
          Could not load your apps. Try again in a moment.
        </p>
      </section>
    );
  }

  if (state.apps.length === 0) {
    return (
      <section className="mx-auto max-w-4xl p-8">
        <h1 className="text-2xl font-semibold">Your apps</h1>
        <p className="mt-4 text-zinc-400">No apps are reachable right now.</p>
      </section>
    );
  }

  return (
    <section className="mx-auto max-w-4xl p-8">
      <h1 className="text-2xl font-semibold">Your apps</h1>
      <ul className="mt-6 grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3">
        {state.apps.map((app) => (
          <li key={app.id}>
            {app.health === "healthy" ? (
              // Every app is an in-shell route now, so all of them route
              // client-side. A framed app is still framed — that happens at
              // the route, not in this link.
              <Link
                to={app.path}
                className="block h-full rounded-lg border border-zinc-800 bg-zinc-900 p-4 transition-colors hover:border-blue-600"
              >
                <div className="font-medium text-zinc-100">{app.name}</div>
                {app.description && (
                  <div className="mt-1 text-sm text-zinc-400">{app.description}</div>
                )}
              </Link>
            ) : (
              // Rendered as a plain element, not a link: offering a click that
              // goes nowhere is worse than a tile explaining why it cannot.
              <div
                className="h-full cursor-not-allowed rounded-lg border border-zinc-800/60 bg-zinc-900/40 p-4 opacity-60"
                aria-disabled="true"
              >
                <div className="font-medium text-zinc-300">{app.name}</div>
                {app.description && (
                  <div className="mt-1 text-sm text-zinc-500">{app.description}</div>
                )}
                <div className="mt-2 text-xs uppercase tracking-wide text-amber-500">
                  Unavailable
                </div>
              </div>
            )}
          </li>
        ))}
      </ul>
    </section>
  );
}
