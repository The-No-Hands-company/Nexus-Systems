import { useEffect, useState } from "react";
import { cloudEndpoints, type CloudEndpoint } from "../../api";
import CloudNav from "./CloudNav";

type State = { kind: "loading" } | { kind: "unavailable" } | { kind: "ready"; routes: CloudEndpoint[] };

/**
 * status.html's loadApiView grouping, verbatim: the first three path
 * segments key the group unless the path starts with `v1`, in which case two
 * segments do. `/ipa/v1/tools` groups under `/ipa/v1/tools`;
 * `/v1/federation/peers` groups under `/v1/federation`.
 */
function groupKey(path: string): string {
  const parts = path.split("/").filter(Boolean);
  if (parts.length >= 3 && parts[0] !== "v1") return `/${parts.slice(0, 3).join("/")}`;
  if (parts.length >= 2) return `/${parts.slice(0, 2).join("/")}`;
  return path;
}

function groupRoutes(routes: CloudEndpoint[]): Array<[string, CloudEndpoint[]]> {
  const groups = new Map<string, CloudEndpoint[]>();
  for (const r of routes) {
    const key = groupKey(r.path);
    const list = groups.get(key);
    if (list) list.push(r);
    else groups.set(key, [r]);
  }
  return Array.from(groups.entries());
}

const METHOD_CLASS: Record<string, string> = {
  GET: "bg-blue-500/10 text-blue-400",
  POST: "bg-green-500/10 text-green-400",
  PUT: "bg-amber-500/10 text-amber-400",
  PATCH: "bg-amber-500/10 text-amber-400",
  DELETE: "bg-red-500/10 text-red-400",
};

/**
 * The ported `view-api` from Cloud's status.html (loadApiView): every route
 * this node's Systems API exposes, grouped by path prefix.
 */
export default function CloudApi() {
  const [state, setState] = useState<State>({ kind: "loading" });

  useEffect(() => {
    let cancelled = false;
    void cloudEndpoints()
      .then((routes) => { if (!cancelled) setState({ kind: "ready", routes }); })
      .catch(() => { if (!cancelled) setState({ kind: "unavailable" }); });
    return () => { cancelled = true; };
  }, []);

  return (
    <section className="mx-auto max-w-5xl space-y-6 p-8">
      <CloudNav />
      <div>
        <h1 className="text-2xl font-semibold">API</h1>
        <p className="mt-2 text-zinc-400">Every route this node's Systems API exposes.</p>
      </div>

      {state.kind === "loading" && <p role="status" className="text-zinc-500">Loading…</p>}

      {state.kind === "unavailable" && (
        <p role="alert" className="text-red-400">
          Cloud is unavailable right now. The rest of the ecosystem still works — try this page
          again in a moment.
        </p>
      )}

      {state.kind === "ready" && state.routes.length === 0 && (
        <p className="text-zinc-400">No routes found.</p>
      )}

      {state.kind === "ready" && state.routes.length > 0 && (
        <div className="space-y-6">
          {groupRoutes(state.routes).map(([group, routes]) => (
            <div key={group} className="rounded-lg border border-zinc-800 bg-zinc-900 p-4">
              <h2 className="font-mono text-sm font-medium text-zinc-200">{group}</h2>
              <div className="mt-3 divide-y divide-zinc-800">
                {routes.map((r, i) => (
                  // eslint-disable-next-line react/no-array-index-key -- routes carry no stable id
                  <div key={`${r.method}-${r.path}-${i}`} className="flex flex-wrap items-center gap-3 py-2">
                    <span
                      className={`w-16 shrink-0 rounded px-1.5 py-0.5 text-center text-xs font-medium ${
                        METHOD_CLASS[r.method] ?? "bg-zinc-800 text-zinc-400"
                      }`}
                    >
                      {r.method}
                    </span>
                    <span className="font-mono text-sm text-zinc-100">{r.path}</span>
                    <span className="text-sm text-zinc-500">{r.description || ""}</span>
                  </div>
                ))}
              </div>
            </div>
          ))}
        </div>
      )}
    </section>
  );
}
