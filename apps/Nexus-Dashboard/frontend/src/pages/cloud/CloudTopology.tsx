import { useEffect, useState, useMemo } from "react";
import { cloudTopology, type CloudTopologyData } from "../../api";
import CloudNav from "./CloudNav";

type State = { kind: "loading" } | { kind: "unavailable" } | { kind: "ready"; topology: CloudTopologyData };

export default function CloudTopology() {
  const [state, setState] = useState<State>({ kind: "loading" });
  const [filterMode, setFilterMode] = useState<string>("all");
  const [search, setSearch] = useState<string>("");

  useEffect(() => {
    let cancelled = false;
    void cloudTopology()
      .then((topology) => { if (!cancelled) setState({ kind: "ready", topology }); })
      .catch(() => { if (!cancelled) setState({ kind: "unavailable" }); });
    return () => { cancelled = true; };
  }, []);

  const filteredApps = useMemo(() => {
    if (state.kind !== "ready") return [];
    return state.topology.apps.filter((app) => {
      const matchesSearch = app.name.toLowerCase().includes(search.toLowerCase()) ||
        app.id.toLowerCase().includes(search.toLowerCase()) ||
        app.kind?.toLowerCase().includes(search.toLowerCase());
      if (!matchesSearch) return false;
      if (filterMode === "all") return true;
      return app.kind === filterMode || app.integrationMode === filterMode;
    });
  }, [state, filterMode, search]);

  return (
    <section className="mx-auto max-w-6xl space-y-6 px-4 py-6 sm:p-8">
      <CloudNav />
      <div>
        <h1 className="text-2xl font-semibold text-zinc-100">Ecosystem Topology Visualizer</h1>
        <p className="mt-1 text-sm text-zinc-400">
          Canonical service mesh dependency graph, API cross-bindings, and runtime topologies.
        </p>
      </div>

      {state.kind === "loading" && <p role="status" className="text-zinc-500">Loading topology graph…</p>}

      {state.kind === "unavailable" && (
        <p role="alert" className="text-rose-400">
          Cloud topology service is temporarily unavailable.
        </p>
      )}

      {state.kind === "ready" && (
        <div className="space-y-6">
          {/* Summary KPIs */}
          <div className="grid grid-cols-2 gap-4 sm:grid-cols-4">
            <div className="rounded-xl border border-zinc-800 bg-zinc-900/60 p-4">
              <span className="text-xs uppercase text-zinc-500">Total Apps</span>
              <div className="mt-1 text-2xl font-semibold text-zinc-100">
                {state.topology.summary.appCount}
              </div>
            </div>
            <div className="rounded-xl border border-zinc-800 bg-zinc-900/60 p-4">
              <span className="text-xs uppercase text-zinc-500">Cross Connections</span>
              <div className="mt-1 text-2xl font-semibold text-emerald-400">
                {state.topology.summary.connectionCount}
              </div>
            </div>
            <div className="rounded-xl border border-zinc-800 bg-zinc-900/60 p-4">
              <span className="text-xs uppercase text-zinc-500">Referenced Nodes</span>
              <div className="mt-1 text-2xl font-semibold text-cyan-400">
                {state.topology.summary.referencedAppCount}
              </div>
            </div>
            <div className="rounded-xl border border-zinc-800 bg-zinc-900/60 p-4">
              <span className="text-xs uppercase text-zinc-500">Embedded Units</span>
              <div className="mt-1 text-2xl font-semibold text-amber-400">
                {state.topology.summary.embeddedAppCount}
              </div>
            </div>
          </div>

          {/* Controls */}
          <div className="flex flex-wrap items-center justify-between gap-3">
            <input
              type="text"
              placeholder="Search services or capabilities…"
              value={search}
              onChange={(e) => setSearch(e.target.value)}
              className="rounded-lg border border-zinc-800 bg-zinc-900 px-3 py-1.5 text-sm text-zinc-200 placeholder-zinc-500 focus:outline-none focus:ring-1 focus:ring-cyan-500"
            />
            <div className="flex flex-wrap gap-1">
              {["all", "platform", "standalone", "embedded", "referenced"].map((mode) => (
                <button
                  key={mode}
                  type="button"
                  onClick={() => setFilterMode(mode)}
                  className={`rounded-lg px-2.5 py-1 text-xs capitalize ${
                    filterMode === mode ? "bg-zinc-700 text-zinc-100" : "bg-zinc-900 text-zinc-400 hover:bg-zinc-800"
                  }`}
                >
                  {mode}
                </button>
              ))}
            </div>
          </div>

          {/* Topology Node Matrix */}
          <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3">
            {filteredApps.map((app) => (
              <div
                key={app.id}
                className="flex flex-col justify-between rounded-xl border border-zinc-800 bg-zinc-900/80 p-4 transition-colors hover:border-zinc-700"
              >
                <div>
                  <div className="flex items-center justify-between">
                    <span className="font-semibold text-zinc-100">{app.name}</span>
                    <span className="rounded bg-zinc-800 px-2 py-0.5 font-mono text-[11px] text-zinc-400">
                      {app.kind || "app"}
                    </span>
                  </div>
                  <div className="mt-2 space-y-1.5 text-xs text-zinc-400">
                    <div>
                      <span className="font-medium text-zinc-500">Mode: </span>
                      <span className="text-zinc-300">{app.integrationMode || "standard"}</span>
                    </div>
                    {app.exposes && app.exposes.length > 0 && (
                      <div>
                        <span className="font-medium text-zinc-500">Exposes: </span>
                        <span className="text-emerald-400">{app.exposes.join(", ")}</span>
                      </div>
                    )}
                    {app.consumes && app.consumes.length > 0 && (
                      <div>
                        <span className="font-medium text-zinc-500">Consumes: </span>
                        <span className="text-zinc-400">{app.consumes.join(", ")}</span>
                      </div>
                    )}
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}
    </section>
  );
}
