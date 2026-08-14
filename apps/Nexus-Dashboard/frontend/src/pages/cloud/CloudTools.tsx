import { useEffect, useState } from "react";
import { cloudTools, type CloudTool } from "../../api";
import { timeAgo, healthDotClass } from "./format";
import CloudNav from "./CloudNav";

type State = { kind: "loading" } | { kind: "unavailable" } | { kind: "ready"; tools: CloudTool[] };

/**
 * The ported `view-tools` from Cloud's status.html (loadToolsView): every
 * tool registered with this node, its capabilities, health and last
 * heartbeat.
 */
export default function CloudTools() {
  const [state, setState] = useState<State>({ kind: "loading" });

  useEffect(() => {
    let cancelled = false;
    void cloudTools()
      .then((tools) => { if (!cancelled) setState({ kind: "ready", tools }); })
      .catch(() => { if (!cancelled) setState({ kind: "unavailable" }); });
    return () => { cancelled = true; };
  }, []);

  return (
    <section className="mx-auto max-w-5xl space-y-6 p-8">
      <CloudNav />
      <div>
        <h1 className="text-2xl font-semibold">Cloud tools</h1>
        <p className="mt-2 text-zinc-400">Every tool registered with this node.</p>
      </div>

      {state.kind === "loading" && <p className="text-zinc-500">Loading…</p>}

      {state.kind === "unavailable" && (
        <p role="alert" className="text-red-400">
          Cloud is unavailable right now. The rest of the ecosystem still works — try this page
          again in a moment.
        </p>
      )}

      {state.kind === "ready" && state.tools.length === 0 && (
        <p className="text-zinc-400">No tools registered.</p>
      )}

      {state.kind === "ready" && state.tools.length > 0 && (
        <div className="overflow-x-auto rounded-lg border border-zinc-800 bg-zinc-900">
          <table className="w-full text-left text-sm">
            <thead>
              <tr className="border-b border-zinc-800 text-xs uppercase tracking-wide text-zinc-500">
                <th className="px-4 py-3 font-medium">Name</th>
                <th className="px-4 py-3 font-medium">Capabilities</th>
                <th className="px-4 py-3 font-medium">Status</th>
                <th className="px-4 py-3 font-medium">Address</th>
                <th className="px-4 py-3 font-medium">Last seen</th>
                <th className="px-4 py-3 font-medium" />
              </tr>
            </thead>
            <tbody className="divide-y divide-zinc-800">
              {state.tools.map((t) => {
                const health = t.health || t.status;
                const url = t.publicUrl || t.upstreamUrl;
                return (
                  <tr key={t.id ?? t.name}>
                    <td className="px-4 py-3 font-medium text-zinc-100">{t.name}</td>
                    <td className="px-4 py-3">
                      {(t.capabilities ?? []).length > 0 ? (
                        <div className="flex flex-wrap gap-1.5">
                          {(t.capabilities ?? []).map((c) => (
                            <span key={c} className="rounded bg-zinc-800 px-1.5 py-0.5 text-xs text-zinc-400">
                              {c}
                            </span>
                          ))}
                        </div>
                      ) : (
                        <span className="rounded bg-zinc-800 px-1.5 py-0.5 text-xs text-zinc-500">none</span>
                      )}
                    </td>
                    <td className="px-4 py-3">
                      <span className="flex items-center gap-1.5 text-zinc-300">
                        <span className={`h-1.5 w-1.5 rounded-full ${healthDotClass(health)}`} />
                        {health || "unknown"}
                      </span>
                    </td>
                    <td className="px-4 py-3 font-mono text-xs text-zinc-400">{url || "—"}</td>
                    <td className="px-4 py-3 text-zinc-400">
                      {t.lastHeartbeatAt ? timeAgo(t.lastHeartbeatAt) : "—"}
                    </td>
                    <td className="px-4 py-3">
                      {url && (
                        <a
                          href={url}
                          target="_blank"
                          rel="noreferrer"
                          className="rounded border border-zinc-700 px-2 py-1 text-xs hover:bg-zinc-800"
                        >
                          Open ↗
                        </a>
                      )}
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>
      )}
    </section>
  );
}
