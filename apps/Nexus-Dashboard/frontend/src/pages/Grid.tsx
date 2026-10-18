import { useEffect, useMemo, useState } from "react";
import { Link } from "react-router-dom";
import { listApps, type AppEntry } from "../api";
import { CATALOG_SHELVES, groupAppsByShelf, isLauncherApp, searchCatalog, type CatalogAudience, type CatalogShelf } from "../shell/catalog";
import AppCard from "../components/AppCard";
import AppShelf from "../components/AppShelf";
import CatalogFilters from "../components/CatalogFilters";

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
  const [query, setQuery] = useState("");
  const [shelf, setShelf] = useState<CatalogShelf | "all">("all");
  const [audience, setAudience] = useState<CatalogAudience | "all">("all");
  const [capability, setCapability] = useState<string | "all">("all");

  useEffect(() => {
    let cancelled = false;
    listApps()
      .then((apps) => { if (!cancelled) setState({ kind: "ready", apps }); })
      .catch(() => { if (!cancelled) setState({ kind: "error" }); });
    return () => { cancelled = true; };
  }, []);

  // Keep all hooks above the loading/error branches: the registry response is
  // asynchronous, so rendering a spinner must not change hook order.
  const apps = useMemo(() => state.kind === "ready" ? state.apps.filter(isLauncherApp) : [], [state]);
  const capabilities = useMemo(() => [...new Set(apps.flatMap((app) => app.capabilities ?? []))].sort(), [apps]);
  const hasFilters = Boolean(query.trim()) || shelf !== "all" || audience !== "all" || capability !== "all";
  const matches = useMemo(
    () => searchCatalog(apps, query, { shelf, audience, capability }),
    [audience, capability, query, shelf, apps],
  );
  const shelves = useMemo(() => groupAppsByShelf(apps), [apps]);

  if (state.kind === "loading") {
    return <section className="mx-auto max-w-4xl px-4 py-6 sm:p-8 text-zinc-500">Loading…</section>;
  }

  // Distinct from an empty grid on purpose: "you have no apps" and "we could
  // not ask" are different facts and must not look the same.
  if (state.kind === "error") {
    return (
      <section className="mx-auto max-w-4xl px-4 py-6 sm:p-8">
        <h1 className="text-2xl font-semibold">Your apps</h1>
        <p role="alert" className="mt-4 text-red-400">
          Could not load your apps. Try again in a moment.
        </p>
      </section>
    );
  }

  if (apps.length === 0) {
    return (
      <section className="mx-auto max-w-4xl px-4 py-6 sm:p-8">
        <h1 className="text-2xl font-semibold">Your apps</h1>
        <p className="mt-4 text-zinc-400">No apps are reachable right now.</p>
      </section>
    );
  }

  return (
    <section className="mx-auto max-w-6xl px-4 py-6 sm:p-8">
      <div className="mb-6 flex flex-wrap items-end justify-between gap-3">
        <div>
          <h1 className="text-2xl font-semibold">App directory</h1>
          <p className="mt-1 text-sm text-zinc-400">Browse the ecosystem by what you want to do.</p>
        </div>
        <Link to="/" className="text-sm text-accent hover:underline focus-visible:outline-2 focus-visible:outline-accent">Back to dashboard</Link>
      </div>

      <CatalogFilters
        query={query}
        shelf={shelf}
        audience={audience}
        capability={capability}
        capabilities={capabilities}
        onQueryChange={setQuery}
        onShelfChange={setShelf}
        onAudienceChange={setAudience}
        onCapabilityChange={setCapability}
      />

      {hasFilters ? (
        <section className="mt-8" aria-labelledby="search-results-heading">
          <h2 id="search-results-heading" className="mb-3 text-[11px] font-semibold uppercase tracking-widest text-zinc-500">Search results</h2>
          {matches.length === 0 ? <p className="text-sm text-zinc-500">No apps match those filters.</p> : <ul className="grid gap-3 sm:grid-cols-2 lg:grid-cols-3">{matches.map((app) => <li key={app.id}><AppCard app={app} showShelf /></li>)}</ul>}
        </section>
      ) : (
        <div className="mt-8 space-y-10">
          {CATALOG_SHELVES.map((key) => <AppShelf key={key} shelf={key} apps={shelves[key]} limit={6} />)}
        </div>
      )}
    </section>
  );
}
