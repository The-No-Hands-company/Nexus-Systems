import { useEffect, useState } from "react";
import { cloudFederationPeers, type CloudPeer } from "../../api";
import { timeAgo } from "./format";
import CloudNav from "./CloudNav";

type State = { kind: "loading" } | { kind: "unavailable" } | { kind: "ready"; peers: CloudPeer[] };

/**
 * Renders `trustLevel`, falling back to `trust` — status.html's
 * `esc(p.trustLevel || p.trust)` coerced whatever it got with `String(...)`,
 * so a peer without `trustLevel` (every real one right now — see the
 * `CloudPeer` doc comment in api.ts) showed literally "[object Object]"
 * rather than crashing. React does not coerce object children the way
 * innerHTML did; it throws. This reproduces the same on-screen text without
 * that crash.
 */
function trustDisplay(peer: CloudPeer): string {
  if (typeof peer.trustLevel === "string" && peer.trustLevel) return peer.trustLevel;
  if (peer.trust === undefined || peer.trust === null) return "—";
  return typeof peer.trust === "string" ? peer.trust : String(peer.trust);
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

      {state.kind === "loading" && <p className="text-zinc-500">Loading…</p>}

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
                <th className="px-4 py-3 font-medium">Domain</th>
                <th className="px-4 py-3 font-medium">Trust</th>
                <th className="px-4 py-3 font-medium">Last seen</th>
                <th className="px-4 py-3 font-medium">Address</th>
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
