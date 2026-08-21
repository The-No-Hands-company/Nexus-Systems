import { useEffect, useState } from "react";
import { cloudFederationPeers, type CloudPeer } from "../../api";
import { timeAgo } from "./format";
import CloudNav from "./CloudNav";

type State = { kind: "loading" } | { kind: "unavailable" } | { kind: "ready"; peers: CloudPeer[] };

/**
 * Renders `trustLevel`, falling back to `trust`.
 *
 * status.html did `esc(p.trustLevel || p.trust)`, which coerced an object
 * `trust` to the literal text "[object Object]". Faithfulness to the original
 * stops short of reproducing that: it tells the operator nothing and reads as
 * a broken page. React would throw on an object child anyway, so some handling
 * is required — this picks the field a trust object actually carries, and
 * falls back to the em-dash used for "nothing to show" everywhere else.
 */
function trustDisplay(peer: CloudPeer): string {
  if (typeof peer.trustLevel === "string" && peer.trustLevel) return peer.trustLevel;
  const trust: unknown = peer.trust;
  if (trust === undefined || trust === null) return "—";
  if (typeof trust === "string") return trust;
  if (typeof trust === "number" || typeof trust === "boolean") return String(trust);
  if (typeof trust === "object") {
    const t = trust as Record<string, unknown>;
    for (const key of ["level", "trustLevel", "status", "score"]) {
      const v = t[key];
      if (typeof v === "string" && v) return v;
      if (typeof v === "number") return String(v);
    }
  }
  return "—";
}

/**
 * The ported `view-federation` from Cloud's status.html (loadFederationView):
 * every peer this node has discovered.
 */
export default function CloudFederation() {
  const [state, setState] = useState<State>({ kind: "loading" });

  useEffect(() => {
    let cancelled = false;
    void cloudFederationPeers()
      .then((peers) => { if (!cancelled) setState({ kind: "ready", peers }); })
      .catch(() => { if (!cancelled) setState({ kind: "unavailable" }); });
    return () => { cancelled = true; };
  }, []);

  return (
    <section className="mx-auto max-w-5xl space-y-6 p-8">
      <CloudNav />
      <div>
        <h1 className="text-2xl font-semibold">Federation</h1>
        <p className="mt-2 text-zinc-400">Peers this node has discovered.</p>
      </div>

      {state.kind === "loading" && <p role="status" className="text-zinc-500">Loading…</p>}

      {state.kind === "unavailable" && (
        <p role="alert" className="text-red-400">
          Cloud is unavailable right now. The rest of the ecosystem still works — try this page
          again in a moment.
        </p>
      )}

      {state.kind === "ready" && state.peers.length === 0 && (
        <div className="rounded-lg border border-zinc-800 bg-zinc-900 p-6 text-center">
          <p className="text-zinc-300">No federation peers discovered yet.</p>
          <p className="mt-2 text-sm text-zinc-500">
            Add BOOTSTRAP_PEERS to your .env to connect to other Nexus nodes.
          </p>
        </div>
      )}

      {state.kind === "ready" && state.peers.length > 0 && (
        <div className="overflow-x-auto rounded-lg border border-zinc-800 bg-zinc-900">
          <table className="w-full text-left text-sm">
            <thead>
              <tr className="border-b border-zinc-800 text-xs uppercase tracking-wide text-zinc-500">
                <th scope="col" className="px-4 py-3 font-medium">Domain</th>
                <th scope="col" className="px-4 py-3 font-medium">Trust</th>
                <th scope="col" className="px-4 py-3 font-medium">Last seen</th>
                <th scope="col" className="px-4 py-3 font-medium">Address</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-zinc-800">
              {state.peers.map((p, i) => (
                // eslint-disable-next-line react/no-array-index-key -- peers may lack a stable id
                <tr key={p.domain ?? p.url ?? i}>
                  <td className="px-4 py-3 font-mono text-xs text-zinc-100">{p.domain || p.url || "—"}</td>
                  <td className="px-4 py-3 text-zinc-300">{trustDisplay(p)}</td>
                  <td className="px-4 py-3 text-zinc-400">{p.lastSeen ? timeAgo(p.lastSeen) : "—"}</td>
                  <td className="px-4 py-3 font-mono text-xs text-zinc-400">
                    {p.address || p.nodeAddress || "—"}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </section>
  );
}
